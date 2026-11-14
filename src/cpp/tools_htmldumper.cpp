#include "tools.hpp"
#include "payloads_gen.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace realplus {

static std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

static std::string get_class_badge_color(const std::string& cls) {
    if (cls == "LocalScript") return "#10b981"; // emerald
    if (cls == "ModuleScript") return "#06b6d4"; // cyan
    if (cls == "RemoteEvent" || cls == "RemoteFunction") return "#8b5cf6"; // purple
    if (cls == "Part" || cls == "MeshPart") return "#0ea5e9"; // sky
    if (cls == "Model") return "#f59e0b"; // amber
    if (cls == "ScreenGui" || cls == "Frame" || cls == "TextButton") return "#ec4899"; // pink
    if (cls == "Humanoid") return "#ef4444"; // red
    if (cls == "Sound") return "#14b8a6"; // teal
    if (cls == "Folder") return "#64748b"; // slate
    return "#6b7280"; // gray
}

static void render_node_html(const Json& node, std::ostringstream& ss, const std::string& mode) {
    if (!node.is_object()) return;

    std::string name = node.get("name").as_string("Unknown");
    std::string cls = node.get("className").as_string("Instance");
    std::string path = node.get("path").as_string("");
    bool has_children = node.contains("children") && node.get("children").is_array() && !node.get("children").as_array().empty();

    std::string color = get_class_badge_color(cls);

    ss << "<div class=\"tree-node\" data-name=\"" << html_escape(name) << "\" data-class=\"" << html_escape(cls) << "\" data-path=\"" << html_escape(path) << "\">\n";

    if (has_children) {
        ss << "<details open>\n<summary class=\"node-summary\">\n";
    } else {
        ss << "<div class=\"node-summary leaf\">\n";
    }

    ss << "<span class=\"badge\" style=\"background:" << color << ";\">" << html_escape(cls) << "</span> ";
    ss << "<span class=\"node-name\">" << html_escape(name) << "</span> ";
    ss << "<span class=\"node-path\">" << html_escape(path) << "</span>\n";

    // Mode All: properties & attributes
    if (mode == "All") {
        if (node.contains("properties") && node.get("properties").is_object()) {
            const auto& props = node.get("properties").as_object();
            if (!props.empty()) {
                ss << "<div class=\"props-block\">\n";
                for (const auto& [pk, pv] : props) {
                    if (pk == "source") {
                        std::string src = pv.as_string("");
                        ss << "<details class=\"code-details\"><summary>Source (" << src.size() << " bytes)</summary>";
                        ss << "<pre class=\"code-pre\"><code>" << html_escape(src) << "</code></pre></details>\n";
                    } else {
                        ss << "<span class=\"prop-tag\"><b>" << html_escape(pk) << ":</b> " << html_escape(pv.as_string_lossy()) << "</span> ";
                    }
                }
                ss << "</div>\n";
            }
        }
    }

    if (has_children) {
        ss << "</summary>\n<div class=\"children-container\">\n";
        for (const auto& child : node.get("children").as_array()) {
            render_node_html(child, ss, mode);
        }
        ss << "</div>\n</details>\n";
    } else {
        ss << "</div>\n";
    }

    ss << "</div>\n";
}

