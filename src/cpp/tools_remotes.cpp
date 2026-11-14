#include "tools.hpp"
#include "payloads_gen.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <vector>

namespace realplus {

static inline const char* bstr(bool v) { return v ? "true" : "false"; }

static std::vector<std::string> tokenize_ident(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (!std::isalnum(c)) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (std::isupper(c) && !cur.empty() && std::islower(static_cast<unsigned char>(cur.back()))) {
            out.push_back(cur);
            cur.clear();
        }
        cur.push_back(static_cast<char>(std::tolower(c)));
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static bool is_roblox_internal_path(const std::string& path) {
    return path.find("RobloxReplicatedStorage") != std::string::npos ||
           path.find("DefaultChatSystemChatEvents") != std::string::npos ||
           path.find("ExperienceChat") != std::string::npos ||
           path.find("CorePackages") != std::string::npos;
}

static Json classify_remote_flags(const std::string& name) {
    static const std::vector<std::pair<std::string, std::unordered_set<std::string>>> cats = {
        {"admin", {"admin", "mod", "staff", "command", "cmd", "exec", "owner", "dev", "debug"}},
        {"economy", {"buy", "purchase", "shop", "sell", "trade", "gift", "claim", "reward", "cash", "coin", "gem", "money", "currency"}},
        {"combat", {"damage", "hit", "kill", "attack", "shoot", "fire", "hurt", "slash", "punch", "heal"}},
        {"movement", {"tp", "teleport", "warp", "spawn", "dash", "fly", "speed"}},
        {"anticheat", {"ban", "kick", "flag", "detect", "anticheat", "ac", "report", "log", "check", "verify"}},
        {"data", {"save", "load", "data", "set", "update", "sync"}}
    };

    auto tokens = tokenize_ident(name);
    Json flags = Json::array();
    for (const auto& cat : cats) {
        for (const auto& tok : tokens) {
            if (cat.second.count(tok)) {
                flags.as_array().push_back(Json(cat.first));
                break;
            }
        }
    }
    return flags;
}

Json run_remmap(RealBridge& real, const Json& args) {
    bool inc_nil = args.get("includeNil").as_bool(true);
    bool inc_bind = args.get("includeBindables").as_bool(false);
    std::string filter = args.get("filter").as_string("");
    int64_t max_items = args.get("maxItems").as_int(200);

    std::string code = payloads::LUAU_REMMAP;
    code = replace_all(std::move(code), "__INCLUDE_NIL__", bstr(inc_nil));
    code = replace_all(std::move(code), "__INCLUDE_BINDABLES__", bstr(inc_bind));
    code = replace_all(std::move(code), "__FILTER__", luau_str(filter));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    Json raw = eval_luau_safe(real, "remmap", code);
    if (!raw.is_object() || !raw.get("remotes").is_array()) {
        return tool_text_response(raw);
    }

    Json kept = Json::array();
    int64_t skipped_internal = 0;
    for (const auto& item : raw.get("remotes").as_array()) {
        if (!item.is_object()) continue;
        std::string path = item.get("path").as_string("");
        std::string rname = item.get("name").as_string("");
        if (is_roblox_internal_path(path)) {
            ++skipped_internal;
            continue;
        }
        Json enriched = item;
        enriched.as_object()["riskFlags"] = classify_remote_flags(rname);
        kept.as_array().push_back(std::move(enriched));
    }

    Json out = raw;
    out.as_object()["remotes"] = std::move(kept);
    out.as_object()["skippedRobloxInternal"] = Json(skipped_internal);
    return tool_text_response(out);
}

Json run_remtrace(RealBridge& real, const Json& args) {
    std::string remote_name = args.get("remoteName").as_string("");
    if (remote_name.empty()) {
        return tool_error("remoteName is required");
    }
    std::string remote_path = args.get("remotePath").as_string("");
    double spy_sec = args.get("spyDurationSec").as_double(0.0);
    int64_t max_callers = args.get("maxCallers").as_int(15);

    Json callers = call_optional(real, "find-remote-callers", Json::object({
        {"remoteName", Json(remote_name)},
        {"maxResults", Json(max_callers)}
    }));
    Json grep = call_optional(real, "script-grep", Json::object({
        {"query", Json(remote_name)},
        {"caseSensitive", Json(false)},
        {"maxResults", Json(max_callers)}
    }));

    Json report = Json::object({
        {"remoteName", Json(remote_name)},
        {"remotePath", Json(remote_path)},
        {"callers", callers},
        {"scriptGrep", grep}
    });

    if (spy_sec > 0.0) {
        if (spy_sec > 15.0) spy_sec = 15.0;
        Json start_res = call_optional(real, "remote-spy", Json::object({
            {"operation", Json("list")},
            {"nameFilter", Json(remote_name)},
            {"limit", Json(20)}
        }));
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(spy_sec * 1000.0)));
        Json list_res = call_optional(real, "remote-spy", Json::object({
            {"operation", Json("list")},
            {"nameFilter", Json(remote_name)},
            {"limit", Json(40)}
        }));
        call_optional(real, "remote-spy", Json::object({{"operation", Json("stop")}}));
        report.as_object()["liveSpyInit"] = start_res;
        report.as_object()["liveSpyCaptured"] = list_res;
    }

