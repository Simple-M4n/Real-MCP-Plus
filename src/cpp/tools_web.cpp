#include "tools.hpp"
#include "../c/win_bridge.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace realplus {

static std::string url_encode(const std::string& value) {
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out += "%20";
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned>(c));
            out += buf;
        }
    }
    return out;
}

static bool http_get_str(const std::string& url, int timeout_ms, std::string& out_body, int& out_status, std::string& out_err) {
    char* body = nullptr;
    size_t len = 0;
    int status = 0;
    int rc = win_http_get(url.c_str(), timeout_ms, &body, &len, &status);
    out_status = status;
    if (rc != 0) {
        if (body) win_free(body);
        out_err = "WinHTTP transport failed for " + url;
        return false;
    }
    if (body) {
        out_body.assign(body, len);
        win_free(body);
    } else {
        out_body.clear();
    }
    if (status < 200 || status >= 300) {
        out_err = "HTTP " + std::to_string(status) + " from " + url;
        return false;
    }
    return true;
}

static Json fetch_scriptblox(const std::string& query, const std::string& mode, int64_t page, int64_t max_res, bool fetch_source) {
    std::string url = "https://scriptblox.com/api/script/search?q=" + url_encode(query) +
                      "&page=" + std::to_string(page) +
                      "&max=" + std::to_string(max_res);
    if (mode == "free" || mode == "paid") {
        url += "&mode=" + mode;
    }

    std::string body, err;
    int status = 0;
    if (!http_get_str(url, 15000, body, status, err)) {
        return Json::object({{"error", Json(err)}, {"status", Json(static_cast<int64_t>(status))}});
    }

    Json parsed;
    if (!Json::try_parse(body, parsed)) {
        return Json::object({{"error", Json("Invalid JSON from ScriptBlox")}});
    }

    const Json& scripts = parsed.get("result").get("scripts");
    if (!scripts.is_array()) {
        return Json::object({{"scripts", Json::array()}, {"raw", parsed}});
    }

    Json items = Json::array();
    for (const auto& s : scripts.as_array()) {
        if (!s.is_object()) continue;
        if (static_cast<int64_t>(items.as_array().size()) >= max_res) break;

        std::string slug = s.get("slug").as_string("");
        std::string title = s.get("title").as_string("");
        std::string game_name = s.get("game").get("name").as_string("");
        std::string script_src = s.get("script").as_string("");
        bool is_paid = (s.get("scriptType").as_string("") == "paid");

        if (fetch_source && script_src.empty() && !slug.empty()) {
            std::string detail_body, detail_err;
            int dstat = 0;
            if (http_get_str("https://scriptblox.com/api/script/" + url_encode(slug), 10000, detail_body, dstat, detail_err)) {
                Json dp;
                if (Json::try_parse(detail_body, dp)) {
                    std::string fetched = dp.get("script").get("script").as_string("");
                    if (!fetched.empty()) script_src = fetched;
                }
            }
        }

        Json item = Json::object({
            {"platform", Json("scriptblox")},
            {"title", Json(title)},
            {"game", Json(game_name)},
            {"slug", Json(slug)},
            {"paid", Json(is_paid)},
            {"keySystem", Json(s.get("key").as_bool(false))},
            {"verified", Json(s.get("verified").as_bool(false))},
            {"views", Json(s.get("views").as_int(0))},
            {"url", Json(slug.empty() ? "" : ("https://scriptblox.com/script/" + slug))}
        });
        if (fetch_source && !script_src.empty()) {
            if (script_src.size() > 16000) script_src.resize(16000);
            item.as_object()["source"] = Json(script_src);
        }
        items.as_array().push_back(std::move(item));
    }

    return Json::object({
        {"count", Json(static_cast<int64_t>(items.as_array().size()))},
        {"scripts", std::move(items)}
    });
}

