#include "tools.hpp"
#include "payloads_gen.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace realplus {

static std::string unescape_lua_string(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == 'n') { out.push_back('\n'); i += 1; }
            else if (next == 'r') { out.push_back('\r'); i += 1; }
            else if (next == 't') { out.push_back('\t'); i += 1; }
            else if (next == '\\') { out.push_back('\\'); i += 1; }
            else if (next == '"') { out.push_back('"'); i += 1; }
            else if (next == '\'') { out.push_back('\''); i += 1; }
            else if (next == 'a') { out.push_back('\a'); i += 1; }
            else if (next == 'b') { out.push_back('\b'); i += 1; }
            else if (next == 'v') { out.push_back('\v'); i += 1; }
            else if (next == 'x' && i + 3 < s.size() && std::isxdigit(s[i + 2]) && std::isxdigit(s[i + 3])) {
                std::string hex = s.substr(i + 2, 2);
                int val = std::stoi(hex, nullptr, 16);
                out.push_back(static_cast<char>(val));
                i += 3;
            } else if (std::isdigit(next)) {
                size_t len = 0;
                while (len < 3 && i + 1 + len < s.size() && std::isdigit(s[i + 1 + len])) {
                    len++;
                }
                std::string digits = s.substr(i + 1, len);
                int val = std::stoi(digits);
                out.push_back(static_cast<char>(val));
                i += len;
            } else {
                out.push_back(next);
                i += 1;
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static std::vector<std::string> extract_urls(const std::string& s) {
    std::vector<std::string> urls;
    std::unordered_set<std::string> seen;
    static const std::regex url_re(R"(https?:\/\/[a-zA-Z0-9\-\._~:\/\?#\[\]@!$&'\(\)\*\+,;=%]+)");
    for (std::sregex_iterator it(s.begin(), s.end(), url_re), end; it != end; ++it) {
        std::string u = it->str();
        while (!u.empty() && (u.back() == ')' || u.back() == ']' || u.back() == '"' || u.back() == '\'')) {
            u.pop_back();
        }
        if (!u.empty() && seen.insert(u).second) {
            urls.push_back(u);
        }
    }
    return urls;
}

static std::vector<std::string> extract_printable_strings(const std::string& data, size_t min_len = 3) {
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    std::string cur;
    for (unsigned char c : data) {
        if (c >= 32 && c <= 126) {
            cur.push_back(static_cast<char>(c));
        } else {
            if (cur.size() >= min_len) {
                if (seen.insert(cur).second) {
                    out.push_back(cur);
                }
            }
            cur.clear();
        }
    }
    if (cur.size() >= min_len && seen.insert(cur).second) {
        out.push_back(cur);
    }
    return out;
}

static std::string fold_string_concats(std::string code) {
    bool changed = true;
    int limit = 20;
    while (changed && limit-- > 0) {
        changed = false;
        static const std::regex concat_re(R"lit("([^"\\]*(?:\\.[^"\\]*)*)"\s*\.\.\s*"([^"\\]*(?:\\.[^"\\]*)*)")lit");
        std::string out;
        std::sregex_iterator begin(code.begin(), code.end(), concat_re);
        std::sregex_iterator end;
        if (begin == end) break;

        size_t last_pos = 0;
        for (auto it = begin; it != end; ++it) {
            out.append(code, last_pos, it->position() - last_pos);
            std::string s1 = it->str(1);
            std::string s2 = it->str(2);
            out += "\"" + s1 + s2 + "\"";
            last_pos = it->position() + it->length();
            changed = true;
        }
        out.append(code, last_pos, code.size() - last_pos);
        code = std::move(out);
    }
    return code;
}

