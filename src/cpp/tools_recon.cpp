#include "tools.hpp"
#include "payloads_gen.hpp"
#include <vector>

namespace realplus {

static inline const char* bstr(bool v) { return v ? "true" : "false"; }

Json run_scac(RealBridge& real, const Json& args) {
    bool inc_conn = args.get("includeConnections").as_bool(true);
    bool chk_rem = args.get("checkRemotes").as_bool(true);
    bool inc_builtin = args.get("includeBuiltinScan").as_bool(true);

    std::string code = payloads::LUAU_SCAC;
    code = replace_all(std::move(code), "__INCLUDE_CONNECTIONS__", bstr(inc_conn));
    code = replace_all(std::move(code), "__CHECK_REMOTES__", bstr(chk_rem));

    Json report = Json::object({
        {"luauScan", eval_luau_safe(real, "scac", code)}
    });

    if (inc_builtin) {
        report.as_object()["anticheatHooks"] = call_optional(real, "dump-anticheat-hooks", Json::object());
        report.as_object()["specialInstances"] = call_optional(real, "list-special-instances", Json::object());
    }
    return tool_text_response(report);
}

Json run_uiscan(RealBridge& real, const Json& args) {
    bool inc_nil = args.get("includeNil").as_bool(true);
    bool inc_core = args.get("includeCoreGui").as_bool(false);
    bool only_susp = args.get("onlySuspicious").as_bool(false);
    bool chk_conn = args.get("checkConnections").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(150);

    std::string code = payloads::LUAU_UISCAN;
    code = replace_all(std::move(code), "__INCLUDE_NIL__", bstr(inc_nil));
    code = replace_all(std::move(code), "__INCLUDE_COREGUI__", bstr(inc_core));
    code = replace_all(std::move(code), "__ONLY_SUSPICIOUS__", bstr(only_susp));
    code = replace_all(std::move(code), "__CHECK_CONNECTIONS__", bstr(chk_conn));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    Json report = Json::object({
        {"uiScan", eval_luau_safe(real, "uiscan", code)},
        {"visibleUi", call_optional(real, "dump-visible-ui", Json::object())}
    });
    return tool_text_response(report);
}

Json run_httpscan(RealBridge& real, const Json& args) {
    bool scan_inst = args.get("scanInstances").as_bool(true);
    bool hook_req = args.get("hookRequests").as_bool(false);
    bool grep_scripts = args.get("grepScripts").as_bool(true);

    std::string code = payloads::LUAU_HTTPSCAN;
    code = replace_all(std::move(code), "__SCAN_INSTANCES__", bstr(scan_inst));
    code = replace_all(std::move(code), "__HOOK_REQUESTS__", bstr(hook_req));

    Json report = Json::object({
        {"runtimeScan", eval_luau_safe(real, "httpscan", code)}
    });
    if (grep_scripts) {
        report.as_object()["scriptMatches"] = grep_batch(real, {
            "https://", "http://", "discord.com/api/webhooks",
            "RequestAsync", "HttpGet", "PostAsync", "HttpService"
        }, 20);
    }
    return tool_text_response(report);
}

Json run_keyscan(RealBridge& real, const Json& args) {
    bool inc_gc = args.get("includeGc").as_bool(true);
    int max_matches = static_cast<int>(args.get("maxMatches").as_int(20));

    Json report = Json::object({
        {"scriptMatches", grep_batch(real, {
            "Key", "Whitelist", "HWID", "GetClientId", "RbxAnalyticsService",
            "linkvertise", "lootlabs", "work.ink", "pastebin", "discord.gg"
        }, max_matches)}
    });
    if (inc_gc) {
        report.as_object()["gcFunctions"] = gc_const_batch(real, {
            "Key", "Whitelist", "HWID", "Valid", "Expired"
        }, 15);
        report.as_object()["gcTables"] = gc_table_batch(real, {
            "Key", "Whitelist", "HWID"
        }, 15);
    }
    return tool_text_response(report);
}

Json run_recon(RealBridge& real, const Json& args) {
    std::string depth = args.get("depth").as_string("quick");
    bool remember = args.get("remember").as_bool(true);
    bool full = (depth == "full");

    Json game_info = call_optional(real, "get-game-info", Json::object());
    Json rem_res = parse_tool_json_or_raw(run_remmap(real, Json::object({{"maxItems", Json(full ? 250 : 120)}})));
    Json scac_res = parse_tool_json_or_raw(run_scac(real, Json::object({{"includeBuiltinScan", Json(full)}})));
    Json prox_res = parse_tool_json_or_raw(run_proxscan(real, Json::object({{"maxItems", Json(full ? 120 : 60)}})));
    Json econ_res = parse_tool_json_or_raw(run_econscan(real, Json::object({{"includeGrep", Json(full)}, {"maxItems", Json(80)}})));
    Json ui_res = parse_tool_json_or_raw(run_uiscan(real, Json::object({{"onlySuspicious", Json(true)}, {"maxItems", Json(60)}})));
    Json cli_res = parse_tool_json_or_raw(run_clicheck(real, Json::object({{"includeGc", Json(full)}, {"includeGrep", Json(full)}, {"maxItems", Json(60)}})));

    Json report = Json::object({
        {"depth", Json(depth)},
        {"gameInfo", game_info},
        {"remotes", rem_res},
        {"antiCheat", scac_res},
        {"proximityAndInteractables", prox_res},
        {"economy", econ_res},
        {"suspiciousUi", ui_res},
        {"clientChecks", cli_res}
    });

    if (full) {
        report.as_object()["npcs"] = parse_tool_json_or_raw(run_npcscan(real, Json::object({{"maxItems", Json(50)}})));
        report.as_object()["zones"] = parse_tool_json_or_raw(run_zonescan(real, Json::object({{"maxItems", Json(60)}})));
        report.as_object()["tools"] = parse_tool_json_or_raw(run_toolscan(real, Json::object({{"maxItems", Json(60)}})));
    }

    if (remember) {
        std::string summary = "Recon (" + depth + ") completed via real-plus.";
        report.as_object()["memorySaved"] = call_optional(real, "remember-game", Json::object({
            {"text", Json(summary)},
            {"tags", Json::array({Json("recon")})}
        }));
    }

    return tool_text_response(report);
}

} // namespace realplus