static Json fetch_rscripts(const std::string& query, const std::string& mode, int64_t page, int64_t max_res, bool fetch_source) {
    std::string url = "https://rscripts.net/api/v2/scripts?q=" + url_encode(query) +
                      "&page=" + std::to_string(page) +
                      "&limit=" + std::to_string(max_res);
    if (mode == "free") url += "&paid=false";
    else if (mode == "paid") url += "&paid=true";

    std::string body, err;
    int status = 0;
    if (!http_get_str(url, 15000, body, status, err)) {
        return Json::object({{"error", Json(err)}, {"status", Json(static_cast<int64_t>(status))}});
    }

    Json parsed;
    if (!Json::try_parse(body, parsed)) {
        return Json::object({{"error", Json("Invalid JSON from RScripts")}});
    }

    const Json& scripts = parsed.get("scripts");
    if (!scripts.is_array()) {
        return Json::object({{"scripts", Json::array()}, {"raw", parsed}});
    }

    Json items = Json::array();
    for (const auto& s : scripts.as_array()) {
        if (!s.is_object()) continue;
        if (static_cast<int64_t>(items.as_array().size()) >= max_res) break;

        std::string id = s.get("_id").as_string("");
        std::string title = s.get("title").as_string("");
        std::string raw_url = s.get("rawScript").as_string("");
        std::string game_name = s.get("game").get("title").as_string("");
        if (game_name.empty()) game_name = s.get("game").get("name").as_string("");

        Json item = Json::object({
            {"platform", Json("rscripts")},
            {"id", Json(id)},
            {"title", Json(title)},
            {"game", Json(game_name)},
            {"paid", Json(s.get("paid").as_bool(false))},
            {"keySystem", Json(s.get("keySystem").as_bool(false))},
            {"mobileReady", Json(s.get("mobileReady").as_bool(false))},
            {"views", Json(s.get("views").as_int(0))},
            {"rawScript", Json(raw_url)},
            {"url", Json(id.empty() ? "" : ("https://rscripts.net/script/" + id))}
        });

        if (fetch_source && !raw_url.empty()) {
            std::string src_body, src_err;
            int sstat = 0;
            if (http_get_str(raw_url, 10000, src_body, sstat, src_err)) {
                if (src_body.size() > 16000) src_body.resize(16000);
                item.as_object()["source"] = Json(src_body);
            }
        }
        items.as_array().push_back(std::move(item));
    }

    return Json::object({
        {"count", Json(static_cast<int64_t>(items.as_array().size()))},
        {"scripts", std::move(items)}
    });
}

static Json fetch_rblxscripts(const std::string& query, int64_t max_res, bool fetch_source) {
    std::string url = "https://api.rblxscripts.net/v1/search?q=" + url_encode(query) + "&type=scripts";
    std::string body, err;
    int status = 0;
    if (!http_get_str(url, 15000, body, status, err)) {
        return Json::object({{"error", Json(err)}, {"status", Json(static_cast<int64_t>(status))}});
    }

    Json parsed;
    if (!Json::try_parse(body, parsed)) {
        return Json::object({{"error", Json("Invalid JSON from rblxscripts.net")}});
    }

    const Json& scripts = parsed.get("data").get("scripts");
    if (!scripts.is_array()) {
        return Json::object({{"scripts", Json::array()}, {"raw", parsed}});
    }

    Json items = Json::array();
    for (const auto& s : scripts.as_array()) {
        if (!s.is_object()) continue;
        if (static_cast<int64_t>(items.as_array().size()) >= max_res) break;

        std::string id = s.get("id").as_string("");
        std::string title = s.get("title").as_string("");
        std::string slug = s.get("slug").as_string("");
        std::string raw_url = s.get("rawUrl").as_string("");
        std::string game_name = s.get("game").get("name").as_string("");

        Json item = Json::object({
            {"platform", Json("rblxscripts")},
            {"id", Json(id)},
            {"title", Json(title)},
            {"game", Json(game_name)},
            {"slug", Json(slug)},
            {"keySystem", Json(s.get("key").as_bool(false))},
            {"patched", Json(s.get("isPatched").as_bool(false))},
            {"rawUrl", Json(raw_url)},
            {"url", Json(s.get("url").as_string(""))}
        });

        if (fetch_source && !raw_url.empty()) {
            std::string src_body, src_err;
            int sstat = 0;
            if (http_get_str(raw_url, 10000, src_body, sstat, src_err)) {
                if (src_body.size() > 16000) src_body.resize(16000);
                item.as_object()["source"] = Json(src_body);
            }
        }
        items.as_array().push_back(std::move(item));
    }

    return Json::object({
        {"count", Json(static_cast<int64_t>(items.as_array().size()))},
        {"scripts", std::move(items)}
    });
}