static std::string fold_string_char(std::string code, std::vector<std::string>& out_extracted) {
    static const std::regex char_re(R"(string\.char\s*\(\s*([\d\s,]+)\s*\))");
    std::string out;
    std::sregex_iterator begin(code.begin(), code.end(), char_re);
    std::sregex_iterator end;
    if (begin == end) return code;

    size_t last_pos = 0;
    for (auto it = begin; it != end; ++it) {
        out.append(code, last_pos, it->position() - last_pos);
        std::string args_str = it->str(1);
        std::stringstream ss(args_str);
        std::string item;
        std::string decoded;
        while (std::getline(ss, item, ',')) {
            while (!item.empty() && std::isspace(item.front())) item.erase(0, 1);
            while (!item.empty() && std::isspace(item.back())) item.pop_back();
            if (!item.empty()) {
                try {
                    int c = std::stoi(item);
                    if (c >= 0 && c <= 255) {
                        decoded.push_back(static_cast<char>(c));
                    }
                } catch (...) {}
            }
        }
        if (!decoded.empty()) {
            out_extracted.push_back(decoded);
            std::string escaped = "\"";
            for (char ch : decoded) {
                if (ch == '"') escaped += "\\\"";
                else if (ch == '\\') escaped += "\\\\";
                else if (ch == '\n') escaped += "\\n";
                else if (ch == '\r') escaped += "\\r";
                else if (ch == '\t') escaped += "\\t";
                else if (static_cast<unsigned char>(ch) < 32 || static_cast<unsigned char>(ch) >= 127) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\%03u", static_cast<unsigned char>(ch));
                    escaped += buf;
                } else {
                    escaped.push_back(ch);
                }
            }
            escaped += "\"";
            out += escaped;
        } else {
            out += it->str();
        }
        last_pos = it->position() + it->length();
    }
    out.append(code, last_pos, code.size() - last_pos);
    return out;
}

// -------------------------------------------------------------
// WeAreDevs Obfuscator Decoder (https://wearedevs.net/obfuscator)
// -------------------------------------------------------------
struct WeAreDevsResult {
    bool detected = false;
    std::string version = "1.0.0";
    std::string primaryPayload;
    std::vector<std::string> decodedStrings;
    std::vector<std::string> extractedUrls;
    std::string reconstructedCode;
};

static WeAreDevsResult decode_wearedevs_script(const std::string& src) {
    WeAreDevsResult res;
    if (src.find("wearedevs.net/obfuscator") != std::string::npos ||
        src.find("WeAreDevs") != std::string::npos ||
        (src.find("return(function(") != std::string::npos && src.find("{\"1") != std::string::npos)) {
        res.detected = true;
    }

    static const std::regex tbl_re(R"(local\s+([A-Za-z0-9_]+)\s*=\s*\{\s*("1[^}]*")\s*\})");
    std::smatch tbl_m;
    std::string table_var;
    std::string table_content;

    if (std::regex_search(src, tbl_m, tbl_re)) {
        res.detected = true;
        table_var = tbl_m[1].str();
        table_content = tbl_m[2].str();
    } else {
        size_t b_start = src.find("{\"1");
        if (b_start == std::string::npos) b_start = src.find("{ \"1");
        if (b_start != std::string::npos) {
            size_t b_end = src.find("}", b_start);
            if (b_end != std::string::npos) {
                res.detected = true;
                table_content = src.substr(b_start + 1, b_end - b_start - 1);
            }
        }
    }

    if (!res.detected && !table_content.empty()) {
        res.detected = true;
    }

    if (!table_content.empty()) {
        static const std::regex str_item_re(R"lit("([^"\\]*(?:\\.[^"\\]*)*)")lit");
        std::sregex_iterator sit(table_content.begin(), table_content.end(), str_item_re);
        std::sregex_iterator send;
        for (; sit != send; ++sit) {
            std::string raw = sit->str(1);
            std::string unescaped = unescape_lua_string(raw);
            if (!unescaped.empty() && unescaped.front() == '1') {
                unescaped = unescaped.substr(1);
            }
            if (!unescaped.empty()) {
                res.decodedStrings.push_back(unescaped);
                if (unescaped.size() > res.primaryPayload.size() &&
                    (unescaped.find("game") != std::string::npos ||
                     unescaped.find("function") != std::string::npos ||
                     unescaped.find("local") != std::string::npos ||
                     unescaped.find("http") != std::string::npos ||
                     unescaped.size() > 50)) {
                    res.primaryPayload = unescaped;
                }
            }
        }
    }

    for (const auto& s : res.decodedStrings) {
        auto urls = extract_urls(s);
        res.extractedUrls.insert(res.extractedUrls.end(), urls.begin(), urls.end());
    }

    if (!res.primaryPayload.empty()) {
        res.reconstructedCode = res.primaryPayload;
    } else if (!table_var.empty() && !res.decodedStrings.empty()) {
        std::string replaced = src;
        for (size_t i = 0; i < res.decodedStrings.size(); ++i) {
            std::string pattern = table_var + "[" + std::to_string(i + 1) + "]";
            std::string rep = "\"" + res.decodedStrings[i] + "\"";
            replaced = replace_all(std::move(replaced), pattern, rep);
        }
        res.reconstructedCode = fold_string_concats(replaced);
    } else {
        res.reconstructedCode = src;
    }

    return res;
}

