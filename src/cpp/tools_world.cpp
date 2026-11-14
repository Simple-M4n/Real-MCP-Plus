#include "tools.hpp"
#include "payloads_gen.hpp"

namespace realplus {

static inline const char* bstr(bool v) { return v ? "true" : "false"; }

Json run_proxscan(RealBridge& real, const Json& args) {
    bool prompts = args.get("includePrompts").as_bool(true);
    bool clicks = args.get("includeClicks").as_bool(true);
    bool touch = args.get("includeTouch").as_bool(true);
    bool seats = args.get("includeSeats").as_bool(false);
    bool conn = args.get("checkConnections").as_bool(true);
    bool sort_dist = args.get("sortByDistance").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(150);

    std::string code = payloads::LUAU_PROXSCAN;
    code = replace_all(std::move(code), "__INCLUDE_PROMPTS__", bstr(prompts));
    code = replace_all(std::move(code), "__INCLUDE_CLICKS__", bstr(clicks));
    code = replace_all(std::move(code), "__INCLUDE_TOUCH__", bstr(touch));
    code = replace_all(std::move(code), "__INCLUDE_SEATS__", bstr(seats));
    code = replace_all(std::move(code), "__CHECK_CONNECTIONS__", bstr(conn));
    code = replace_all(std::move(code), "__SORT_BY_DISTANCE__", bstr(sort_dist));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    return tool_text_response(eval_luau_safe(real, "proxscan", code));
}

Json run_npcscan(RealBridge& real, const Json& args) {
    bool alive = args.get("aliveOnly").as_bool(true);
    bool sort_dist = args.get("sortByDistance").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(100);

    std::string code = payloads::LUAU_NPCSCAN;
    code = replace_all(std::move(code), "__ALIVE_ONLY__", bstr(alive));
    code = replace_all(std::move(code), "__SORT_BY_DISTANCE__", bstr(sort_dist));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    return tool_text_response(eval_luau_safe(real, "npcscan", code));
}

Json run_zonescan(RealBridge& real, const Json& args) {
    int64_t max_items = args.get("maxItems").as_int(150);
    std::string code = payloads::LUAU_ZONESCAN;
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));
    return tool_text_response(eval_luau_safe(real, "zonescan", code));
}

Json run_toolscan(RealBridge& real, const Json& args) {
    bool inc_storage = args.get("includeStorage").as_bool(true);
    bool chk_conn = args.get("checkConnections").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(120);

    std::string code = payloads::LUAU_TOOLSCAN;
    code = replace_all(std::move(code), "__INCLUDE_STORAGE__", bstr(inc_storage));
    code = replace_all(std::move(code), "__CHECK_CONNECTIONS__", bstr(chk_conn));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    return tool_text_response(eval_luau_safe(real, "toolscan", code));
}

Json run_netcheck(RealBridge& real, const Json& args) {
    std::string target = args.get("targetPath").as_string("");
    bool unanchored = args.get("unanchoredOnly").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(120);

    std::string code = payloads::LUAU_NETCHECK;
    code = replace_all(std::move(code), "__TARGET_PATH__", luau_str(target));
    code = replace_all(std::move(code), "__UNANCHORED_ONLY__", bstr(unanchored));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    return tool_text_response(eval_luau_safe(real, "netcheck", code));
}

Json run_animscan(RealBridge& real, const Json& args) {
    bool inc_sounds = args.get("includeSounds").as_bool(true);
    int64_t max_items = args.get("maxItems").as_int(150);

    std::string code = payloads::LUAU_ANIMSCAN;
    code = replace_all(std::move(code), "__INCLUDE_SOUNDS__", bstr(inc_sounds));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    return tool_text_response(eval_luau_safe(real, "animscan", code));
}

Json run_animmake(RealBridge& real, const Json& args) {
    std::string action = args.get("action").as_string("play");
    std::string anim_id = args.get("animId").as_string("custom_anim");
    bool loop = args.get("loop").as_bool(true);
    double speed = args.get("speed").as_double(1.0);
    Json kf = args.get("keyframes").is_array() ? args.get("keyframes") : Json::array();
    std::string kf_json = kf.dump();

    std::string code = payloads::LUAU_ANIMMAKE;
    code = replace_all(std::move(code), "__ACTION__", luau_str(action));
    code = replace_all(std::move(code), "__ANIM_ID__", luau_str(anim_id));
    code = replace_all(std::move(code), "__LOOP__", bstr(loop));
    code = replace_all(std::move(code), "__SPEED__", std::to_string(speed));
    code = replace_all(std::move(code), "__KEYFRAMES_JSON__", luau_str(kf_json));

    return tool_text_response(eval_luau_safe(real, "animmake", code));
}

Json run_scriptmap(RealBridge& real, const Json& args) {
    bool inc_nil = args.get("includeNil").as_bool(true);
    bool test_dec = args.get("testDecompile").as_bool(false);
    std::string filter = args.get("filter").as_string("");
    int64_t max_items = args.get("maxItems").as_int(200);

    std::string code = payloads::LUAU_SCRIPTMAP;
    code = replace_all(std::move(code), "__INCLUDE_NIL__", bstr(inc_nil));
    code = replace_all(std::move(code), "__TEST_DECOMPILE__", bstr(test_dec));
    code = replace_all(std::move(code), "__FILTER__", luau_str(filter));
    code = replace_all(std::move(code), "__MAX_ITEMS__", std::to_string(max_items));

    return tool_text_response(eval_luau_safe(real, "scriptmap", code));
}

Json run_hookwatch(RealBridge& real, const Json& args) {
    std::string action = args.get("action").as_string("collect");
    bool inc_builtin = args.get("includeBuiltin").as_bool(true);

    std::string code = payloads::LUAU_HOOKWATCH;
    code = replace_all(std::move(code), "__ACTION__", luau_str(action));

    Json report = Json::object({
        {"hookWatch", eval_luau_safe(real, "hookwatch", code)}
    });
    if (inc_builtin) {
        report.as_object()["anticheatHooks"] = call_optional(real, "dump-anticheat-hooks", Json::object());
    }
    return tool_text_response(report);
}

} // namespace realplus
