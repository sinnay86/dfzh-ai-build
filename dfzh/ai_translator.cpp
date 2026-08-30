// ai_translator.cpp
// 本地 Ollama AI 实时补翻译 —— DFZH 第三层翻译后端实现
#include "ai_translator.h"
#include "config.h"
#include "logger.h"

// cpp-httplib（header-only，本地 HTTP 无需 OpenSSL）
// 在 Windows 上需要链接 ws2_32（见 CMakeLists.txt）
#include <httplib.h>

// toml++（DFZH 已有依赖，用于解析配置文件）
#include <toml++/toml.hpp>

#include <string>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <filesystem>

namespace {

    // JSON 字符串转义（内部工具函数，不暴露到头文件）
    std::string jsonEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 20);
        out.push_back('"');
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out.push_back(c);
                    }
            }
        }
        out.push_back('"');
        return out;
    }

} // anonymous namespace

namespace DFHack {
namespace DFZH {
namespace Hooks {

    // =====================================================================
    // 构造 / 析构
    // =====================================================================
    AITranslator::AITranslator() {
        // 默认 prompt（若配置文件未提供则使用此默认值）
        prompt_template_ =
            "你是一个专业的英文到简体中文翻译器。"
            "请将下面的英文文本翻译成简体中文。"
            "只输出翻译结果，不要输出解释、Markdown 格式、引号或\"翻译：\"等前缀。\n\n"
            "英文文本：\n{text}\n\n"
            "简体中文翻译：";
    }

    AITranslator::~AITranslator() {
        stop();
    }

    // =====================================================================
    // 配置加载
    // =====================================================================
    void AITranslator::loadConfig() {
        try {
            auto config_path = Config::getDataPath() / "ai_translation.toml";
            if (!std::filesystem::exists(config_path)) {
                LOGGERMANAGER.getLogger()->warn(
                    "[AITranslator] config file not found: {}, using defaults.",
                    config_path.string());
                return;
            }

            auto tbl = toml::parse_file(config_path.string());

            enabled_         = tbl["ollama"]["enabled"].value_or(true);
            host_            = tbl["ollama"]["host"].value_or(std::string("127.0.0.1"));
            port_            = tbl["ollama"]["port"].value_or(11434);
            model_           = tbl["ollama"]["model"].value_or(std::string("qwen3:4b"));
            timeout_sec_     = tbl["ollama"]["timeout"].value_or(10);
            max_queue_size_  = static_cast<size_t>(tbl["ollama"]["max_queue_size"].value_or(200));

            auto prompt_opt = tbl["prompt"]["template"].value<std::string>();
            if (prompt_opt && !prompt_opt->empty()) {
                prompt_template_ = *prompt_opt;
            }

            LOGGERMANAGER.getLogger()->info(
                "[AITranslator] config loaded: enabled={}, host={}:{}, model={}, timeout={}s, max_queue={}",
                enabled_, host_, port_, model_, timeout_sec_, max_queue_size_);
        } catch (const std::exception& e) {
            LOGGERMANAGER.getLogger()->error(
                "[AITranslator] loadConfig failed: {}, using defaults.", e.what());
        }
    }

    // =====================================================================
    // 主线程接口：tryTranslate
    // =====================================================================
    bool AITranslator::tryTranslate(const std::string& input, std::string& output) {
        if (!enabled_ || input.empty()) {
            return false;
        }

        // 过短文本不交给 AI（应由词典/规则覆盖，AI 翻短句质量差且浪费资源）
        if (input.length() < 4) {
            return false;
        }

        // 1. 查 AI Cache
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = cache_.find(input);
            if (it != cache_.end()) {
                output = it->second;
                return true;
            }
        }

        // 2. 查 Pending（正在翻译中，不重复入队）
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            if (pending_.find(input) != pending_.end()) {
                return false;  // 正在翻译，这帧继续显示英文
            }
        }

        // 3. 加入 Pending + Queue（非阻塞，立即返回）
        {
            std::lock_guard<std::mutex> p_lock(pending_mutex_);
            pending_.insert(input);
        }
        {
            std::lock_guard<std::mutex> q_lock(queue_mutex_);
            if (task_queue_.size() >= max_queue_size_) {
                // 队列已满，丢弃最旧的任务，避免内存无限增长
                LOGGERMANAGER.getLogger()->warn(
                    "[AITranslator] queue full ({}), dropping oldest task.", max_queue_size_);
                task_queue_.pop();
            }
            task_queue_.push(input);
        }
        cv_.notify_one();

        return false;  // 已入队，这帧继续显示英文
    }