// -------------------------------------------------------------
// Prometheus V2 Deobfuscator Pipeline (0x251/Prometheus-DeobfuscatorV2)
// -------------------------------------------------------------
struct PrometheusResult {
    bool detected = false;
    std::string confidence = "low";
    int constantArraysCount = 0;
    int foldedConstants = 0;
    std::vector<std::string> appliedPasses;
    std::vector<std::string> decodedStrings;
    std::vector<std::string> extractedUrls;
    std::string reconstructedCode;
};

static PrometheusResult decode_prometheus_script(const std::string& src) {
    PrometheusResult res;
    int score = 0;
    if (src.find("Prometheus") != std::string::npos) score += 3;
    if (src.find("bit32") != std::string::npos || src.find("bit.bxor") != std::string::npos) score += 1;
    if (src.find("getfenv") != std::string::npos) score += 1;
    if (src.find("newproxy") != std::string::npos) score += 1;

    static const std::regex const_arr_re(R"(local\s+([A-Za-z0-9_]+)\s*=\s*\{\s*([0-9xXa-fA-F\-,\s]+)\s*\})");
    std::sregex_iterator ait(src.begin(), src.end(), const_arr_re);
    std::sregex_iterator aend;
    std::unordered_map<std::string, std::vector<std::string>> const_tables;

    for (; ait != aend; ++ait) {
        score += 2;
        res.constantArraysCount++;
        std::string tbl_name = ait->str(1);
        std::string elements_raw = ait->str(2);
        std::stringstream ss(elements_raw);
        std::string elem;
        std::vector<std::string> items;
        while (std::getline(ss, elem, ',')) {
            while (!elem.empty() && std::isspace(elem.front())) elem.erase(0, 1);
            while (!elem.empty() && std::isspace(elem.back())) elem.pop_back();
            if (!elem.empty()) items.push_back(elem);
        }
        if (!items.empty()) {
            const_tables[tbl_name] = std::move(items);
        }
    }

    if (score >= 2) {
        res.detected = true;
        res.confidence = (score >= 4) ? "high" : "medium";
    }

    std::string working = src;

    // Pass 1: ConstantArrayDecode
    if (!const_tables.empty()) {
        res.appliedPasses.push_back("ConstantArrayDecode");
        for (const auto& [name, items] : const_tables) {
            for (size_t i = 0; i < items.size(); ++i) {
                std::string target = name + "[" + std::to_string(i + 1) + "]";
                if (working.find(target) != std::string::npos) {
                    working = replace_all(std::move(working), target, items[i]);
                    res.foldedConstants++;
                }
            }
        }
    }

    // Pass 2: FoldConcats
    std::string folded_concats = fold_string_concats(working);
    if (folded_concats != working) {
        res.appliedPasses.push_back("FoldConcats");
        working = std::move(folded_concats);
    }

    // Pass 3: Fold string.char and decode string escapes
    std::vector<std::string> char_strings;
    std::string folded_chars = fold_string_char(working, char_strings);
    if (!char_strings.empty()) {
        res.appliedPasses.push_back("UndoEncryptStrings");
        working = std::move(folded_chars);
        res.decodedStrings.insert(res.decodedStrings.end(), char_strings.begin(), char_strings.end());
    }

    // Pass 4: UndoSplitStrings
    std::string final_folded = fold_string_concats(working);
    if (final_folded != working) {
        res.appliedPasses.push_back("UndoSplitStrings");
        working = std::move(final_folded);
    }

    res.extractedUrls = extract_urls(working);
    res.reconstructedCode = working;

    return res;
}

