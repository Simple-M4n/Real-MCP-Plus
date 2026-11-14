#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;
    using Variant = std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object>;

    Variant data;

    Json() : data(nullptr) {}
    Json(std::nullptr_t) : data(nullptr) {}
    Json(bool b) : data(b) {}
    Json(int i) : data(static_cast<int64_t>(i)) {}
    Json(unsigned int i) : data(static_cast<int64_t>(i)) {}
    Json(int64_t i) : data(i) {}
    Json(uint64_t i) : data(static_cast<int64_t>(i)) {}
    Json(double d) : data(d) {}
    Json(const char* s) : data(s ? std::string(s) : std::string()) {}
    Json(std::string s) : data(std::move(s)) {}
    Json(Array a) : data(std::move(a)) {}
    Json(Object o) : data(std::move(o)) {}

    static Json null() {
        return Json(nullptr);
    }

    static Json array(std::initializer_list<Json> init = {}) {
        return Json(Array(init));
    }

    static Json object(std::initializer_list<std::pair<const std::string, Json>> init = {}) {
        return Json(Object(init));
    }

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_int() const { return std::holds_alternative<int64_t>(data); }
    bool is_double() const { return std::holds_alternative<double>(data); }
    bool is_number() const { return is_int() || is_double(); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_array() const { return std::holds_alternative<Array>(data); }
    bool is_object() const { return std::holds_alternative<Object>(data); }

    bool as_bool(bool def = false) const {
        return is_bool() ? std::get<bool>(data) : def;
    }

    int64_t as_int(int64_t def = 0) const {
        if (is_int()) return std::get<int64_t>(data);
        if (is_double()) return static_cast<int64_t>(std::get<double>(data));
        return def;
    }

    double as_double(double def = 0.0) const {
        if (is_double()) return std::get<double>(data);
        if (is_int()) return static_cast<double>(std::get<int64_t>(data));
        return def;
    }

    const std::string& as_string() const {
        static const std::string empty;
        return is_string() ? std::get<std::string>(data) : empty;
    }

    std::string as_string(const std::string& def) const {
        return is_string() ? std::get<std::string>(data) : def;
    }

    std::string as_string_lossy() const {
        if (is_string()) return std::get<std::string>(data);
        if (is_int()) return std::to_string(std::get<int64_t>(data));
        if (is_double()) return std::to_string(std::get<double>(data));
        if (is_bool()) return std::get<bool>(data) ? "true" : "false";
        return "";
    }

    const Array& as_array() const {
        static const Array empty;
        return is_array() ? std::get<Array>(data) : empty;
    }

    Array& as_array() { return as_array_mut(); }

    Array& as_array_mut() {
        if (!is_array()) data = Array{};
        return std::get<Array>(data);
    }

    const Object& as_object() const {
        static const Object empty;
        return is_object() ? std::get<Object>(data) : empty;
    }

    Object& as_object() { return as_object_mut(); }

    Object& as_object_mut() {
        if (!is_object()) data = Object{};
        return std::get<Object>(data);
    }

    bool contains(const std::string& key) const {
        return is_object() && std::get<Object>(data).count(key) > 0;
    }

    const Json& get(const std::string& key) const {
        static const Json null_json;
        if (!is_object()) return null_json;
        const auto& obj = std::get<Object>(data);
        auto it = obj.find(key);
        return it != obj.end() ? it->second : null_json;
    }

    Json& operator[](const std::string& key) {
        return as_object_mut()[key];
    }

    void push_back(Json val) {
        as_array_mut().push_back(std::move(val));
    }

    std::string dump(int indent = -1, int depth = 0) const {
        if (is_null()) return "null";
        if (is_bool()) return std::get<bool>(data) ? "true" : "false";
        if (is_int()) return std::to_string(std::get<int64_t>(data));
        if (is_double()) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.15g", std::get<double>(data));
            return buf;
        }
        if (is_string()) return escape_string(std::get<std::string>(data));

        if (is_array()) {
            const auto& arr = std::get<Array>(data);
            if (arr.empty()) return "[]";
            std::string out = "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) out += ",";
                if (indent >= 0) {
                    out += "\n" + std::string((depth + 1) * indent, ' ');
                }
                out += arr[i].dump(indent, depth + 1);
            }
            if (indent >= 0) {
                out += "\n" + std::string(depth * indent, ' ');
            }
            out += "]";
            return out;
        }

        const auto& obj = std::get<Object>(data);
        if (obj.empty()) return "{}";
        std::string out = "{";
        bool first = true;
        for (const auto& [k, v] : obj) {
            if (!first) out += ",";
            first = false;
            if (indent >= 0) {
                out += "\n" + std::string((depth + 1) * indent, ' ');
            }
            out += escape_string(k);
            out += (indent >= 0) ? ": " : ":";
            out += v.dump(indent, depth + 1);
        }
        if (indent >= 0) {
            out += "\n" + std::string(depth * indent, ' ');
        }
        out += "}";
        return out;
    }

    static bool try_parse(const std::string& src, Json& out) {
        size_t pos = 0;
        if (!parse_value(src, pos, out)) return false;
        skip_ws(src, pos);
        return pos == src.size();
    }

    static Json parse_or_string(const std::string& src) {
        Json out;
        if (try_parse(src, out)) return out;
        return Json(src);
    }