    // =====================================================================
    // 生命周期：start / stop
    // =====================================================================
    void AITranslator::start() {
        loadConfig();

        if (!enabled_) {
            LOGGERMANAGER.getLogger()->info("[AITranslator] disabled in config, not starting worker.");
            return;
        }

        if (running_.exchange(true)) {
            return;  // 已经在运行
        }

        worker_ = std::thread(&AITranslator::workerLoop, this);
        LOGGERMANAGER.getLogger()->info("[AITranslator] worker thread started.");
    }

    void AITranslator::stop() {
        if (!running_.exchange(false)) {
            return;  // 已经停止
        }

        cv_.notify_all();  // 唤醒 Worker，使其检查 running_ 标志

        if (worker_.joinable()) {
            // Worker 最多在 Ollama 超时（timeout_sec_）后返回，
            // join() 会等待 Worker 线程结束。插件卸载时这是安全的。
            worker_.join();
        }

        LOGGERMANAGER.getLogger()->info("[AITranslator] worker thread stopped.");
    }

    // =====================================================================
    // 新翻译完成标志
    // =====================================================================
    bool AITranslator::hasNewTranslations() const {
        return new_translations_flag_.load(std::memory_order_acquire);
    }

    void AITranslator::clearNewTranslationsFlag() {
        new_translations_flag_.store(false, std::memory_order_release);
    }

    // =====================================================================
    // Worker 线程主循环
    // =====================================================================
    void AITranslator::workerLoop() {
        LOGGERMANAGER.getLogger()->info("[AITranslator] workerLoop entering.");

        while (running_.load(std::memory_order_acquire)) {
            std::string input;

            // 从队列取任务（阻塞等待，带超时以便定期检查 running_）
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                    return !task_queue_.empty() || !running_.load(std::memory_order_acquire);
                });

                if (!running_.load(std::memory_order_acquire)) {
                    break;
                }
                if (task_queue_.empty()) {
                    continue;
                }