// -------------------------------------------------------------
// IronBrew 2 (IB2) Deobfuscator Pipeline
// -------------------------------------------------------------
struct IronBrewResult {
    bool detected = false;
    std::string version = "IB2 / 2.7.x";
    std::string confidence = "low";
    std::vector<std::string> appliedPasses;
    std::vector<std::string> decodedStrings;
    std::vector<std::string> extractedUrls;
    std::vector<std::string> extractedGlobals;
    std::vector<std::string> extractedServices;
    std::string reconstructedCode;
};

static IronBrewResult decode_ironbrew_script(const std::string& src) {
    IronBrewResult res;
    int score = 0;
    if (src.find("IronBrew") != std::string::npos ||
        src.find("Ironbrew") != std::string::npos ||
        src.find("ironbrew") != std::string::npos ||
        src.find("This file was protected using IronBrew") != std::string::npos) {
        score += 4;
        res.version = "IronBrew 2 / Luraph Legacy";
    }
    if (src.find("bit32") != std::string::npos || src.find("bit.band") != std::string::npos || src.find("bit.rshift") != std::string::npos) {
        score += 1;
    }
    if (src.find("while true do") != std::string::npos || src.find("repeat until") != std::string::npos) {
        score += 1;
    }
    if (src.find("gBit") != std::string::npos || src.find("gBits") != std::string::npos || src.find("gInt") != std::string::npos || src.find("gString") != std::string::npos) {
        score += 3;
    }

    if (score >= 3) {
        res.detected = true;
        res.confidence = (score >= 4) ? "high" : "medium";
    }

    res.appliedPasses.push_back("IronBrewSignatureDetect");

    // Extract all string literals (the bytecode chunk and string constants)
    static const std::regex str_literal_re(R"lit("([^"\\]*(?:\\.[^"\\]*)*)")lit");
    std::sregex_iterator sit(src.begin(), src.end(), str_literal_re);
    std::sregex_iterator send;
    std::string largest_chunk;

    for (; sit != send; ++sit) {
        std::string raw = sit->str(1);
        std::string unescaped = unescape_lua_string(raw);
        if (unescaped.size() > largest_chunk.size()) {
            largest_chunk = unescaped;
        }
        auto printables = extract_printable_strings(unescaped, 3);
        for (const auto& s : printables) {
            res.decodedStrings.push_back(s);
        }
    }

    res.appliedPasses.push_back("BytecodeChunkExtract");
    res.appliedPasses.push_back("ConstantPoolUnpack");

    // Classify IOCs, Services, Globals, and Remotes
    static const std::unordered_set<std::string> known_services = {
        "Players", "ReplicatedStorage", "Workspace", "RunService", "HttpService",
        "StarterGui", "UserInputService", "TweenService", "Lighting", "CoreGui"
    };
    static const std::unordered_set<std::string> known_globals = {
        "print", "warn", "loadstring", "HttpGet", "HttpPost", "setclipboard",
        "getgenv", "getfenv", "hookmetamethod", "hookfunction", "fireclickdetector"
    };

    std::unordered_set<std::string> seen_str;
    for (const auto& s : res.decodedStrings) {
        if (!seen_str.insert(s).second) continue;

        if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0) {
            res.extractedUrls.push_back(s);
        } else if (known_services.count(s)) {
            res.extractedServices.push_back(s);
        } else if (known_globals.count(s)) {
            res.extractedGlobals.push_back(s);
        }
    }

    // Build human-readable reconstructed summary
    std::ostringstream recon;
    recon << "-- [[ IronBrew 2 VM Deobfuscation Analysis ]]\n";
    recon << "-- Protected Version: " << res.version << "\n";
    recon << "-- Bytecode Chunk Size: " << largest_chunk.size() << " bytes\n\n";

    if (!res.extractedUrls.empty()) {
        recon << "-- [Extracted Network Endpoints / Webhooks]\n";
        for (const auto& u : res.extractedUrls) {
            recon << "-- " << u << "\n";
        }
        recon << "\n";
    }

    if (!res.extractedServices.empty()) {
        recon << "-- [Referenced Game Services]\n";
        for (const auto& s : res.extractedServices) {
            recon << "local " << s << " = game:GetService(\"" << s << "\")\n";
        }
        recon << "\n";
    }

    if (!res.extractedGlobals.empty()) {
        recon << "-- [Referenced Globals / APIs]\n";
        recon << "-- ";
        for (size_t i = 0; i < res.extractedGlobals.size(); ++i) {
            recon << res.extractedGlobals[i] << (i + 1 < res.extractedGlobals.size() ? ", " : "\n\n");
        }
    }

    recon << "-- [Constant String Pool (" << res.decodedStrings.size() << " strings unpacked)]\n";
    size_t preview_limit = std::min<size_t>(res.decodedStrings.size(), 30);
    for (size_t i = 0; i < preview_limit; ++i) {
        recon << "-- [" << (i + 1) << "] " << res.decodedStrings[i] << "\n";
    }
    if (res.decodedStrings.size() > preview_limit) {
        recon << "-- ... (" << (res.decodedStrings.size() - preview_limit) << " more strings)\n";
    }

    res.reconstructedCode = recon.str();
    return res;
}

