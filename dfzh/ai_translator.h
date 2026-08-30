#pragma once
// ai_translator.h
// 本地 Ollama AI 实时补翻译 —— DFZH 第三层翻译后端
//
// 职责：
//   1. 主线程 tryTranslate()：查 AI Cache，命中则返回中文；未命中则加入 Pending + Queue，立即返回
//   2. 后台 Worker Thread：从 Queue 取文本，调本地 Ollama HTTP API，写入 Cache，清除 Pending
//   3. hasNewTranslations()：Worker 写入新翻译后置位，供主线程触发 processTranslations() 重新执行
//
// 约束：
//   - 严禁在主线程同步等待 Ollama
//   - Worker 只写 Cache，不碰 SDL/DF 渲染对象
//   - 任何失败（Ollama 未启动/超时/JSON 错误/空结果）只导致"该文本继续显示英文"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstddef>

namespace DFHack {
namespace DFZH {
namespace Hooks {

    class AITranslator {
    public:
        static AITranslator& getInstance() {
            static AITranslator instance;
            return instance;
        }

        // 主线程每帧调用（在 ScreenManager::processTranslations 中）
        //   input  : 经过 DictManager / Rulesets 均未命中的英文文本
        //   output : 若 Cache 命中，写入中文翻译
        //   返回 true  = Cache 命中，output 有效
        //   返回 false = 未命中（已入队 或 正在 Pending 中），output 不变
        bool tryTranslate(const std::string& input, std::string& output);

        // 插件生命周期
        void start();   // 启动 Worker 线程（ScreenManager::init 末尾调用）
        void stop();    // 停止 Worker 线程（ScreenManager::shutdown 开头调用）

        // 新翻译完成标志 —— Worker 写入 Cache 后置位
        // 主线程在 onSDLGetWindowSize 中检查，若置位则重新执行 processTranslations()
        bool hasNewTranslations() const;
        void clearNewTranslationsFlag();

        // 从配置文件加载参数（start() 内部自动调用）
        void loadConfig();

    private:
        AITranslator();
        ~AITranslator();
        AITranslator(const AITranslator&) = delete;
        AITranslator& operator=(const AITranslator&) = delete;

        // Worker 线程主循环
        void workerLoop();

        // 调本地 Ollama /api/generate，非流式
        //   input  : 待翻译英文
        //   output : 翻译后的中文
        //   返回 true = 成功，output 有效
        bool callOllama(const std::string& input, std::string& output);

        // 构造 Ollama prompt（注入系统提示词 + 待翻译文本）
        std::string buildPrompt(const std::string& input) const;

        // 从 Ollama JSON 响应中提取 "response" 字段
        // 用简单字符串解析，不引入额外 JSON 库
        static bool extractResponseField(const std::string& json_body, std::string& out_response);

        // 校验翻译结果：非空、长度合理、不含明显错误标记
        static bool validateTranslation(const std::string& input, const std::string& translated);

        // ===== 配置（Worker 启动前加载，运行中只读）=====
        bool        enabled_            = true;
        std::string host_               = "127.0.0.1";
        int         port_               = 11434;
        std::string model_              = "qwen3:4b";
        int         timeout_sec_        = 10;
        size_t      max_queue_size_     = 200;
        std::string prompt_template_;   // 含 {text} 占位符

        // ===== 线程安全数据 =====
        // AI Cache：英文原文 -> 中文翻译
        std::unordered_map<std::string, std::string> cache_;
        std::mutex cache_mutex_;

        // Pending：正在翻译中的文本（去重，防止重复入队）
        std::unordered_set<std::string> pending_;
        std::mutex pending_mutex_;

        // 任务队列
        std::queue<std::string> task_queue_;
        std::mutex queue_mutex_;
        std::condition_variable cv_;

        // Worker 线程
        std::thread worker_;
        std::atomic<bool> running_{false};

        // 新翻译完成标志（Worker 置位，主线程检查并清零）
        std::atomic<bool> new_translations_flag_{false};
    };

#define AITRANSLATOR DFHack::DFZH::Hooks::AITranslator::getInstance()

} // namespace Hooks
} // namespace DFZH
} // namespace DFHack