static Json fetch_haxhell(const std::string& query, int64_t max_res, bool fetch_source) {
    std::string url = "https://haxhell.com/api/v1/scripts?q=" + url_encode(query);
    std::string body, err;
    int status = 0;
    if (!http_get_str(url, 15000, body, status, err)) {
        return Json::object({{"error", Json(err)}, {"status", Json(static_cast<int64_t>(status))}});
    }

    Json parsed;
    if (!Json::try_parse(body, parsed)) {
        return Json::object({{"error", Json("Invalid JSON from haxhell.com")}});
    }

    const Json& scripts = parsed.get("data");
    if (!scripts.is_array()) {
        return Json::object({{"scripts", Json::array()}, {"raw", parsed}});
    }

    Json items = Json::array();
    for (const auto& s : scripts.as_array()) {
        if (!s.is_object()) continue;
        if (static_cast<int64_t>(items.as_array().size()) >= max_res) break;

        std::string id = s.get("id").as_string("");
        std::string title = s.get("title").as_string("");
        std::string slug = s.get("slug").as_string("");
        std::string game_name = s.get("game").get("name").as_string("");
        std::string raw_url = s.get("links").get("raw").as_string("");

        Json item = Json::object({
            {"platform", Json("haxhell")},
            {"id", Json(id)},
            {"title", Json(title)},
            {"game", Json(game_name)},
            {"slug", Json(slug)},
            {"keySystem", Json(s.get("flags").get("keySystem").as_bool(false))},
            {"patched", Json(s.get("flags").get("patched").as_bool(false))},
            {"rawUrl", Json(raw_url)},
            {"url", Json(s.get("links").get("webpage").as_string(""))}
        });

        if (fetch_source && !raw_url.empty()) {
            std::string src_body, src_err;
            int sstat = 0;
            if (http_get_str(raw_url, 10000, src_body, sstat, src_err)) {
                if (src_body.size() > 16000) src_body.resize(16000);
                item.as_object()["source"] = Json(src_body);
            }
        }
        items.as_array().push_back(std::move(item));
    }

    return Json::object({
        {"count", Json(static_cast<int64_t>(items.as_array().size()))},
        {"scripts", std::move(items)}
    });
}