// -------------------------------------------------------------
// MoonSec V2 / V3 Deobfuscator Pipeline
// -------------------------------------------------------------
struct MoonSecResult {
    bool detected = false;
    std::string version = "MoonSec V2/V3";
    std::string confidence = "low";
    uint8_t recoveredXorKey = 0;
    std::vector<std::string> appliedPasses;
    std::vector<std::string> decodedStrings;
    std::vector<std::string> extractedUrls;
    std::string reconstructedCode;
};

static MoonSecResult decode_moonsec_script(const std::string& src) {
    MoonSecResult res;
    int score = 0;
    if (src.find("MoonSec") != std::string::npos ||
        src.find("Moonsec") != std::string::npos ||
        src.find("moonsec") != std::string::npos) {
        score += 4;
        if (src.find("V3") != std::string::npos || src.find("v3") != std::string::npos) {
            res.version = "MoonSec V3";
        } else {
            res.version = "MoonSec V2";
        }
    }
    if (src.find("bxor") != std::string::npos || src.find("~") != std::string::npos) {
        score += 1;
    }
    if (src.find("\\") != std::string::npos && (src.find("return(") != std::string::npos || src.find("(function(") != std::string::npos)) {
        score += 1;
    }

    if (score >= 2) {
        res.detected = true;
        res.confidence = (score >= 4) ? "high" : "medium";
    }

    res.appliedPasses.push_back("MoonSecSignatureDetect");

    // Extract the primary encrypted byte chunk
    static const std::regex str_literal_re(R"lit("([^"\\]*(?:\\.[^"\\]*)*)")lit");
    std::sregex_iterator sit(src.begin(), src.end(), str_literal_re);
    std::sregex_iterator send;
    std::string largest_chunk;

    for (; sit != send; ++sit) {
        std::string raw = sit->str(1);
        std::string unescaped = unescape_lua_string(raw);
        if (unescaped.size() > largest_chunk.size()) {
            largest_chunk = std::move(unescaped);
        }
    }

    // Recover XOR / Byte encryption key by heuristic frequency analysis
    uint8_t best_key = 0;
    size_t max_ascii_score = 0;

    if (!largest_chunk.empty()) {
        for (int k = 1; k < 256; ++k) {
            size_t ascii_count = 0;
            size_t sample_size = std::min<size_t>(largest_chunk.size(), 400);
            for (size_t i = 0; i < sample_size; ++i) {
                unsigned char dec = static_cast<unsigned char>(largest_chunk[i]) ^ static_cast<unsigned char>(k);
                if ((dec >= 'a' && dec <= 'z') || (dec >= 'A' && dec <= 'Z') || (dec >= '0' && dec <= '9') || dec == '_' || dec == '/' || dec == ':') {
                    ascii_count++;
                }
            }
            if (ascii_count > max_ascii_score) {
                max_ascii_score = ascii_count;
                best_key = static_cast<uint8_t>(k);
            }
        }
    }

    std::string decrypted_payload = largest_chunk;
    if (max_ascii_score > 30) {
        res.recoveredXorKey = best_key;
        res.appliedPasses.push_back("XorKeyRecovery");
        for (size_t i = 0; i < decrypted_payload.size(); ++i) {
            decrypted_payload[i] = static_cast<char>(static_cast<unsigned char>(decrypted_payload[i]) ^ best_key);
        }
        res.appliedPasses.push_back("BytecodeDecrypt");
    }

    // Extract printable strings from both raw and decrypted chunks
    auto strings_dec = extract_printable_strings(decrypted_payload, 3);
    auto strings_raw = extract_printable_strings(largest_chunk, 3);

    std::unordered_set<std::string> seen;
    for (const auto& s : strings_dec) {
        if (seen.insert(s).second) res.decodedStrings.push_back(s);
    }
    for (const auto& s : strings_raw) {
        if (seen.insert(s).second) res.decodedStrings.push_back(s);
    }

    res.appliedPasses.push_back("ConstantPoolUnpack");

    for (const auto& s : res.decodedStrings) {
        auto urls = extract_urls(s);
        res.extractedUrls.insert(res.extractedUrls.end(), urls.begin(), urls.end());
    }

    std::ostringstream recon;
    recon << "-- [[ MoonSec Deobfuscation Analysis ]]\n";
    recon << "-- Version: " << res.version << "\n";
    if (res.recoveredXorKey != 0) {
        recon << "-- Recovered XOR Decryption Key: 0x" << std::hex << static_cast<int>(res.recoveredXorKey) << std::dec << "\n";
    }
    recon << "-- Encrypted Bytecode Size: " << largest_chunk.size() << " bytes\n\n";

    if (!res.extractedUrls.empty()) {
        recon << "-- [Extracted Network Endpoints / Webhooks]\n";
        for (const auto& u : res.extractedUrls) {
            recon << "-- " << u << "\n";
        }
        recon << "\n";
    }

    recon << "-- [Unpacked Constants & Identifiers (" << res.decodedStrings.size() << " strings)]\n";
    size_t preview_limit = std::min<size_t>(res.decodedStrings.size(), 30);
    for (size_t i = 0; i < preview_limit; ++i) {
        recon << "-- [" << (i + 1) << "] " << res.decodedStrings[i] << "\n";
    }
    if (res.decodedStrings.size() > preview_limit) {
        recon << "-- ... (" << (res.decodedStrings.size() - preview_limit) << " more strings)\n";
    }

    res.reconstructedCode = recon.str();
    return res;
}