                input = std::move(task_queue_.front());
                task_queue_.pop();
            }

            if (input.empty()) {
                std::lock_guard<std::mutex> p_lock(pending_mutex_);
                pending_.erase(input);
                continue;
            }

            // 调 Ollama 翻译
            std::string translated;
            bool success = callOllama(input, translated);

            if (success && validateTranslation(input, translated)) {
                // 成功：写入 Cache
                {
                    std::lock_guard<std::mutex> c_lock(cache_mutex_);
                    cache_[input] = translated;
                }
                new_translations_flag_.store(true, std::memory_order_release);

                LOGGERMANAGER.getLogger()->debug(
                    "[AITranslator] translated: \"{}\" -> \"{}\"",
                    input.substr(0, 60), translated.substr(0, 60));
            } else {
                // 失败：不写 Cache，下次出现时会重新尝试（Pending 已清除）
                LOGGERMANAGER.getLogger()->warn(
                    "[AITranslator] translation failed for: \"{}\"",
                    input.substr(0, 80));
            }

            // 无论成功失败，都从 Pending 移除
            {
                std::lock_guard<std::mutex> p_lock(pending_mutex_);
                pending_.erase(input);
            }
        }

        // 线程退出前，清空 Pending（避免残留状态导致文本永远不翻译）
        {
            std::lock_guard<std::mutex> p_lock(pending_mutex_);
            pending_.clear();
        }

        LOGGERMANAGER.getLogger()->info("[AITranslator] workerLoop exiting.");
    }

    // =====================================================================
    // Ollama HTTP 调用
    // =====================================================================
    bool AITranslator::callOllama(const std::string& input, std::string& output) {
        try {
            httplib::Client cli(host_, port_);

            // 超时设置（连接超时短一点，读超时=配置的翻译超时）
            cli.set_connection_timeout(5);
            cli.set_read_timeout(timeout_sec_);
            cli.set_write_timeout(5);

            // 构造请求体（非流式）
            std::string prompt = buildPrompt(input);
            std::string body =
                "{\"model\":\"" + model_ + "\","
                "\"prompt\":" + jsonEscape(prompt) + ","
                "\"stream\":false,"
                "\"options\":{\"temperature\":0.3}}";

            auto res = cli.Post("/api/generate", body, "application/json");

            if (!res) {
                auto err = res.error();
                LOGGERMANAGER.getLogger()->warn(
                    "[AITranslator] HTTP request failed: {}", httplib::to_string(err));
                return false;
            }

            if (res->status != 200) {
                LOGGERMANAGER.getLogger()->warn(
                    "[AITranslator] HTTP status {}: {}", res->status, res->body.substr(0, 200));
                return false;
            }

            // 从 JSON 响应提取 response 字段
            std::string response_text;
            if (!extractResponseField(res->body, response_text)) {
                LOGGERMANAGER.getLogger()->warn(
                    "[AITranslator] failed to extract response field from JSON: {}",
                    res->body.substr(0, 200));
                return false;
            }

            // 去除首尾空白
            size_t start = response_text.find_first_not_of(" \t\r\n\"'");
            size_t end = response_text.find_last_not_of(" \t\r\n\"'");
            if (start == std::string::npos) {
                return false;
            }
            output = response_text.substr(start, end - start + 1);

            return !output.empty();

        } catch (const std::exception& e) {
            LOGGERMANAGER.getLogger()->error("[AITranslator] callOllama exception: {}", e.what());
            return false;
        }
    }

    // =====================================================================
    // Prompt 构造
    // =====================================================================
    std::string AITranslator::buildPrompt(const std::string& input) const {
        std::string result = prompt_template_;
        size_t pos = result.find("{text}");
        if (pos != std::string::npos) {
            result.replace(pos, 6, input);
        } else {
            result += "\n" + input;
        }
        return result;
    }

    // =====================================================================
    // 从 Ollama JSON 响应提取 "response" 字段
    // 不引入完整 JSON 库，用简单的状态机解析字符串值
    // =====================================================================
    bool AITranslator::extractResponseField(const std::string& json_body, std::string& out_response) {
        // 查找 "response" 键
        const std::string key = "\"response\"";
        size_t pos = json_body.find(key);
        if (pos == std::string::npos) {
            return false;
        }
        pos += key.length();

        // 跳过冒号和空白
        while (pos < json_body.size() && (json_body[pos] == ':' || json_body[pos] == ' ' || json_body[pos] == '\t')) {
            ++pos;
        }

        if (pos >= json_body.size() || json_body[pos] != '"') {
            return false;  // response 不是字符串类型
        }
        ++pos;  // 跳过开头引号

        // 读取字符串值，处理转义
        std::string result;
        result.reserve(256);
        while (pos < json_body.size()) {
            char c = json_body[pos];
            if (c == '\\' && pos + 1 < json_body.size()) {
                char next = json_body[pos + 1];
                switch (next) {
                    case '"':  result.push_back('"');  break;
                    case '\\': result.push_back('\\'); break;
                    case '/':  result.push_back('/');  break;
                    case 'n':  result.push_back('\n'); break;
                    case 'r':  result.push_back('\r'); break;
                    case 't':  result.push_back('\t'); break;
                    case 'b':  result.push_back('\b'); break;
                    case 'f':  result.push_back('\f'); break;
                    case 'u': {
                        // \uXXXX Unicode 转义（简单处理 BMP 范围）
                        if (pos + 5 < json_body.size()) {
                            std::string hex = json_body.substr(pos + 2, 4);
                            try {
                                int code = std::stoi(hex, nullptr, 16);
                                if (code < 0x80) {
                                    result.push_back(static_cast<char>(code));
                                } else if (code < 0x800) {
                                    result.push_back(static_cast<char>(0xC0 | (code >> 6)));
                                    result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                                } else {
                                    result.push_back(static_cast<char>(0xE0 | (code >> 12)));
                                    result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                                    result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                                }
                            } catch (...) {
                                result.push_back('?');
                            }
                            pos += 4;  // 额外跳过 4 个 hex 字符（循环末尾还会 +1）
                        }
                        break;
                    }
                    default:
                        result.push_back(next);
                }
                pos += 2;
            } else if (c == '"') {
                // 结束引号
                out_response = std::move(result);
                return true;
            } else {
                result.push_back(c);
                ++pos;
            }
        }

        return false;  // 未找到结束引号
    }

    // =====================================================================
    // 翻译结果校验
    // =====================================================================
    bool AITranslator::validateTranslation(const std::string& input, const std::string& translated) {
        if (translated.empty()) {
            return false;
        }

        // 翻译结果不能和原文完全相同（说明模型没翻译）
        if (translated == input) {
            return false;
        }

        // 翻译结果不能过短（少于原文长度的 20%，可能是截断或错误）
        if (translated.length() < input.length() * 0.2 && translated.length() < 4) {
            return false;
        }

        // 检查是否包含常见的错误前缀（模型没遵守指令）
        const char* bad_prefixes[] = {
            "翻译：", "翻译:", "Translation:", "翻译结果：",
            "以下是", "中文翻译：", "中文版：", "```"
        };
        for (const char* prefix : bad_prefixes) {
            if (translated.compare(0, strlen(prefix), prefix) == 0) {
                // 有前缀但后面可能还有有效翻译，尝试去除前缀
                // 这里不直接拒绝，只记录警告
                LOGGERMANAGER.getLogger()->debug(
                    "[AITranslator] translation has prefix '{}', may need prompt tuning.", prefix);
                break;
            }
        }

        return true;
    }

} // namespace Hooks
} // namespace DFZH
} // namespace DFHack