    return tool_text_response(report);
}

Json run_remotesim(RealBridge& real, const Json& args) {
    std::string path = args.get("remotePath").as_string("");
    if (path.empty()) {
        return tool_error("remotePath is required");
    }
    std::string method = args.get("method").as_string("FireServer");
    Json call_args = args.get("args").is_array() ? args.get("args") : Json::array();
    bool cap_inbound = args.get("captureInbound").as_bool(true);
    double cap_sec = args.get("captureSec").as_double(1.5);
    if (cap_sec < 0.1) cap_sec = 0.1;
    if (cap_sec > 10.0) cap_sec = 10.0;

    Json inbound_start = Json::null();
    if (cap_inbound) {
        inbound_start = call_optional(real, "inbound-spy", Json::object({
            {"operation", Json("start")},
            {"path", Json(path)}
        }));
    }

    Json fire_res = call_optional(real, "fire-remote", Json::object({
        {"path", Json(path)},
        {"method", Json(method)},
        {"args", call_args}
    }));

    Json inbound_list = Json::null();
    if (cap_inbound) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(cap_sec * 1000.0)));
        inbound_list = call_optional(real, "inbound-spy", Json::object({
            {"operation", Json("list")},
            {"limit", Json(25)}
        }));
        call_optional(real, "inbound-spy", Json::object({{"operation", Json("stop")}}));
    }

    Json report = Json::object({
        {"remotePath", Json(path)},
        {"method", Json(method)},
        {"fireResult", fire_res}
    });
    if (cap_inbound) {
        report.as_object()["inboundStart"] = inbound_start;
        report.as_object()["inboundCaptured"] = inbound_list;
    }
    return tool_text_response(report);
}

Json run_clicheck(RealBridge& real, const Json& args) {
    bool inc_gc = args.get("includeGc").as_bool(true);
    bool inc_grep = args.get("includeGrep").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(150);

    std::string code = payloads::LUAU_CLICHECK;
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    Json report = Json::object({
        {"clientState", eval_luau_safe(real, "clicheck", code)}
    });
    if (inc_grep) {
        report.as_object()["scriptMatches"] = grep_batch(real, {
            "Cooldown", "Debounce", "WalkSpeed", "JumpPower", "Damage", "Range", "Magnitude"
        }, 15);
    }
    if (inc_gc) {
        report.as_object()["gcFunctions"] = gc_const_batch(real, {
            "Cooldown", "Damage", "WalkSpeed", "Range"
        }, 12);
    }
    return tool_text_response(report);
}

Json run_modscan(RealBridge& real, const Json& args) {
    std::string filter = args.get("filter").as_string("");
    bool req_mods = args.get("requireModules").as_bool(false);
    int64_t max_items = args.get("maxItems").as_int(120);

    std::string code = payloads::LUAU_MODSCAN;
    code = replace_all(std::move(code), "__FILTER__", luau_str(filter));
    code = replace_all(std::move(code), "__REQUIRE_MODULES__", bstr(req_mods));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    return tool_text_response(eval_luau_safe(real, "modscan", code));
}

Json run_econscan(RealBridge& real, const Json& args) {
    bool inc_grep = args.get("includeGrep").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(150);

    std::string code = payloads::LUAU_ECONSCAN;
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    Json report = Json::object({
        {"economyScan", eval_luau_safe(real, "econscan", code)}
    });
    if (inc_grep) {
        report.as_object()["scriptMatches"] = grep_batch(real, {
            "MarketplaceService", "PromptProductPurchase", "PromptGamePassPurchase", "UserOwnsGamePassAsync"
        }, 15);
    }
    return tool_text_response(report);
}

} // namespace realplus
