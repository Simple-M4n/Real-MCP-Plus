#include "bridge.hpp"
#include "../c/win_bridge.h"
#include <chrono>
#include <stdexcept>
#include <utility>

namespace realplus {

RealBridge::RealBridge(const std::string& exe_path) {
    proc_ = win_proc_spawn(exe_path.c_str());
    if (!proc_) {
        throw std::runtime_error("Failed to spawn real-mcp process: " + exe_path);
    }
    alive_.store(true);
    reader_thread_ = std::thread(&RealBridge::reader_loop, this);
}

RealBridge::~RealBridge() {
    alive_.store(false);
    if (proc_) {
        win_proc_close(proc_);
        proc_ = nullptr;
    }
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
    fail_all_pending("RealBridge closed");
}

void RealBridge::fail_all_pending(const std::string& reason) {
    std::lock_guard<std::mutex> lock(pending_mtx_);
    for (auto& kv : pending_) {
        try {
            kv.second.set_exception(std::make_exception_ptr(std::runtime_error(reason)));
        } catch (...) {}
    }
    pending_.clear();
}

void RealBridge::reader_loop() {
    while (alive_.load()) {
        char* line = nullptr;
        size_t len = 0;
        int rc = win_proc_read_line(proc_, &line, &len);
        if (rc <= 0) {
            if (line) win_free(line);
            break;
        }
        std::string s(line, len);
        win_free(line);
        if (s.empty()) continue;

        Json msg;
        if (!Json::try_parse(s, msg) || !msg.is_object()) continue;

        if (msg.contains("id") && !msg.get("id").is_null()) {
            int64_t id = msg.get("id").as_int(-1);
            std::promise<Json> prom;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(pending_mtx_);
                auto it = pending_.find(id);
                if (it != pending_.end()) {
                    prom = std::move(it->second);
                    pending_.erase(it);
                    found = true;
                }
            }
            if (found) {
                if (msg.contains("error") && !msg.get("error").is_null()) {
                    const Json& err = msg.get("error");
                    std::string emsg = err.is_object() && err.contains("message")
                        ? err.get("message").as_string_lossy()
                        : err.dump();
                    prom.set_exception(std::make_exception_ptr(std::runtime_error("Upstream RPC error: " + emsg)));
                } else {
                    prom.set_value(msg.contains("result") ? msg.get("result") : Json::object());
                }
            }
        }
    }
    alive_.store(false);
    fail_all_pending("real-mcp process exited");
}

Json RealBridge::request(const std::string& method, const Json& params) {
    if (!alive_.load()) {
        throw std::runtime_error("real-mcp process is not running");
    }
    int64_t id = next_id_.fetch_add(1);
    std::promise<Json> prom;
    auto fut = prom.get_future();
    {
        std::lock_guard<std::mutex> lock(pending_mtx_);
        pending_.emplace(id, std::move(prom));
    }

    Json payload = Json::object({
        {"jsonrpc", Json("2.0")},
        {"id", Json(id)},
        {"method", Json(method)},
        {"params", params.is_null() ? Json::object() : params}
    });
    std::string line = payload.dump();
    {
        std::lock_guard<std::mutex> lock(write_mtx_);
        if (win_proc_write_line(proc_, line.data(), line.size()) != 0) {
            std::lock_guard<std::mutex> plock(pending_mtx_);
            pending_.erase(id);
            throw std::runtime_error("Failed to write request to real-mcp stdin");
        }
    }

    if (fut.wait_for(std::chrono::seconds(90)) != std::future_status::ready) {
        std::lock_guard<std::mutex> plock(pending_mtx_);
        pending_.erase(id);
        throw std::runtime_error("Timeout waiting for real-mcp method: " + method);
    }
    return fut.get();
}

void RealBridge::notify(const std::string& method, const Json& params) {
    if (!alive_.load()) return;
    Json payload = Json::object({
        {"jsonrpc", Json("2.0")},
        {"method", Json(method)},
        {"params", params.is_null() ? Json::object() : params}
    });
    std::string line = payload.dump();
    std::lock_guard<std::mutex> lock(write_mtx_);
    win_proc_write_line(proc_, line.data(), line.size());
}

