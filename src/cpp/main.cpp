#include "bridge.hpp"
#include "json.hpp"
#include "tools.hpp"

#include <cstdlib>
#include <filesystem>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ServerConfig {
    bool only_plus = false;
    std::string real_mcp_path;
};

std::string resolve_real_mcp_path(const ServerConfig& cfg, const char* argv0) {
    if (!cfg.real_mcp_path.empty()) return cfg.real_mcp_path;
    if (const char* env = std::getenv("REAL_MCP_PATH")) {
        if (*env && fs::exists(env)) return std::string(env);
    }
    if (argv0 && *argv0) {
        fs::path sibling = fs::path(argv0).parent_path() / "real-mcp.exe";
        if (fs::exists(sibling)) return sibling.string();
    }
    if (const char* local_app = std::getenv("LOCALAPPDATA")) {
        fs::path p1 = fs::path(local_app) / "Real" / "data" / "mcp" / "real-mcp.exe";
        if (fs::exists(p1)) return p1.string();
        fs::path p2 = fs::path(local_app) / "Programs" / "Real" / "real-mcp.exe";
        if (fs::exists(p2)) return p2.string();
    }
    if (const char* app_data = std::getenv("APPDATA")) {
        fs::path p3 = fs::path(app_data) / "Real" / "real-mcp.exe";
        if (fs::exists(p3)) return p3.string();
    }
    if (const char* prog_files = std::getenv("ProgramFiles")) {
        fs::path p4 = fs::path(prog_files) / "Real" / "real-mcp.exe";
        if (fs::exists(p4)) return p4.string();
    }
    return "real-mcp.exe";
}

class BridgeManager {
public:
    explicit BridgeManager(std::string exe_path) : exe_path_(std::move(exe_path)) {}

    realplus::RealBridge& get(const Json& init_params) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (bridge_ && bridge_->is_alive()) {
            return *bridge_;
        }
        bridge_ = std::make_unique<realplus::RealBridge>(exe_path_);
        Json p = init_params.is_object() ? init_params : Json::object({
            {"protocolVersion", Json("2024-11-05")},
            {"capabilities", Json::object()},
            {"clientInfo", Json::object({{"name", Json("real-mcp-plus")}, {"version", Json("2.0.0")}})}
        });
        bridge_->request("initialize", p);
        bridge_->notify("notifications/initialized", Json::object());
        return *bridge_;
    }

    void try_eager_init(const Json& init_params) {
        try {
            get(init_params);
        } catch (...) {
            // Allow server startup in --only-plus or offline testing even if real-mcp.exe is absent
        }
    }

private:
    std::string exe_path_;
    std::mutex mtx_;
    std::unique_ptr<realplus::RealBridge> bridge_;
};

std::unordered_set<std::string> build_plus_name_set() {
    std::unordered_set<std::string> names;
    Json tools = realplus::list_plus_tools();
    if (tools.is_array()) {
        for (const auto& t : tools.as_array()) {
            if (t.is_object() && t.contains("name")) {
                names.insert(t.get("name").as_string(""));
            }
        }
    }
    return names;
}

} // namespace

