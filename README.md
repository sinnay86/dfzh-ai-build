# DFZH-AI：矮人要塞 DFZH + Ollama AI 实时补翻译

基于 DFZH v0.8.4 修改，增加本地 Ollama AI 实时补翻译功能。

**支持版本：Dwarf Fortress 53.06 + DFHack 53.06-r1**

---

## 一、自动编译（GitHub Actions）

本仓库已配置 GitHub Actions 自动编译。推送代码到 `main` 分支后自动触发编译。

### 编译步骤

1. 把本仓库所有文件上传到你的 GitHub 公开仓库
2. 推送后，点击仓库顶部的 **Actions** 标签页
3. 看到 **Build DFZH-AI** 工作流正在运行
4. 等待 5-15 分钟编译完成
5. 点击进入该次运行，拉到最底部 **Artifacts** 区域
6. 下载 **dfzh-ai-release-package**（包含 DLL + 数据文件）

### 编译产物

下载的 zip 解压后包含：
```
release-package/
├── plugins/
│   └── dfzh.plug.dll          ← 编译好的插件
└── data/
    └── dfzh/
        ├── dfzh_config.txt
        ├── dfzh_dict_exact.csv
        ├── dfzh_dict_word.csv
        ├── ai_translation.toml  ← AI 翻译配置
        ├── fonts/
        └── rulesets/
```

---

## 二、安装到游戏

### 前置要求

1. **Dwarf Fortress 53.06**（Steam 版）
2. **DFHack 53.06-r1**（必须与 DF 版本匹配）
   - 下载：https://github.com/DFHack/dfhack/releases/tag/53.06-r1
   - 下载 `dfhack-53.06-r1-Windows-64bit.zip`
   - 解压到 DF 游戏目录（和 `Dwarf Fortress.exe` 同级）
3. **Ollama**（本地 AI 运行时）
   - 下载：https://ollama.com/download
   - 安装后启动，确保 `ollama serve` 正在运行
4. **Qwen 模型**（翻译用）
   ```bash
   ollama pull qwen2.5:7b
   ```

### 安装 DFZH-AI

1. 从 GitHub Actions 下载 `dfzh-ai-release-package.zip`
2. 解压
3. 把 `plugins/dfzh.plug.dll` 复制到 DF 游戏目录的 `hack/plugins/` 下
4. 把 `data/dfzh/` 目录下的所有文件复制到 `hack/data/dfzh/` 下
5. 确认目录结构：
   ```
   DF游戏目录/
   ├── hack/
   │   ├── plugins/
   │   │   └── dfzh.plug.dll
   │   └── data/
   │       └── dfzh/
   │           ├── dfzh_config.txt
   │           ├── dfzh_dict_exact.csv
   │           ├── ai_translation.toml
   │           ├── fonts/
   │           └── rulesets/
   └── Dwarf Fortress.exe
   ```

---

## 三、使用

### 启动

1. 启动 Ollama（确保 `ollama serve` 正在运行）
2. 启动 Dwarf Fortress（通过 DFHack）
3. 进入游戏后按 `Ctrl-Alt-K` 开启中文显示

### 翻译行为

- **固定 UI 文本**（菜单、物品名）：DFZH 词典命中，瞬间显示中文
- **首次出现的程序化文本**（战斗日志、矮人想法）：
  - 第 1 帧显示英文，同时后台开始 AI 翻译
  - 0.3-1 秒后自动切换为中文
- **重复文本**：AI Cache 命中，瞬间显示中文
- **Ollama 未启动或超时**：该文本继续显示英文，不影响游戏

### 验证 AI 翻译是否工作

1. 启动游戏后打开 DFHack 控制台（`Ctrl-Shift-P`）
2. 看到 `[AITranslator] worker thread started.` 说明 AI 翻译已启动
3. 遇到未被词典翻译的长文本，观察是否在 1 秒内变为中文

---

## 四、AI 翻译配置

编辑 `hack/data/dfzh/ai_translation.toml`：