Json run_web_scripts_opt(RealBridge* real, const Json& args) {
    std::string query = args.get("query").as_string("");
    std::string platform = args.get("platform").as_string("all");
    std::string mode = args.get("mode").as_string("all");
    bool use_game = args.get("useCurrentGame").as_bool(false);
    bool fetch_src = args.get("fetchSource").as_bool(false);
    int64_t max_res = std::clamp<int64_t>(args.get("maxResults").as_int(10), 1, 30);
    int64_t page = std::max<int64_t>(args.get("page").as_int(1), 1);

    Json game_info = Json::null();
    if (real && (use_game || query.empty())) {
        game_info = call_optional(*real, "get-game-info", Json::object());
        if (query.empty() && game_info.is_object()) {
            query = game_info.get("name").as_string("");
            if (query.empty()) query = game_info.get("gameName").as_string("");
            if (query.empty() && game_info.contains("placeId")) {
                query = game_info.get("placeId").as_string_lossy();
            }
        }
    }

    if (query.empty()) {
        return tool_error("query is empty and current game info could not be resolved");
    }

    Json report = Json::object({
        {"query", Json(query)},
        {"platform", Json(platform)},
        {"mode", Json(mode)},
        {"page", Json(page)}
    });
    if (!game_info.is_null()) {
        report.as_object()["gameInfo"] = game_info;
    }

    if (platform == "all") {
        auto sb_task = std::async(std::launch::async, [&]() {
            return fetch_scriptblox(query, mode, page, max_res, fetch_src);
        });
        auto rs_task = std::async(std::launch::async, [&]() {
            return fetch_rscripts(query, mode, page, max_res, fetch_src);
        });
        auto rblx_task = std::async(std::launch::async, [&]() {
            return fetch_rblxscripts(query, max_res, fetch_src);
        });
        auto hax_task = std::async(std::launch::async, [&]() {
            return fetch_haxhell(query, max_res, fetch_src);
        });
        report.as_object()["scriptblox"] = sb_task.get();
        report.as_object()["rscripts"] = rs_task.get();
        report.as_object()["rblxscripts"] = rblx_task.get();
        report.as_object()["haxhell"] = hax_task.get();
    } else {
        if (platform == "scriptblox") {
            report.as_object()["scriptblox"] = fetch_scriptblox(query, mode, page, max_res, fetch_src);
        } else if (platform == "rscripts") {
            report.as_object()["rscripts"] = fetch_rscripts(query, mode, page, max_res, fetch_src);
        } else if (platform == "rblxscripts") {
            report.as_object()["rblxscripts"] = fetch_rblxscripts(query, max_res, fetch_src);
        } else if (platform == "haxhell") {
            report.as_object()["haxhell"] = fetch_haxhell(query, max_res, fetch_src);
        }
    }

    return tool_text_response(report);
}

Json run_web_scripts(RealBridge& real, const Json& args) {
    return run_web_scripts_opt(&real, args);
}

struct CuratedGame {
    const char* name;
    int64_t universe_id;
    int64_t place_id;
    const char* category;
};

static const CuratedGame CURATED_GAMES[] = {
    {"Blox Fruits", 994732206, 2753915549, "anime"},
    {"Anime Defenders", 5894178524, 17017769292, "anime"},
    {"Anime Vanguards", 6035882049, 16146832113, "anime"},
    {"The Strongest Battlegrounds", 3804820993, 10449761463, "pvp"},
    {"Blade Ball", 4777817887, 13772394625, "pvp"},
    {"Rivals", 5233667503, 17625359962, "shooter"},
    {"Arsenal", 111958650, 286090429, "shooter"},
    {"Fisch", 6325068221, 16732694052, "simulator"},
    {"Pet Simulator 99", 5095191194, 8737899170, "simulator"},
    {"Bee Swarm Simulator", 63319010, 1537690962, "simulator"},
    {"Doors", 3537248384, 6516141723, "horror"},
    {"Dandy's World", 5836881260, 15996793934, "horror"},
    {"Tower of Hell", 703124385, 1962086868, "obby"},
    {"Deepwoken", 1359573625, 4170656461, "rpg"},
    {"Natural Disaster Survival", 920587237, 189707, "survival"},
    {"Brookhaven RP", 2012091522, 4924922222, "roleplay"},
    {"Driving Empire", 1078712392, 3351674303, "driving"},
    {"Crossroads", 13058, 1818, "classic"}
};

static int64_t resolve_place_to_universe(int64_t place_id) {
    std::string url = "https://apis.roblox.com/universes/v1/places/" + std::to_string(place_id) + "/universe";
    std::string body, err;
    int status = 0;
    if (!http_get_str(url, 8000, body, status, err)) return 0;
    Json j;
    if (Json::try_parse(body, j) && j.is_object() && j.contains("universeId")) {
        return j.get("universeId").as_int(0);
    }
    return 0;
}