Json RealBridge::call_tool(const std::string& name, const Json& arguments) {
    return request("tools/call", Json::object({
        {"name", Json(name)},
        {"arguments", arguments.is_object() ? arguments : Json::object()}
    }));
}

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string luau_str(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\%03u", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
    return out;
}

std::string extract_tool_text(const Json& res) {
    if (!res.is_object()) return res.as_string_lossy();
    const Json& content = res.get("content");
    if (content.is_array()) {
        std::string out;
        for (const auto& item : content.as_array()) {
            if (item.is_object() && item.contains("text")) {
                if (!out.empty()) out.push_back('\n');
                out += item.get("text").as_string_lossy();
            }
        }
        if (!out.empty()) return out;
    }
    return res.dump();
}

Json parse_tool_json_or_raw(const Json& res) {
    std::string text = extract_tool_text(res);
    Json parsed;
    if (Json::try_parse(text, parsed)) {
        return parsed;
    }
    return Json::object({{"raw", Json(text)}});
}

Json eval_luau_safe(RealBridge& real, const std::string& label, const std::string& code) {
    try {
        Json res = real.call_tool("eval", Json::object({{"code", Json(code)}}));
        if (res.is_object() && res.get("isError").as_bool(false)) {
            return Json::object({{"error", Json(extract_tool_text(res))}, {"step", Json(label)}});
        }
        return parse_tool_json_or_raw(res);
    } catch (const std::exception& e) {
        return Json::object({{"error", Json(e.what())}, {"step", Json(label)}});
    }
}

Json call_optional(RealBridge& real, const std::string& tool, const Json& args) {
    try {
        Json res = real.call_tool(tool, args);
        if (res.is_object() && res.get("isError").as_bool(false)) {
            return Json::object({{"error", Json(extract_tool_text(res))}});
        }
        return parse_tool_json_or_raw(res);
    } catch (const std::exception& e) {
        return Json::object({{"error", Json(e.what())}});
    }
}

Json grep_batch(RealBridge& real, const std::vector<std::string>& patterns, int max_matches) {
    Json obj = Json::object();
    for (const auto& pat : patterns) {
        obj.as_object()[pat] = call_optional(real, "script-grep", Json::object({
            {"query", Json(pat)},
            {"caseSensitive", Json(false)},
            {"maxResults", Json(static_cast<int64_t>(max_matches))}
        }));
    }
    return obj;
}

Json gc_const_batch(RealBridge& real, const std::vector<std::string>& constants, int max_results) {
    Json obj = Json::object();
    for (const auto& c : constants) {
        obj.as_object()[c] = call_optional(real, "gc-scan", Json::object({
            {"kind", Json("function")},
            {"constants", Json::array({Json(c)})},
            {"limit", Json(static_cast<int64_t>(max_results))}
        }));
    }
    return obj;
}

Json gc_table_batch(RealBridge& real, const std::vector<std::string>& keys, int max_results) {
    Json obj = Json::object();
    for (const auto& k : keys) {
        obj.as_object()[k] = call_optional(real, "gc-scan", Json::object({
            {"kind", Json("table")},
            {"keys", Json::array({Json(k)})},
            {"limit", Json(static_cast<int64_t>(max_results))}
        }));
    }
    return obj;
}

Json tool_text_response(const Json& val, bool is_error) {
    std::string text = val.is_string() ? val.as_string() : val.dump(2);
    Json res = Json::object({
        {"content", Json::array({Json::object({
            {"type", Json("text")},
            {"text", Json(text)}
        })})}
    });
    if (is_error) {
        res.as_object()["isError"] = Json(true);
    }
    return res;
}

Json tool_error(const std::string& msg) {
    return tool_text_response(Json(msg), true);
}

} // namespace realplus