// -------------------------------------------------------------
// Generic Deobfuscator
// -------------------------------------------------------------
static Json decode_generic_script(const std::string& src) {
    std::vector<std::string> extracted_strings;
    std::string working = fold_string_char(src, extracted_strings);
    working = fold_string_concats(working);

    auto urls = extract_urls(working);

    std::vector<std::string> hex_strings;
    static const std::regex hex_re(R"(\\x([0-9a-fA-F]{2}))");
    for (std::sregex_iterator it(working.begin(), working.end(), hex_re), end; it != end; ++it) {
        int val = std::stoi(it->str(1), nullptr, 16);
        if (val >= 32 && val <= 126) {
            std::string s(1, static_cast<char>(val));
            hex_strings.push_back(s);
        }
    }

    Json obj = Json::object();
    obj.as_object()["reconstructedCode"] = Json(working);
    Json str_arr = Json::array();
    for (const auto& s : extracted_strings) str_arr.as_array().push_back(Json(s));
    obj.as_object()["extractedStrings"] = std::move(str_arr);

    Json url_arr = Json::array();
    for (const auto& u : urls) url_arr.as_array().push_back(Json(u));
    obj.as_object()["extractedUrls"] = std::move(url_arr);

    return obj;
}

// -------------------------------------------------------------
// simpledeobf Tool Entrypoints
// -------------------------------------------------------------
Json run_simpledeobf_opt([[maybe_unused]] RealBridge* real, const Json& args) {
    std::string script = args.get("script").as_string("");
    if (script.empty()) {
        return tool_error("Argument 'script' must not be empty");
    }

    std::string mode = args.get("mode").as_string("auto");
    bool extract_strings = args.get("extractStrings").as_bool(true);
    bool extract_urls_opt = args.get("extractUrls").as_bool(true);

    std::string chosen_mode = mode;
    std::string detected_type = "Generic / Unknown";
    std::string confidence = "low";
    std::string deobf_code;
    std::vector<std::string> passes;
    std::vector<std::string> all_strings;
    std::vector<std::string> all_urls;
    Json security_report = Json::object();
    Json extra_metadata = Json::object();

    if (mode == "auto" || mode == "wearedevs") {
        WeAreDevsResult wd = decode_wearedevs_script(script);
        if (wd.detected || mode == "wearedevs") {
            detected_type = "WeAreDevs Luau Obfuscator v" + wd.version;
            confidence = "high";
            chosen_mode = "wearedevs";
            passes.push_back("WeAreDevsTableDecode");
            passes.push_back("StringPrefixStrip");
            passes.push_back("FoldConcats");
            deobf_code = wd.reconstructedCode;
            all_strings = std::move(wd.decodedStrings);
            all_urls = std::move(wd.extractedUrls);
        }
    }

    if (detected_type == "Generic / Unknown" && (mode == "auto" || mode == "prometheus")) {
        PrometheusResult pr = decode_prometheus_script(script);
        if (pr.detected || mode == "prometheus") {
            detected_type = "Prometheus V2 / Pipeline";
            confidence = pr.confidence;
            chosen_mode = "prometheus";
            passes = std::move(pr.appliedPasses);
            deobf_code = pr.reconstructedCode;
            all_strings = std::move(pr.decodedStrings);
            all_urls = std::move(pr.extractedUrls);
        }
    }

    if (detected_type == "Generic / Unknown" && (mode == "auto" || mode == "ironbrew")) {
        IronBrewResult ib = decode_ironbrew_script(script);
        if (ib.detected || mode == "ironbrew") {
            detected_type = ib.version;
            confidence = ib.confidence;
            chosen_mode = "ironbrew";
            passes = std::move(ib.appliedPasses);
            deobf_code = ib.reconstructedCode;
            all_strings = std::move(ib.decodedStrings);
            all_urls = std::move(ib.extractedUrls);
            Json s_arr = Json::array();
            for (const auto& s : ib.extractedServices) s_arr.as_array().push_back(Json(s));
            extra_metadata.as_object()["referencedServices"] = std::move(s_arr);
            Json g_arr = Json::array();
            for (const auto& g : ib.extractedGlobals) g_arr.as_array().push_back(Json(g));
            extra_metadata.as_object()["referencedGlobals"] = std::move(g_arr);
        }
    }

    if (detected_type == "Generic / Unknown" && (mode == "auto" || mode == "moonsec")) {
        MoonSecResult ms = decode_moonsec_script(script);
        if (ms.detected || mode == "moonsec") {
            detected_type = ms.version;
            confidence = ms.confidence;
            chosen_mode = "moonsec";
            passes = std::move(ms.appliedPasses);
            deobf_code = ms.reconstructedCode;
            all_strings = std::move(ms.decodedStrings);
            all_urls = std::move(ms.extractedUrls);
            if (ms.recoveredXorKey != 0) {
                extra_metadata.as_object()["recoveredXorKey"] = Json(static_cast<int64_t>(ms.recoveredXorKey));
            }
        }
    }

    if (detected_type == "Generic / Unknown") {
        chosen_mode = "generic";
        Json gen = decode_generic_script(script);
        deobf_code = gen.get("reconstructedCode").as_string(script);
        passes.push_back("FoldStringChar");
        passes.push_back("FoldConcats");
        for (const auto& item : gen.get("extractedStrings").as_array()) {
            all_strings.push_back(item.as_string(""));
        }
        for (const auto& item : gen.get("extractedUrls").as_array()) {
            all_urls.push_back(item.as_string(""));
        }
    }

    // Security & IOC analysis
    Json webhooks = Json::array();
    Json pastebin = Json::array();
    for (const auto& u : all_urls) {
        if (u.find("discord.com/api/webhooks") != std::string::npos ||
            u.find("discordapp.com/api/webhooks") != std::string::npos) {
            webhooks.as_array().push_back(Json(u));
        } else if (u.find("pastebin.com") != std::string::npos ||
                   u.find("raw.githubusercontent.com") != std::string::npos) {
            pastebin.as_array().push_back(Json(u));
        }
    }
    security_report.as_object()["discordWebhooks"] = std::move(webhooks);
    security_report.as_object()["remotePayloadUrls"] = std::move(pastebin);
    security_report.as_object()["containsLoadstring"] = Json(deobf_code.find("loadstring") != std::string::npos);
    security_report.as_object()["containsHttpGet"] = Json(deobf_code.find("HttpGet") != std::string::npos);

    Json out = Json::object();
    out.as_object()["detectedType"] = Json(detected_type);
    out.as_object()["confidence"] = Json(confidence);
    out.as_object()["modeApplied"] = Json(chosen_mode);

    Json p_arr = Json::array();
    for (const auto& p : passes) p_arr.as_array().push_back(Json(p));
    out.as_object()["passesApplied"] = std::move(p_arr);

    out.as_object()["deobfuscatedSource"] = Json(deobf_code);

    if (extract_strings) {
        Json s_arr = Json::array();
        for (const auto& s : all_strings) s_arr.as_array().push_back(Json(s));
        out.as_object()["extractedStrings"] = std::move(s_arr);
    }

    if (extract_urls_opt) {
        Json u_arr = Json::array();
        for (const auto& u : all_urls) u_arr.as_array().push_back(Json(u));
        out.as_object()["extractedUrls"] = std::move(u_arr);
    }

    out.as_object()["securityAnalysis"] = std::move(security_report);
    if (!extra_metadata.as_object().empty()) {
        out.as_object()["metadata"] = std::move(extra_metadata);
    }

    return tool_text_response(out);
}