static std::string generate_html_document(const Json& dump_json, const std::string& mode, const std::string& screenshot_b64) {
    std::ostringstream ss;
    int64_t total = dump_json.get("totalScanned").as_int(0);
    std::string timestamp_str = std::to_string(dump_json.get("timestamp").as_int(0));
    std::string raw_json = dump_json.dump();

    ss << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    ss << "<meta charset=\"UTF-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    ss << "<title>Roblox DataModel Dump - " << html_escape(mode) << "</title>\n";
    ss << "<style>\n";
    ss << "* { box-sizing: border-box; margin: 0; padding: 0; }\n";
    ss << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: #0b0f19; color: #f1f5f9; padding: 24px; font-size: 14px; line-height: 1.5; }\n";
    ss << ".container { max-width: 1400px; margin: 0 auto; }\n";
    ss << ".header { display: flex; align-items: center; justify-content: space-between; padding-bottom: 20px; border-bottom: 1px solid #1e293b; margin-bottom: 20px; flex-wrap: wrap; gap: 12px; }\n";
    ss << ".title-area { display: flex; align-items: center; gap: 12px; }\n";
    ss << "h1 { font-size: 22px; font-weight: 700; color: #fff; }\n";
    ss << ".mode-badge { padding: 4px 10px; border-radius: 9999px; font-size: 12px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.05em; background: #3b82f6; color: #fff; }\n";
    ss << ".stats { font-size: 13px; color: #94a3b8; }\n";
    ss << ".toolbar { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; align-items: center; }\n";
    ss << ".search-box { flex: 1; min-width: 250px; background: #1e293b; border: 1px solid #334155; padding: 9px 14px; border-radius: 8px; color: #fff; font-size: 14px; outline: none; transition: border-color 0.2s; }\n";
    ss << ".search-box:focus { border-color: #3b82f6; }\n";
    ss << ".btn { background: #1e293b; color: #f1f5f9; border: 1px solid #334155; padding: 8px 14px; border-radius: 8px; font-size: 13px; font-weight: 600; cursor: pointer; transition: background 0.15s, border-color 0.15s; }\n";
    ss << ".btn:hover { background: #334155; border-color: #475569; }\n";
    ss << ".btn-primary { background: #2563eb; border-color: #1d4ed8; color: #fff; }\n";
    ss << ".btn-primary:hover { background: #1d4ed8; }\n";
    ss << ".split-layout { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }\n";
    ss << "@media(max-width: 900px) { .split-layout { grid-template-columns: 1fr; } }\n";
    ss << ".panel { background: #111827; border: 1px solid #1f2937; border-radius: 10px; padding: 16px; overflow: auto; max-height: 82vh; }\n";
    ss << ".panel-title { font-size: 15px; font-weight: 700; color: #e2e8f0; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }\n";
    ss << ".tree-node { margin: 2px 0; font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; font-size: 13px; }\n";
    ss << ".node-summary { padding: 4px 6px; border-radius: 6px; display: flex; align-items: center; gap: 8px; cursor: pointer; user-select: none; flex-wrap: wrap; }\n";
    ss << ".node-summary:hover { background: #1e293b; }\n";
    ss << ".node-summary.leaf { cursor: default; }\n";
    ss << ".children-container { margin-left: 20px; border-left: 1px dashed #334155; padding-left: 8px; }\n";
    ss << ".badge { padding: 2px 6px; border-radius: 4px; font-size: 11px; font-weight: 700; color: #fff; }\n";
    ss << ".node-name { font-weight: 600; color: #f8fafc; }\n";
    ss << ".node-path { color: #64748b; font-size: 11px; margin-left: auto; }\n";
    ss << ".props-block { width: 100%; margin: 4px 0 2px 28px; display: flex; flex-wrap: wrap; gap: 6px; }\n";
    ss << ".prop-tag { background: #1e293b; border: 1px solid #334155; padding: 2px 6px; border-radius: 4px; font-size: 11px; color: #cbd5e1; }\n";
    ss << ".code-details { width: 100%; margin-top: 4px; background: #0f172a; border: 1px solid #1e293b; border-radius: 6px; padding: 6px; }\n";
    ss << ".code-pre { background: #030712; padding: 10px; border-radius: 6px; font-family: monospace; font-size: 12px; color: #38bdf8; overflow-x: auto; max-height: 350px; }\n";
    ss << ".screenshot-img { width: 100%; height: auto; border-radius: 8px; border: 1px solid #334155; box-shadow: 0 4px 12px rgba(0,0,0,0.5); }\n";
    ss << ".entity-card { background: #1e293b; border: 1px solid #334155; border-radius: 8px; padding: 12px; margin-bottom: 10px; }\n";
    ss << ".hp-bar-bg { background: #334155; height: 8px; border-radius: 4px; overflow: hidden; margin: 6px 0; }\n";
    ss << ".hp-bar-fill { height: 100%; background: #10b981; transition: width 0.3s; }\n";
    ss << "</style>\n</head>\n<body>\n";

    ss << "<div class=\"container\">\n";
    ss << "  <div class=\"header\">\n";
    ss << "    <div class=\"title-area\">\n";
    ss << "      <h1>Roblox DataModel Explorer</h1>\n";
    ss << "      <span class=\"mode-badge\">" << html_escape(mode) << "</span>\n";
    ss << "    </div>\n";
    ss << "    <div class=\"stats\">Total Instances: <b>" << total << "</b> | Generated: " << timestamp_str << "</div>\n";
    ss << "  </div>\n";

    ss << "  <div class=\"toolbar\">\n";
    ss << "    <input type=\"text\" id=\"searchBox\" class=\"search-box\" placeholder=\"Filter by name, class, or path...\" oninput=\"filterTree()\">\n";
    ss << "    <button class=\"btn\" onclick=\"expandAll()\">Expand All</button>\n";
    ss << "    <button class=\"btn\" onclick=\"collapseAll()\">Collapse All</button>\n";
    ss << "    <button class=\"btn btn-primary\" onclick=\"downloadJson()\">Download JSON</button>\n";
    ss << "  </div>\n";

    // Mode-specific render
    const Json& data = dump_json.get("data");

    if (mode == "UI") {
        ss << "  <div class=\"split-layout\">\n";
        ss << "    <div class=\"panel\">\n";
        ss << "      <div class=\"panel-title\">UI Hierarchy Tree</div>\n";
        if (data.is_object() && data.contains("UI")) {
            for (const auto& [root_name, root_node] : data.get("UI").as_object()) {
                render_node_html(root_node, ss, mode);
            }
        }
        ss << "    </div>\n";
        ss << "    <div class=\"panel\">\n";
        ss << "      <div class=\"panel-title\">Game Window Visual Preview</div>\n";
        if (!screenshot_b64.empty()) {
            ss << "      <img src=\"data:image/png;base64," << screenshot_b64 << "\" class=\"screenshot-img\" alt=\"Roblox Window Screenshot\">\n";
        } else {
            ss << "      <div style=\"color:#64748b; padding:40px; text-align:center;\">Screenshot not captured or unavailable</div>\n";
        }
        ss << "    </div>\n";
        ss << "  </div>\n";
    } else if (mode == "Humanoid") {
        ss << "  <div class=\"panel\">\n";
        ss << "    <div class=\"panel-title\">Living Entities & Humanoids</div>\n";
        if (data.is_object() && data.contains("Entities") && data.get("Entities").is_array()) {
            for (const auto& ent : data.get("Entities").as_array()) {
                std::string ename = ent.get("name").as_string("Unknown");
                std::string epath = ent.get("path").as_string("");
                double hp = ent.get("health").as_double(0.0);
                double max_hp = ent.get("maxHealth").as_double(100.0);
                double speed = ent.get("walkSpeed").as_double(16.0);
                bool is_lp = ent.get("isLocalPlayer").as_bool(false);
                bool is_plr = ent.get("isPlayer").as_bool(false);
                double pct = (max_hp > 0.0) ? std::clamp((hp / max_hp) * 100.0, 0.0, 100.0) : 0.0;
                std::string hp_col = (pct > 50.0) ? "#10b981" : (pct > 20.0 ? "#f59e0b" : "#ef4444");

                ss << "<div class=\"entity-card\" data-name=\"" << html_escape(ename) << "\">\n";
                ss << "  <div style=\"display:flex; justify-content:space-between; align-items:center;\">\n";
                ss << "    <b style=\"font-size:15px;\">" << html_escape(ename) << "</b>\n";
                if (is_lp) ss << "<span class=\"badge\" style=\"background:#3b82f6;\">LOCAL PLAYER</span>\n";
                else if (is_plr) ss << "<span class=\"badge\" style=\"background:#8b5cf6;\">PLAYER</span>\n";
                else ss << "<span class=\"badge\" style=\"background:#64748b;\">NPC</span>\n";
                ss << "  </div>\n";
                ss << "  <div style=\"color:#94a3b8; font-size:12px; margin:2px 0;\">" << html_escape(epath) << "</div>\n";
                ss << "  <div class=\"hp-bar-bg\"><div class=\"hp-bar-fill\" style=\"width:" << pct << "%; background:" << hp_col << ";\"></div></div>\n";
                ss << "  <div style=\"font-size:12px; color:#cbd5e1; display:flex; gap:16px;\">\n";
                ss << "    <span>HP: <b>" << hp << " / " << max_hp << "</b></span>\n";
                ss << "    <span>Speed: <b>" << speed << "</b></span>\n";
                if (ent.contains("position") && ent.get("position").is_array()) {
                    const auto& p = ent.get("position").as_array();
                    if (p.size() >= 3) {
                        ss << "    <span>Pos: [" << p[0].as_double(0) << ", " << p[1].as_double(0) << ", " << p[2].as_double(0) << "]</span>\n";
                    }
                }
                ss << "  </div>\n";
                ss << "</div>\n";
            }
        }
        ss << "  </div>\n";
    } else if (mode == "Script") {
        ss << "  <div class=\"panel\">\n";
        ss << "    <div class=\"panel-title\">Scripts & Decompiled Sources</div>\n";
        if (data.is_object()) {
            for (const auto& [c_name, c_node] : data.as_object()) {
                render_node_html(c_node, ss, "All");
            }
        }
        ss << "  </div>\n";
    } else {
        // Structure and All
        ss << "  <div class=\"panel\">\n";
        ss << "    <div class=\"panel-title\">Hierarchy Tree</div>\n";
        if (data.is_object()) {
            for (const auto& [c_name, c_node] : data.as_object()) {
                render_node_html(c_node, ss, mode);
            }
        }
        ss << "  </div>\n";
    }

    ss << "</div>\n";

    // JavaScript for live search & export
    ss << "<script>\n";
    ss << "const RAW_DATA = " << raw_json << ";\n";
    ss << "function filterTree() {\n";
    ss << "  const q = document.getElementById('searchBox').value.toLowerCase();\n";
    ss << "  const nodes = document.querySelectorAll('.tree-node, .entity-card');\n";
    ss << "  nodes.forEach(n => {\n";
    ss << "    const text = (n.getAttribute('data-name') || '') + ' ' + (n.getAttribute('data-class') || '') + ' ' + (n.getAttribute('data-path') || '');\n";
    ss << "    if (!q || text.toLowerCase().includes(q)) {\n";
    ss << "      n.style.display = '';\n";
    ss << "    } else {\n";
    ss << "      n.style.display = 'none';\n";
    ss << "    }\n";
    ss << "  });\n";
    ss << "}\n";
    ss << "function expandAll() {\n";
    ss << "  document.querySelectorAll('details').forEach(d => d.open = true);\n";
    ss << "}\n";
    ss << "function collapseAll() {\n";
    ss << "  document.querySelectorAll('details').forEach(d => d.open = false);\n";
    ss << "}\n";
    ss << "function downloadJson() {\n";
    ss << "  const blob = new Blob([JSON.stringify(RAW_DATA, null, 2)], { type: 'application/json' });\n";
    ss << "  const url = URL.createObjectURL(blob);\n";
    ss << "  const a = document.createElement('a');\n";
    ss << "  a.href = url;\n";
    ss << "  a.download = 'roblox_dump_" << html_escape(mode) << ".json';\n";
    ss << "  document.body.appendChild(a);\n";
    ss << "  a.click();\n";
    ss << "  document.body.removeChild(a);\n";
    ss << "  URL.revokeObjectURL(url);\n";
    ss << "}\n";
    ss << "</script>\n";

    ss << "</body>\n</html>\n";
    return ss.str();
}

