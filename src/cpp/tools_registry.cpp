#include "tools.hpp"
#include <functional>
#include <unordered_map>

namespace realplus {

static Json prop_bool(const std::string& desc, bool def) {
    return Json::object({{"type", Json("boolean")}, {"description", Json(desc)}, {"default", Json(def)}});
}

static Json prop_int(const std::string& desc, int64_t def) {
    return Json::object({{"type", Json("integer")}, {"description", Json(desc)}, {"default", Json(def)}});
}

static Json prop_num(const std::string& desc, double def) {
    return Json::object({{"type", Json("number")}, {"description", Json(desc)}, {"default", Json(def)}});
}

static Json prop_str(const std::string& desc, const std::string& def = "") {
    Json o = Json::object({{"type", Json("string")}, {"description", Json(desc)}});
    if (!def.empty()) o.as_object()["default"] = Json(def);
    return o;
}

static Json make_tool(const std::string& name, const std::string& desc, Json props,
                      std::vector<std::string> required = {}) {
    Json schema = Json::object({
        {"type", Json("object")},
        {"properties", std::move(props)}
    });
    if (!required.empty()) {
        Json req = Json::array();
        for (auto& r : required) req.as_array().push_back(Json(r));
        schema.as_object()["required"] = std::move(req);
    }
    return Json::object({
        {"name", Json(name)},
        {"description", Json(desc)},
        {"inputSchema", std::move(schema)}
    });
}

Json list_plus_tools() {
    Json arr = Json::array();
    auto& a = arr.as_array();

    a.push_back(make_tool("scac",
        "Scan for client-side anti-cheat: hidden scripts, Suspicious GUIs, remotes, hook detections, and Connections.",
        Json::object({
            {"includeConnections", prop_bool("Inspect Humanoid/Root/Player/RunService connections", true)},
            {"checkRemotes", prop_bool("Flag anti-cheat / kick / ban remotes", true)},
            {"includeBuiltinScan", prop_bool("Run built-in dump-anticheat-hooks and list-special-instances", true)}
        })));

    a.push_back(make_tool("uiscan",
        "Scan PlayerGui, StarterGui, CoreGui, and nil for hidden, invisible, or suspicious UI elements.",
        Json::object({
            {"includeNil", prop_bool("Scan nil instances for ScreenGuis", true)},
            {"includeCoreGui", prop_bool("Scan CoreGui", false)},
            {"onlySuspicious", prop_bool("Return only flagged UI elements", false)},
            {"checkConnections", prop_bool("Check MouseButton1Click / Activated connections on buttons", true)},
            {"maxItems", prop_int("Max items to return", 150)}
        })));

    a.push_back(make_tool("httpscan",
        "Scan for HTTP/URL references, Discord webhooks, RequestAsync/HttpGet usage, and optionally hook live requests.",
        Json::object({
            {"scanInstances", prop_bool("Scan ValueBase/StringValue instances for URLs", true)},
            {"hookRequests", prop_bool("Install runtime request hook in game", false)},
            {"grepScripts", prop_bool("Grep decompiled scripts for URLs and HttpService", true)}
        })));

    a.push_back(make_tool("remmap",
        "Map all RemoteEvent, RemoteFunction, UnreliableRemoteEvent (and optional Bindables) with path, nil status, and risk flags.",
        Json::object({
            {"includeNil", prop_bool("Include nil-parented remotes", true)},
            {"includeBindables", prop_bool("Include BindableEvent/BindableFunction", false)},
            {"filter", prop_str("Case-insensitive substring filter")},
            {"maxItems", prop_int("Max remotes to return", 200)}
        })));

    a.push_back(make_tool("proxscan",
        "Scan Workspace for ProximityPrompts, ClickDetectors, TouchTransmitters (TouchInterest), and Seats.",
        Json::object({
            {"includePrompts", prop_bool("Include ProximityPrompts", true)},
            {"includeClicks", prop_bool("Include ClickDetectors", true)},
            {"includeTouch", prop_bool("Include TouchTransmitters", true)},
            {"includeSeats", prop_bool("Include Seat/VehicleSeat", false)},
            {"checkConnections", prop_bool("Check active connections", true)},
            {"sortByDistance", prop_bool("Sort by distance from LocalPlayer", true)},
            {"maxItems", prop_int("Max items to return", 150)}
        })));

    a.push_back(make_tool("web-scripts",
        "Search ScriptBlox, RScripts, rblxscripts.net, and haxhell.com for Roblox scripts by query or current game PlaceId/Name, optionally fetching source.",
        Json::object({
            {"query", prop_str("Search query or game name")},
            {"platform", prop_str("Target platform: all, scriptblox, rscripts, rblxscripts, haxhell", "all")},
            {"mode", prop_str("Filter mode: all, free, paid", "all")},
            {"useCurrentGame", prop_bool("Auto-query using current Roblox game info", false)},
            {"fetchSource", prop_bool("Fetch raw script source when available", false)},
            {"maxResults", prop_int("Max results per platform", 10)},
            {"page", prop_int("Page number", 1)}
        })));

    a.push_back(make_tool("rbxgame-search",
        "Search Roblox games across curated categories or custom queries with live concurrent players, visits, likes/dislikes, creator, and icon URLs.",
        Json::object({
            {"query", prop_str("Search query or game title")},
            {"category", prop_str("Curated category: all, popular, anime, pvp, horror, simulator, shooter, rpg, obby, survival, driving, roleplay", "all")},
            {"includeVotes", prop_bool("Fetch upvotes and downvotes", true)},
            {"includeThumbnails", prop_bool("Fetch game icon thumbnail URLs", true)},
            {"maxResults", prop_int("Maximum games to return", 10)}
        })));

    a.push_back(make_tool("simpledeobf",
        "Deobfuscate and analyze protected Lua/Luau scripts with automatic detection (WeAreDevs, Prometheus V2, IronBrew 2, MoonSec V2/V3, string encryption, and character array folding).",
        Json::object({
            {"script", prop_str("Obfuscated Luau script code or payload")},
            {"mode", prop_str("Deobfuscation mode: auto, wearedevs, prometheus, ironbrew, moonsec, generic", "auto")},
            {"extractStrings", prop_bool("Extract decrypted strings and literals", true)},
            {"extractUrls", prop_bool("Extract hidden URLs, webhooks, and endpoints", true)}
        }), {"script"}));

    a.push_back(make_tool("dumper",
        "Dump full Roblox data model hierarchies, ReplicatedStorage remotes, ModuleScript configs, and ValueBases into structured JSON for offline reverse engineering.",
        Json::object({
            {"target", prop_str("Target container: all, ReplicatedStorage, StarterGui, StarterPlayer, Workspace", "all")},
            {"maxDepth", prop_int("Maximum recursion depth", 3)},
            {"saveToFile", prop_str("Optional filename to save the dump directly on disk")}
        })));

    a.push_back(make_tool("clicheck",
        "Audit client-trusted attributes, ValueBases, cooldowns, speeds, and client-side check patterns.",
        Json::object({
            {"includeGc", prop_bool("Scan GC for cooldown/damage/speed constants", true)},
            {"includeGrep", prop_bool("Grep scripts for client-side check patterns", true)},
            {"maxItems", prop_int("Max items to return", 150)}
        })));

    a.push_back(make_tool("modscan",
        "Scan ModuleScripts across ReplicatedStorage, StarterPlayer, Backpack, and nil; optionally require() and dump exported keys.",
        Json::object({
            {"filter", prop_str("Case-insensitive name/path filter")},
            {"requireModules", prop_bool("Attempt safe require() on matching modules", false)},
            {"maxItems", prop_int("Max modules to return", 120)}
        })));

    a.push_back(make_tool("econscan",
        "Scan game economy: Leaderstats, currencies, shops, GamePasses, DeveloperProducts, and economy-related scripts.",
        Json::object({
            {"includeGrep", prop_bool("Grep scripts for MarketplaceService/PromptPurchase", true)},
            {"maxItems", prop_int("Max items to return", 150)}
        })));

    a.push_back(make_tool("keyscan",
        "Find key systems, whitelist checks, HWID checks, and Discord/Linkvertise URLs across scripts and GC.",
        Json::object({
            {"includeGc", prop_bool("Scan GC functions/tables for key/whitelist constants", true)},
            {"maxMatches", prop_int("Max matches per pattern", 20)}
        })));

    a.push_back(make_tool("remtrace",
        "Trace a specific Remote: find all calling scripts via find-remote-callers, grep references, and optionally spy live traffic.",
        Json::object({
            {"remoteName", prop_str("Remote name to trace")},
            {"remotePath", prop_str("Optional full path to the remote")},
            {"spyDurationSec", prop_num("Optional live remote-spy capture duration in seconds (0 = skip)", 0.0)},
            {"maxCallers", prop_int("Max caller scripts to return", 15)}
        }), {"remoteName"}));

    a.push_back(make_tool("toolscan",
        "Scan Tools in Backpack, Character, StarterPack, and ReplicatedStorage/Workspace with remotes and connections.",
        Json::object({
            {"includeStorage", prop_bool("Include ReplicatedStorage/Workspace/StarterPack tools", true)},
            {"checkConnections", prop_bool("Inspect Activated/Equipped connections", true)},
            {"maxItems", prop_int("Max tools to return", 120)}
        })));

    a.push_back(make_tool("npcscan",
        "Scan Workspace for NPCs/Humanoids (non-player characters), HP, WalkSpeed, distance, and attached prompts/remotes.",
        Json::object({
            {"aliveOnly", prop_bool("Only NPCs with Health > 0", true)},
            {"sortByDistance", prop_bool("Sort by distance from LocalPlayer", true)},
            {"maxItems", prop_int("Max NPCs to return", 100)}
        })));

    a.push_back(make_tool("zonescan",
        "Scan Workspace and CollectionService tags for zones, teleports, spawns, safezones, regions, and doors.",
        Json::object({
            {"maxItems", prop_int("Max zones to return", 150)}
        })));

    a.push_back(make_tool("scriptmap",
        "Map all LocalScripts and ModuleScripts (including nil), their enabled state, paths, and optional decompile test.",
        Json::object({
            {"includeNil", prop_bool("Include nil-parented scripts", true)},
            {"testDecompile", prop_bool("Test decompile() availability on scripts", false)},
            {"filter", prop_str("Case-insensitive filter")},
            {"maxItems", prop_int("Max scripts to return", 200)}
        })));

    a.push_back(make_tool("hookwatch",
        "Detect runtime metamethod/hookfunction/newcclosure tampering and built-in anticheat hooks.",
        Json::object({
            {"action", prop_str("Action: start or collect", "collect")},
            {"includeBuiltin", prop_bool("Include dump-anticheat-hooks", true)}
        })));

    a.push_back(make_tool("recon",
        "One-shot full game reconnaissance orchestrating game info, remmap, scac, proxscan, econscan, uiscan, clicheck, and memory save.",
        Json::object({
            {"depth", prop_str("Recon depth: quick or full", "quick")},
            {"remember", prop_bool("Save summary to remember-game memory", true)}
        })));

    a.push_back(make_tool("remotesim",
        "Safely test-fire or invoke a Remote with given arguments while capturing inbound responses via inbound-spy.",
        Json::object({
            {"remotePath", prop_str("Full path or dot-separated path to Remote")},
            {"method", prop_str("FireServer or InvokeServer", "FireServer")},
            {"args", Json::object({{"type", Json("array")}, {"description", Json("Arguments array to pass")}, {"default", Json::array()}})},
            {"captureInbound", prop_bool("Listen for inbound remote responses", true)},
            {"captureSec", prop_num("Capture window in seconds", 1.5)}
        }), {"remotePath"}));

    a.push_back(make_tool("animscan",
        "Scan ReplicatedStorage, Workspace, StarterPack, and Character for Animation and Sound assets with IDs.",
        Json::object({
            {"includeSounds", prop_bool("Include Sound instances", true)},
            {"maxItems", prop_int("Max assets to return", 150)}
        })));

    a.push_back(make_tool("netcheck",
        "Inspect NetworkOwnership (isnetworkowner / ReceiveAge) of unanchored Workspace parts near LocalPlayer.",
        Json::object({
            {"targetPath", prop_str("Optional specific Model/Part path to inspect")},
            {"unanchoredOnly", prop_bool("Only inspect unanchored BaseParts", true)},
            {"maxItems", prop_int("Max parts to return", 120)}
        })));

    a.push_back(make_tool("animmake",
        "Create, play, stop, list, or export FE keyframe animations directly on LocalPlayer Motor6D joints (C0/Transform).",
        Json::object({
            {"action", prop_str("Action: play, stop, list, or export", "play")},
            {"animId", prop_str("Animation identifier", "custom_anim")},
            {"loop", prop_bool("Loop the animation", true)},
            {"speed", prop_num("Playback speed multiplier", 1.0)},
            {"keyframes", Json::object({{"type", Json("array")}, {"description", Json("Array of {time, joints: {JointName: [x,y,z,rx,ry,rz]}}")}, {"default", Json::array()}})}
        })));

    return arr;
}

Json dispatch_plus_tool(RealBridge& real, const std::string& name, const Json& args) {
    using Handler = std::function<Json(RealBridge&, const Json&)>;
    static const std::unordered_map<std::string, Handler> table = {
        {"scac", run_scac},
        {"uiscan", run_uiscan},
        {"httpscan", run_httpscan},
        {"remmap", run_remmap},
        {"proxscan", run_proxscan},
        {"web-scripts", run_web_scripts},
        {"rbxgame-search", run_rbxgame_search},
        {"simpledeobf", run_simpledeobf},
        {"dumper", run_dumper},
        {"clicheck", run_clicheck},
        {"modscan", run_modscan},
        {"econscan", run_econscan},
        {"keyscan", run_keyscan},
        {"remtrace", run_remtrace},
        {"toolscan", run_toolscan},
        {"npcscan", run_npcscan},
        {"zonescan", run_zonescan},
        {"scriptmap", run_scriptmap},
        {"hookwatch", run_hookwatch},
        {"recon", run_recon},
        {"remotesim", run_remotesim},
        {"animscan", run_animscan},
        {"netcheck", run_netcheck},
        {"animmake", run_animmake},
    };

    auto it = table.find(name);
    if (it == table.end()) {
        return tool_error("Unknown tool: " + name);
    }
    try {
        return it->second(real, args.is_object() ? args : Json::object());
    } catch (const std::exception& e) {
        return tool_error(std::string("Tool '") + name + "' failed: " + e.what());
    }
}

} // namespace realplus