int main(int argc, char** argv) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    ServerConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--only-plus") {
            cfg.only_plus = true;
        } else if (arg == "--real-mcp" && i + 1 < argc) {
            cfg.real_mcp_path = argv[++i];
        }
    }

    std::string real_exe = resolve_real_mcp_path(cfg, argc > 0 ? argv[0] : nullptr);
    BridgeManager bridge_mgr(real_exe);
    const auto plus_names = build_plus_name_set();

    std::mutex out_mtx;
    auto send_json = [&](const Json& msg) {
        std::string line = msg.dump();
        std::lock_guard<std::mutex> lock(out_mtx);
        std::cout.write(line.data(), static_cast<std::streamsize>(line.size()));
        std::cout.put('\n');
        std::cout.flush();
    };

    auto send_result = [&](const Json& id, const Json& result) {
        send_json(Json::object({
            {"jsonrpc", Json("2.0")},
            {"id", id},
            {"result", result}
        }));
    };

    auto send_error = [&](const Json& id, int64_t code, const std::string& message) {
        send_json(Json::object({
            {"jsonrpc", Json("2.0")},
            {"id", id},
            {"error", Json::object({{"code", Json(code)}, {"message", Json(message)}})}
        }));
    };

    Json saved_init_params = Json::object();
    std::vector<std::thread> workers;
    std::mutex workers_mtx;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        Json req;
        if (!Json::try_parse(line, req) || !req.is_object()) {
            send_error(Json::null(), -32700, "Parse error");
            continue;
        }

        std::string method = req.get("method").as_string("");
        bool has_id = req.contains("id") && !req.get("id").is_null();
        Json id = has_id ? req.get("id") : Json::null();
        Json params = req.contains("params") ? req.get("params") : Json::object();

        if (method == "initialize") {
            saved_init_params = params;
            if (!cfg.only_plus) {
                bridge_mgr.try_eager_init(saved_init_params);
            }
            std::string proto = params.get("protocolVersion").as_string("2024-11-05");
            send_result(id, Json::object({
                {"protocolVersion", Json(proto)},
                {"capabilities", Json::object({
                    {"tools", Json::object({{"listChanged", Json(false)}})}
                })},
                {"serverInfo", Json::object({
                    {"name", Json("real-mcp-plus")},
                    {"version", Json("1.0.5")}
                })}
            }));
            continue;
        }

        if (method == "notifications/initialized" || method == "notifications/cancelled") {
            continue;
        }

        if (!has_id) {
            continue;
        }

        if (method == "ping") {
            send_result(id, Json::object());
            continue;
        }

        if (method == "tools/list") {
            Json tools = realplus::list_plus_tools();
            if (!cfg.only_plus) {
                try {
                    auto& real = bridge_mgr.get(saved_init_params);
                    Json up = real.request("tools/list", Json::object());
                    if (up.is_object() && up.get("tools").is_array()) {
                        for (const auto& t : up.get("tools").as_array()) {
                            std::string tname = t.get("name").as_string("");
                            if (!plus_names.count(tname)) {
                                tools.as_array().push_back(t);
                            }
                        }
                    }
                } catch (...) {
                    // Return plus tools even if upstream real-mcp is unavailable
                }
            }
            send_result(id, Json::object({{"tools", std::move(tools)}}));
            continue;
        }

        if (method == "tools/call") {
            {
                std::lock_guard<std::mutex> lock(workers_mtx);
                auto it = workers.begin();
                while (it != workers.end()) {
                    if (it->joinable()) {
                        it->join();
                        it = workers.erase(it);
                    } else {
                        ++it;
                    }
                }
                workers.emplace_back([&, id, params]() {
                    std::string tname = params.get("name").as_string("");
                    Json targs = params.get("arguments").is_object() ? params.get("arguments") : Json::object();

                    try {
                        if (plus_names.count(tname)) {
                            if (tname == "web-scripts" && !targs.get("useCurrentGame").as_bool(false) &&
                                !targs.get("query").as_string("").empty()) {
                                send_result(id, realplus::run_web_scripts_opt(nullptr, targs));
                            } else if (tname == "rbxgame-search") {
                                send_result(id, realplus::run_rbxgame_search_opt(nullptr, targs));
                            } else if (tname == "simpledeobf") {
                                send_result(id, realplus::run_simpledeobf_opt(nullptr, targs));
                            } else if (tname == "proto-unpack") {
                                send_result(id, realplus::run_proto_unpack_opt(nullptr, targs));
                            } else {
                                auto& real = bridge_mgr.get(saved_init_params);
                                send_result(id, realplus::dispatch_plus_tool(real, tname, targs));
                            }
                        } else if (!cfg.only_plus) {
                            auto& real = bridge_mgr.get(saved_init_params);
                            send_result(id, real.call_tool(tname, targs));
                        } else {
                            send_result(id, realplus::tool_error("Unknown tool in --only-plus mode: " + tname));
                        }
                    } catch (const std::exception& e) {
                        send_result(id, realplus::tool_error(e.what()));
                    }
                });
            }
            continue;
        }

        send_error(id, -32601, "Method not found: " + method);
    }

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
    return 0;
}