Json run_htmldumper(RealBridge& real, const Json& args) {
    std::string mode = args.get("mode").as_string("Structure");
    std::string format = args.get("format").as_string("html");
    std::string target = args.get("target").as_string("all");
    int64_t max_depth = args.get("maxDepth").as_int(5);
    if (max_depth < 1) max_depth = 1;
    if (max_depth > 15) max_depth = 15;
    bool include_source = args.get("includeSource").as_bool(true);
    bool capture_screenshot = args.get("captureScreenshot").as_bool(true);
    std::string save_to_file = args.get("saveToFile").as_string("");

    std::string code = payloads::LUAU_HTMLDUMPER;
    code = replace_all(std::move(code), "__MODE__", mode);
    code = replace_all(std::move(code), "__TARGET__", target);
    code = replace_all(std::move(code), "__MAX_DEPTH__", std::to_string(max_depth));
    code = replace_all(std::move(code), "__INCLUDE_SOURCE__", include_source ? "true" : "false");

    Json dump_result = eval_luau_safe(real, "htmldumper", code);
    if (dump_result.is_object() && dump_result.contains("error")) {
        return tool_error("htmldumper eval failed: " + dump_result.get("error").as_string_lossy());
    }

    std::string screenshot_b64;
    if (mode == "UI" && capture_screenshot) {
        try {
            Json ss_res = call_optional(real, "screenshot-window", Json::object());
            if (ss_res.is_object() && ss_res.contains("content") && ss_res.get("content").is_array()) {
                for (const auto& item : ss_res.get("content").as_array()) {
                    if (item.is_object()) {
                        if (item.get("type").as_string("") == "image") {
                            screenshot_b64 = item.get("data").as_string("");
                        } else if (item.contains("data")) {
                            screenshot_b64 = item.get("data").as_string("");
                        }
                    }
                }
            }
        } catch (...) {}
    }

    std::string html_content;
    if (format == "html" || format == "both") {
        html_content = generate_html_document(dump_result, mode, screenshot_b64);
    }

    if (!save_to_file.empty()) {
        try {
            std::filesystem::path p(save_to_file);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }
            std::ofstream out(p, std::ios::binary | std::ios::trunc);
            if (!out) {
                return tool_error("Failed to open file for writing: " + save_to_file);
            }
            if (format == "json" || (p.extension() == ".json" && format != "html")) {
                std::string jstr = dump_result.dump(2);
                out.write(jstr.data(), jstr.size());
            } else {
                out.write(html_content.data(), html_content.size());
            }
            out.close();

            size_t file_size = std::filesystem::file_size(p);
            Json summary = Json::object({
                {"status", Json("success")},
                {"mode", Json(mode)},
                {"format", Json(format)},
                {"target", Json(target)},
                {"totalScanned", dump_result.get("totalScanned")},
                {"savedToFile", Json(save_to_file)},
                {"fileSizeBytes", Json(static_cast<int64_t>(file_size))},
                {"timestamp", dump_result.get("timestamp")}
            });
            return tool_text_response(summary);
        } catch (const std::exception& e) {
            return tool_error("Failed to save report to file: " + std::string(e.what()));
        }
    }

    if (format == "json") {
        return tool_text_response(dump_result);
    }

    Json resp = Json::object({
        {"status", Json("success")},
        {"mode", Json(mode)},
        {"format", Json(format)},
        {"totalScanned", dump_result.get("totalScanned")},
        {"htmlLength", Json(static_cast<int64_t>(html_content.size()))},
        {"html", Json(html_content)}
    });
    if (format == "both") {
        resp.as_object()["data"] = dump_result;
    }
    return tool_text_response(resp);
}

} // namespace realplus