private:
    static std::string escape_string(const std::string& s) {
        std::string out = "\"";
        out.reserve(s.size() + 4);
        for (unsigned char c : s) {
            switch (c) {
                case '\"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out.push_back(static_cast<char>(c));
                    }
            }
        }
        out.push_back('\"');
        return out;
    }

    static void skip_ws(const std::string& s, size_t& pos) {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) {
            ++pos;
        }
    }

    static bool parse_value(const std::string& s, size_t& pos, Json& out) {
        skip_ws(s, pos);
        if (pos >= s.size()) return false;
        char c = s[pos];
        if (c == 'n' && s.compare(pos, 4, "null") == 0) {
            pos += 4;
            out = Json(nullptr);
            return true;
        }
        if (c == 't' && s.compare(pos, 4, "true") == 0) {
            pos += 4;
            out = Json(true);
            return true;
        }
        if (c == 'f' && s.compare(pos, 5, "false") == 0) {
            pos += 5;
            out = Json(false);
            return true;
        }
        if (c == '\"') {
            std::string str;
            if (!parse_string(s, pos, str)) return false;
            out = Json(std::move(str));
            return true;
        }
        if (c == '[') return parse_array(s, pos, out);
        if (c == '{') return parse_object(s, pos, out);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parse_number(s, pos, out);
        }
        return false;
    }

    static void append_utf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    static bool parse_hex4(const std::string& s, size_t& pos, uint32_t& val) {
        if (pos + 4 > s.size()) return false;
        val = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s[pos++];
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
            else return false;
        }
        return true;
    }

    static bool parse_string(const std::string& s, size_t& pos, std::string& out) {
        if (pos >= s.size() || s[pos] != '\"') return false;
        ++pos;
        out.clear();
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '\"') return true;
            if (c == '\\') {
                if (pos >= s.size()) return false;
                char esc = s[pos++];
                switch (esc) {
                    case '\"': out.push_back('\"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parse_hex4(s, pos, cp)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos + 2 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                            size_t saved = pos;
                            pos += 2;
                            uint32_t lo = 0;
                            if (parse_hex4(s, pos, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00));
                            } else {
                                pos = saved;
                            }
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: return false;
                }
            } else {
                out.push_back(c);
            }
        }
        return false;
    }

    static bool parse_number(const std::string& s, size_t& pos, Json& out) {
        size_t start = pos;
        if (s[pos] == '-') ++pos;
        bool is_float = false;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        if (pos < s.size() && s[pos] == '.') {
            is_float = true;
            ++pos;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            is_float = true;
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        std::string num_str = s.substr(start, pos - start);
        if (num_str.empty() || num_str == "-") return false;
        if (is_float) {
            out = Json(std::strtod(num_str.c_str(), nullptr));
        } else {
            out = Json(static_cast<int64_t>(std::strtoll(num_str.c_str(), nullptr, 10)));
        }
        return true;
    }

    static bool parse_array(const std::string& s, size_t& pos, Json& out) {
        ++pos;
        Array arr;
        skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            out = Json(std::move(arr));
            return true;
        }
        for (;;) {
            Json elem;
            if (!parse_value(s, pos, elem)) return false;
            arr.push_back(std::move(elem));
            skip_ws(s, pos);
            if (pos >= s.size()) return false;
            if (s[pos] == ']') {
                ++pos;
                out = Json(std::move(arr));
                return true;
            }
            if (s[pos] != ',') return false;
            ++pos;
        }
    }

    static bool parse_object(const std::string& s, size_t& pos, Json& out) {
        ++pos;
        Object obj;
        skip_ws(s, pos);
        if (pos < s.size() && s[pos] == '}') {
            ++pos;
            out = Json(std::move(obj));
            return true;
        }
        for (;;) {
            skip_ws(s, pos);
            std::string key;
            if (!parse_string(s, pos, key)) return false;
            skip_ws(s, pos);
            if (pos >= s.size() || s[pos] != ':') return false;
            ++pos;
            Json val;
            if (!parse_value(s, pos, val)) return false;
            obj[std::move(key)] = std::move(val);
            skip_ws(s, pos);
            if (pos >= s.size()) return false;
            if (s[pos] == '}') {
                ++pos;
                out = Json(std::move(obj));
                return true;
            }
            if (s[pos] != ',') return false;
            ++pos;
        }
    }
};
