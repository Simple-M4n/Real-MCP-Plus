#pragma once

#include "json.hpp"
#include <atomic>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct win_proc;
typedef struct win_proc win_proc_t;

namespace realplus {

class RealBridge {
public:
    explicit RealBridge(const std::string& exe_path);
    ~RealBridge();

    RealBridge(const RealBridge&) = delete;
    RealBridge& operator=(const RealBridge&) = delete;

    Json request(const std::string& method, const Json& params);
    void notify(const std::string& method, const Json& params);
    Json call_tool(const std::string& name, const Json& arguments);
    bool is_alive() const { return alive_.load(); }

private:
    void reader_loop();
    void fail_all_pending(const std::string& reason);

    win_proc_t* proc_ = nullptr;
    std::atomic<bool> alive_{false};
    std::atomic<int64_t> next_id_{1};
    std::mutex write_mtx_;
    std::mutex pending_mtx_;
    std::unordered_map<int64_t, std::promise<Json>> pending_;
    std::thread reader_thread_;
};

std::string replace_all(std::string s, const std::string& from, const std::string& to);
std::string luau_str(const std::string& s);
std::string extract_tool_text(const Json& res);
Json parse_tool_json_or_raw(const Json& res);
Json eval_luau_safe(RealBridge& real, const std::string& label, const std::string& code);
Json call_optional(RealBridge& real, const std::string& tool, const Json& args);
Json grep_batch(RealBridge& real, const std::vector<std::string>& patterns, int max_matches = 25);
Json gc_const_batch(RealBridge& real, const std::vector<std::string>& constants, int max_results = 20);
Json gc_table_batch(RealBridge& real, const std::vector<std::string>& keys, int max_results = 20);

Json tool_text_response(const Json& val, bool is_error = false);
Json tool_error(const std::string& msg);

} // namespace realplus