```toml
[ollama]
enabled = true              # 是否启用 AI 补翻译
host = "127.0.0.1"         # Ollama 地址
port = 11434                # Ollama 端口
model = "qwen2.5:7b"       # 模型名称（必须已 ollama pull）
timeout = 10                # 单次翻译超时（秒）
max_queue_size = 200        # 最大队列长度

[prompt]
template = "你是一个专业的游戏翻译器。将以下英文文本翻译成简体中文。只输出翻译结果，不要输出解释、Markdown或前缀。\n\n原文：{text}\n\n译文："
```

### 模型选择建议

| 模型 | 显存占用 | 翻译速度 | 翻译质量 | 推荐场景 |
|---|---|---|---|---|
| qwen3:4b | ~3GB | 极快 | 良好 | 低配机、追求速度 |
| qwen2.5:7b | ~5GB | 快 | 优秀 | **推荐**，质量速度平衡 |
| qwen2.5:14b | ~10GB | 较慢 | 极佳 | 高配机、追求质量 |

修改配置后重启游戏生效。

---

## 五、热键

| 热键 | 功能 |
|---|---|
| `Ctrl-Alt-K` | 切换中文显示开/关 |
| `Ctrl-Alt-R` | 重新加载词典和规则 |
| `Ctrl-Alt-L` | 导出未翻译文本日志 |

---

## 六、故障排查

### 游戏启动后 DFHack 不加载

**原因：DFHack 版本与 DF 版本不匹配。**
- DF 53.06 必须用 DFHack 53.06-r1
- 下载对应版本：https://github.com/DFHack/dfhack/releases/tag/53.06-r1

### AI 翻译不工作

1. Ollama 是否运行？`curl http://127.0.0.1:11434/api/tags`
2. 模型是否已下载？`ollama list`
3. `ai_translation.toml` 中的 `model` 名称是否与 `ollama list` 一致
4. DFHack 控制台是否有 `[AITranslator]` 相关日志

### 翻译延迟过高

- 换用更小的模型（qwen3:4b）
- 关闭其他占用 GPU 的程序
- 确认 Ollama 使用 GPU 推理

### 翻译质量差

- 换用更大的模型（7b → 14b）
- 编辑 `ai_translation.toml` 中的 `prompt.template`，加强翻译指令

### 游戏卡顿

- 本 Mod 的 AI 翻译在后台线程运行，不阻塞游戏主线程
- 如卡顿，检查是否为 Ollama 占用资源，可限制 Ollama 并行数：
  ```bash
  set OLLAMA_NUM_PARALLEL=1
  ```

---

## 七、技术架构

```
屏幕文本 → SentenceDetector → DictManager(CSV词典) → RulesManager(TOML规则) → AI(Ollama)
                ↓ 命中                  ↓ 命中                    ↓ 命中              ↓ Cache命中
               中文渲染                中文渲染                   中文渲染             中文渲染
                                                                      ↓ Cache未命中
                                                                 加入Queue → Worker后台翻译
                                                                      ↓ 翻译完成
                                                                 写入Cache → 下一帧自动显示中文
```

- AI 是第三层补充翻译，不替代原有翻译系统
- 单 Worker 线程异步翻译，不阻塞 DF 主线程
- 内存 Cache 缓存翻译结果，重复文本零延迟
- Pending 集合防止重复请求
- 故障安全：Ollama 不可用时只显示英文，不影响游戏

---

## 八、源码说明

| 文件 | 说明 |
|---|---|
| `dfzh/ai_translator.h` | AI 翻译器类声明 |
| `dfzh/ai_translator.cpp` | AI 翻译器实现（Worker线程、Ollama调用、Cache、Pending） |
| `dfzh/screen_manager.cpp` | 修改：接入 AI 第三层翻译 |
| `CMakeLists.txt` | 修改：加入新源文件、cpp-httplib、ws2_32 |
| `3rdParty/cpp-httplib/httplib.h` | header-only HTTP 客户端 |
| `data/ai_translation.toml` | AI 翻译配置 |
| `.github/workflows/build.yml` | GitHub Actions 自动编译脚本 |

---

## 九、许可证

- 本 Mod 源码：MIT License（与 DFZH 一致）
- cpp-httplib：MIT License
- 翻译数据：CC BY-NC 4.0（DFZH 原有数据）