Json run_rbxgame_search_opt(RealBridge* real, const Json& args) {
    (void)real;
    std::string query = args.get("query").as_string("");
    std::string category = args.get("category").as_string("all");
    bool inc_votes = args.get("includeVotes").as_bool(true);
    bool inc_thumbs = args.get("includeThumbnails").as_bool(true);
    int64_t max_results = std::clamp<int64_t>(args.get("maxResults").as_int(10), 1, 25);

    std::string q_lower = query;
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), [](unsigned char c) { return std::tolower(c); });
    std::string cat_lower = category;
    std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(), [](unsigned char c) { return std::tolower(c); });

    struct FoundGame {
        int64_t universe_id = 0;
        int64_t place_id = 0;
        std::string fallback_name;
        std::string category;
    };
    std::vector<FoundGame> candidates;
    std::unordered_set<int64_t> seen_universes;

    for (const auto& cg : CURATED_GAMES) {
        if (cat_lower != "all" && cat_lower != "popular" && cat_lower != cg.category) {
            continue;
        }
        if (!q_lower.empty()) {
            std::string n = cg.name;
            std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
            if (n.find(q_lower) == std::string::npos && std::string(cg.category).find(q_lower) == std::string::npos) {
                continue;
            }
        }
        if (seen_universes.insert(cg.universe_id).second) {
            candidates.push_back({cg.universe_id, cg.place_id, cg.name, cg.category});
            if (static_cast<int64_t>(candidates.size()) >= max_results) break;
        }
    }

    if (!q_lower.empty() && static_cast<int64_t>(candidates.size()) < max_results) {
        std::string sb_url = "https://scriptblox.com/api/script/search?q=" + url_encode(query) + "&max=10";
        std::string sb_body, sb_err;
        int sb_status = 0;
        if (http_get_str(sb_url, 8000, sb_body, sb_status, sb_err)) {
            Json sb_json;
            if (Json::try_parse(sb_body, sb_json)) {
                const auto& scripts = sb_json.get("result").get("scripts");
                if (scripts.is_array()) {
                    for (const auto& s : scripts.as_array()) {
                        if (!s.is_object()) continue;
                        const auto& g = s.get("game");
                        if (!g.is_object()) continue;
                        int64_t pid = g.get("gameId").as_int(0);
                        if (pid <= 0) continue;
                        int64_t uid = resolve_place_to_universe(pid);
                        if (uid > 0 && seen_universes.insert(uid).second) {
                            candidates.push_back({uid, pid, g.get("name").as_string(""), "search"});
                            if (static_cast<int64_t>(candidates.size()) >= max_results) break;
                        }
                    }
                }
            }
        }
    }

    if (candidates.empty()) {
        return tool_text_response(Json::object({
            {"query", Json(query)},
            {"category", Json(category)},
            {"count", Json(0)},
            {"games", Json::array()}
        }));
    }

    std::string uids_param;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i > 0) uids_param.push_back(',');
        uids_param += std::to_string(candidates[i].universe_id);
    }

    std::unordered_map<int64_t, std::pair<int64_t, int64_t>> votes_map;
    std::unordered_map<int64_t, std::string> thumbs_map;

    std::string info_url = "https://games.roblox.com/v1/games?universeIds=" + uids_param;
    std::string votes_url = "https://games.roblox.com/v1/games/votes?universeIds=" + uids_param;
    std::string thumb_url = "https://thumbnails.roblox.com/v1/games/icons?universeIds=" + uids_param + "&size=150x150&format=Png";

    auto fetch_info_task = std::async(std::launch::async, [info_url]() {
        std::string body, err;
        int stat = 0;
        http_get_str(info_url, 8000, body, stat, err);
        return body;
    });

    auto fetch_votes_task = std::async(std::launch::async, [votes_url, inc_votes]() {
        std::string body;
        if (inc_votes) {
            std::string err;
            int stat = 0;
            http_get_str(votes_url, 8000, body, stat, err);
        }
        return body;
    });

    auto fetch_thumbs_task = std::async(std::launch::async, [thumb_url, inc_thumbs]() {
        std::string body;
        if (inc_thumbs) {
            std::string err;
            int stat = 0;
            http_get_str(thumb_url, 8000, body, stat, err);
        }
        return body;
    });

    std::string info_body = fetch_info_task.get();
    Json info_json;
    std::unordered_map<int64_t, Json> info_map;
    if (Json::try_parse(info_body, info_json) && info_json.get("data").is_array()) {
        for (const auto& item : info_json.get("data").as_array()) {
            if (item.is_object() && item.contains("id")) {
                info_map[item.get("id").as_int(0)] = item;
            }
        }
    }

    std::string votes_body = fetch_votes_task.get();
    if (inc_votes && !votes_body.empty()) {
        Json vj;
        if (Json::try_parse(votes_body, vj) && vj.get("data").is_array()) {
            for (const auto& item : vj.get("data").as_array()) {
                if (item.is_object() && item.contains("id")) {
                    votes_map[item.get("id").as_int(0)] = {
                        item.get("upVotes").as_int(0),
                        item.get("downVotes").as_int(0)
                    };
                }
            }
        }
    }

    std::string thumb_body = fetch_thumbs_task.get();
    if (inc_thumbs && !thumb_body.empty()) {
        Json tj;
        if (Json::try_parse(thumb_body, tj) && tj.get("data").is_array()) {
            for (const auto& item : tj.get("data").as_array()) {
                if (item.is_object() && item.contains("targetId")) {
                    thumbs_map[item.get("targetId").as_int(0)] = item.get("imageUrl").as_string("");
                }
            }
        }
    }

    Json games_arr = Json::array();
    for (const auto& c : candidates) {
        int64_t uid = c.universe_id;
        auto it = info_map.find(uid);

        Json game_obj = Json::object({
            {"universeId", Json(uid)},
            {"placeId", Json(c.place_id)},
            {"category", Json(c.category)}
        });

        if (it != info_map.end()) {
            const auto& data = it->second;
            int64_t rpid = data.get("rootPlaceId").as_int(c.place_id);
            game_obj.as_object()["placeId"] = Json(rpid);
            game_obj.as_object()["name"] = Json(data.get("name").as_string(c.fallback_name));
            game_obj.as_object()["description"] = Json(data.get("description").as_string(""));
            game_obj.as_object()["playing"] = Json(data.get("playing").as_int(0));
            game_obj.as_object()["visits"] = Json(data.get("visits").as_int(0));
            game_obj.as_object()["favoritedCount"] = Json(data.get("favoritedCount").as_int(0));
            game_obj.as_object()["maxPlayers"] = Json(data.get("maxPlayers").as_int(0));
            game_obj.as_object()["genre"] = Json(data.get("genre").as_string(""));
            game_obj.as_object()["creator"] = data.get("creator");
            game_obj.as_object()["robloxUrl"] = Json("https://www.roblox.com/games/" + std::to_string(rpid));
        } else {
            game_obj.as_object()["name"] = Json(c.fallback_name);
            game_obj.as_object()["robloxUrl"] = Json("https://www.roblox.com/games/" + std::to_string(c.place_id));
        }

        if (inc_votes) {
            auto vit = votes_map.find(uid);
            int64_t up = (vit != votes_map.end()) ? vit->second.first : 0;
            int64_t down = (vit != votes_map.end()) ? vit->second.second : 0;
            double rating = (up + down > 0) ? (static_cast<double>(up) / static_cast<double>(up + down) * 100.0) : 0.0;
            game_obj.as_object()["upVotes"] = Json(up);
            game_obj.as_object()["downVotes"] = Json(down);
            game_obj.as_object()["ratingPercent"] = Json(rating);
        }

        if (inc_thumbs) {
            auto tit = thumbs_map.find(uid);
            game_obj.as_object()["iconUrl"] = Json((tit != thumbs_map.end()) ? tit->second : "");
        }

        games_arr.as_array().push_back(std::move(game_obj));
    }

    return tool_text_response(Json::object({
        {"query", Json(query)},
        {"category", Json(category)},
        {"count", Json(static_cast<int64_t>(games_arr.as_array().size()))},
        {"games", std::move(games_arr)}
    }));
}

Json run_rbxgame_search(RealBridge& real, const Json& args) {
    return run_rbxgame_search_opt(&real, args);
}

} // namespace realplus