Json run_simpledeobf(RealBridge& real, const Json& args) {
    return run_simpledeobf_opt(&real, args);
}

// -------------------------------------------------------------
// dumper Tool Entrypoint
// -------------------------------------------------------------
Json run_dumper(RealBridge& real, const Json& args) {
    std::string target = args.get("target").as_string("all");
    int64_t max_depth = args.get("maxDepth").as_int(3);
    if (max_depth < 1) max_depth = 1;
    if (max_depth > 10) max_depth = 10;
    std::string save_to_file = args.get("saveToFile").as_string("");

    std::string code = payloads::LUAU_DUMPER;
    code = replace_all(std::move(code), "__TARGET__", target);
    code = replace_all(std::move(code), "__MAX_DEPTH__", std::to_string(max_depth));

    Json dump_result = eval_luau_safe(real, "dumper", code);

    if (dump_result.is_object() && dump_result.contains("error")) {
        return tool_error("Dumper eval failed: " + dump_result.get("error").as_string_lossy());
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
            std::string dumped_str = dump_result.dump(2);
            out.write(dumped_str.data(), dumped_str.size());
            out.close();

            size_t file_size = std::filesystem::file_size(p);
            Json summary = Json::object({
                {"status", Json("success")},
                {"target", Json(target)},
                {"maxDepth", Json(max_depth)},
                {"savedToFile", Json(save_to_file)},
                {"fileSizeBytes", Json(static_cast<int64_t>(file_size))},
                {"totalInstances", dump_result.get("totalInstances")},
                {"timestamp", dump_result.get("timestamp")}
            });
            return tool_text_response(summary);
        } catch (const std::exception& e) {
            return tool_error("Failed to save dump to file: " + std::string(e.what()));
        }
    }

    return tool_text_response(dump_result);
}

} // namespace realplus
