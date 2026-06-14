# EmbedAgent 设计说明书

## 1. 项目定位

EmbedAgent 是一个基于 **C++11** 的嵌入式 Linux LLM Agent 库，构建于 [cppev](../cppev/) 事件驱动框架之上。目标是让嵌入式 Linux 设备（工控机、网关、边缘计算节点）能以极低的资源占用调用云端大语言模型 API，实现本地工具调用、离线队列缓冲、会话持久化等智能代理功能。

**核心设计原则：**
- C++11 严格兼容，支持 GCC 4.8+ 嵌入式工具链
- 全异步、单线程（loop thread only），无锁、无额外线程
- 不使用异常，错误通过返回值或回调传递
- 最小依赖：cppev + libcurl + cJSON

---

## 2. 整体架构

```
┌──────────────────────────────────────────────────────────────┐
│  应用层（examples / user code）                               │
│  EaRuntimeConfig  EvApp  EvConfig                            │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│  EaQueueCoordinator                                          │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ EaAgent                                                 │ │
│  │  ├── EaLlmClient (HTTP / SSE streaming)                 │ │
│  │  ├── EaSession (对话历史 + trim)                         │ │
│  │  ├── EaToolRegistry (本地工具调用)                       │ │
│  │  └── EaStorage (可选会话持久化)                          │ │
│  └─────────────────────────────────────────────────────────┘ │
│  ┌─────────────────┐  ┌───────────────────────────────────┐  │
│  │ EaOfflineQueue  │  │ EaConnectivityMonitor             │  │
│  │ (内存 / 文件)   │  │ (在线状态 + HTTP 周期探测)         │  │
│  └─────────────────┘  └───────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│  cppev                                                       │
│  EvEventLoop  EvHttpClient  EvSseParser  EvLlmJson           │
│  EvJsonDocument  EvTimerQueue                                │
└──────────────────────────────────────────────────────────────┘
```

### 2.1 模块职责

| 模块 | 类 | 职责 |
|---|---|---|
| Agent 核心 | `EaAgent` | 多轮对话编排、tool-calling 循环、system prompt 渲染 |
| LLM 客户端 | `EaLlmClient` | HTTP POST / SSE 流式 chat completions，URL 自动解析 |
| 工具系统 | `EaToolRegistry` / `EaToolBuilder` / `EaToolArgs` | 工具注册、JSON Schema 生成、本地调用 |
| 会话管理 | `EaSession` / `EaSessionCodec` | 对话历史维护、trim、JSON 序列化 |
| 提示模板 | `EaPromptTemplate` | `{{variable}}` 占位符替换 |
| 离线队列 | `EaOfflineQueue` / `EaMemoryOfflineQueue` / `EaFileOfflineQueue` | 断网时缓冲用户消息 |
| 调度协调 | `EaQueueCoordinator` | 在线直发、离线入队、恢复后自动 flush |
| 连通性监控 | `EaConnectivityMonitor` | 在线/离线状态管理，可选 HTTP 周期探测 |
| 存储 | `EaFileStorage` | 会话持久化（JSON）、KV 文件存储 |
| 密钥管理 | `EaSecretResolver` / `EaFileSecretStore` / `EaEnvSecretStore` | CLI > 环境变量 > 文件三级解析 |
| 运行时配置 | `EaRuntimeConfig` | 启动阶段聚合存储、密钥、持久化选项 |

---

## 3. 核心组件设计

### 3.1 EaAgent — 多轮 Agent 编排

EaAgent 是库的核心，负责驱动"用户消息 → LLM → 工具调用 → LLM → … → 最终回复"的循环。

```
submitUserMessage(text)
        │
        ▼
refreshSystemPrompt()  // 注入 device_name, current_time
appendUser(text)
        │
        ▼  ┌────────────────────────────────────┐
runTurn()  │ 检查 roundTrip >= maxToolRoundTrips │→ finishTurn(false)
           └────────────────────────────────────┘
        │
        ▼
llmClient_.chat(session.snapshot(), tools, onChunk, onDone)
        │
   ┌────┴─────────────────────────┐
   │ finish_reason == "tool_calls" │→ invokeTools() → session.appendToolResult()
   │                               │  roundTrip++
   │                               │  runTurn()  (递归)
   └──────────────────────────────┘
        │ finish_reason == "stop"
        ▼
session.appendAssistant(content)
finishTurn(ok=true)  → [可选] storage.saveSession()
```

**关键设计点：**
- 同一时刻只允许一个活跃 turn（`activeTurn_`），并发请求立即失败
- `maxToolRoundTrips`（默认 8）防止工具调用死循环
- 每次 `submitUserMessage` 都会重新渲染 system prompt，注入最新 `current_time`
- `skipAppendUser=true` 用于 flush 重放（消息已在会话中，不重复追加）

### 3.2 EaLlmClient — HTTP Chat Completions

#### URL 自动解析

```cpp
// 传入 API base → 自动补全路径
"https://api.deepseek.com"      → "https://api.deepseek.com/v1/chat/completions"
"https://api.openai.com"        → "https://api.openai.com/v1/chat/completions"
"https://api.example.com/v1"    → "https://api.example.com/v1/chat/completions"
// 已含 completions 则不修改
"https://api.example.com/v1/chat/completions"  → 原样
```

#### 流式模式（SSE）

```
EvHttpClient::requestStream
        │ chunk bytes
        ▼
EvSseParser::feed()
        │ data: {...}
        ▼
parseStreamDelta()
        │ EvLlmStreamDelta
        ▼
EvLlmStreamAccumulator::applyDelta()  // 拼接 tool_call fragments
onChunk(delta)                         // 实时推送 content 给调用方
        │
  stream ends
        ▼
accumulator.toolCalls()  // 完整 tool_call 列表
onDone(result)
```

#### Mock 模式

`EaLlmClientOptions::mockResponses` 非空时跳过 HTTP，从列表中顺序取下一条预设响应，工具 handler 仍真实执行。适合离线测试。

### 3.3 工具调用系统

#### 参数定义层次

```
EaParamType (string/integer/number/boolean)
    ↓
EaToolParam (name, type, description, required, enumValues)
    ↓
buildParametersJson()  → JSON Schema string
    ↓
EvLlmToolDef (name, description, parametersJson)
    ↓
EaToolSpec (def + EaToolHandler)
    ↓
EaToolRegistry
```

#### 两种注册方式

**方式 A — 高级便捷重载（推荐）：**
```cpp
registry.registerTool("tool_name", "description",
    {/* std::vector<EaToolParam> */},
    [](const EaToolArgs& args, std::string* out) {
        // args.getString / getInt / getDouble / getBool / has
        *out = "{...}";
        return true;
    });
```

**方式 B — EaToolBuilder 链式 API：**
```cpp
EaToolSpec spec = EaToolBuilder()
    .name("gpio_write")
    .description("...")
    .param("pin", EaParamType::kInteger, "GPIO pin number")
    .enumParam("value", EaParamType::kInteger, "0 or 1", {"0","1"})
    .handler([](const EaToolArgs& args, std::string* out) { ... })
    .build();
registry.registerTool(spec);
```

**方式 C — 低级 EaToolSpec（原始 JSON Schema）：**
```cpp
EaToolSpec spec;
spec.def.name        = "name";
spec.def.parametersJson = "{\"type\":\"object\",...}";
spec.handler = [](const std::string& argsJson, std::string* out) { ... };
registry.registerTool(spec);
```

### 3.4 EaSession — 对话历史管理

```
messages_:
  [ system | user | assistant | tool | tool | assistant | user | ... ]
```

- `snapshot()` 返回含 system message 的完整列表，发送给 LLM
- `trim()` 超出 `maxMessages` 或 `maxApproxTokens` 时从最旧非 system 消息开始删除
- `rollbackLastUser()` 网络失败时回退最后一条 user 消息，防止入队后重复
- `serializeSession` / `loadSessionSnapshot` 将会话序列化为 JSON（`session.json`）

### 3.5 EaQueueCoordinator — 离线调度

```
submitUserMessage(text)
        │
   isOnline?
  ┌──────┴──────┐
  │ Yes         │ No / flushing
  ▼             ▼
agent.submit  enqueue(text) → onDone("queued")
  │
  ├── success → monitor.setOnline(true)
  └── network error → monitor.setOnline(false)
                    → session.rollbackLastUser()
                    → enqueue(text) → onDone("queued")

onConnectivityChanged(kOnline) → tryFlush()
  └── peek front item → agent.submit → dequeue or update attemptCount
```

**关键设计：**
- `onDone` 返回字符串 `"queued"` 表示已离线入队，`ok=true` 代表请求被接受
- 超过 `maxItemAttempts`（默认 3）次的条目被丢弃
- 网络恢复由 `EaConnectivityMonitor` 状态变更事件触发

### 3.6 EaConnectivityMonitor — 连通性感知

状态机：`kUnknown → kOnline / kOffline`

```
状态转换触发方式：
1. setOnline(bool)           — 外部直接设置（如 LLM 调用成功/失败后）
2. HTTP 探测（可选）          — startProbe(url, intervalSec)
   GET probeUrl, timeout=10s
   2xx-3xx → kOnline
   error / 4xx+ → kOffline
```

`isOnline()` 返回 `state_ != kOffline`，即 `kUnknown` 也视为在线（乐观策略）。

### 3.7 EaSecretResolver — API Key 三级解析

```
优先级（高→低）：
  1. CLI override   (--api-key=KEY 由应用层设置)
  2. 环境变量       (EMBEDAGENT_API_KEY)
  3. 文件存储       ({dataDir}/api_key)  XOR 混淆
```

`EaFileSecretStore` 使用简单 XOR 混淆存储密钥（不是加密，仅防止明文可见）。

---

## 4. 线程模型

所有核心类均标注 **call in loop thread only**，在 `cppev::EvEventLoop` 所在线程调用。

```
cppev::EvApp::onStart()           ← loop 线程
    EaRuntimeConfig config(opts)
    EaAgent agent(loop(), agentOpts)
    agent.setToolRegistry(&tools)
    agent.submitUserMessage(...)   ← loop 线程发起
                │
                ▼
    llmClient_.chat(...)           ← 内部通过 EvHttpClient 异步 I/O
                │
    onChunk / onDone callback      ← loop 线程回调
```

工具 handler 在 loop 线程同步调用。如果工具执行耗时（如文件 I/O），应通过 `EvApp::runAsync` 卸载到后台线程。

---

## 5. 错误处理

| 错误类型 | 处理方式 |
|---|---|
| HTTP 4xx/5xx | `onDone(false, "EaLlmClient: HTTP xxx")` + `LOGE` 打印 response body |
| 网络超时/连接失败 | `onDone(false, curl error message)` |
| JSON 解析失败 | `onDone(false, "EaLlmClient: parseChatResponse failed")` |
| 工具不存在 | `onDone(false, "EaAgent: tool invoke failed: name")` + `LOGE` 列出已注册工具 |
| 工具重名注册 | `registerTool` 返回 `false` + `LOGW` |
| 超过最大工具轮次 | `onDone(false, "EaAgent: max tool round trips exceeded")` |
| API Key 缺失 | `onDone(false, "EaLlmClient: url/apiKey/model required")` |
| 请求并发 | `onDone(false, "EaAgent: request already in progress")` |

---

## 6. 存储结构

`EaFileStorage` 默认目录 `/tmp/embedagent`（可通过 `--data-dir` 覆盖）：

```
{dataDir}/
├── session.json      — 序列化的 EaSession（含 system prompt、历史消息）
├── api_key           — XOR 混淆的 API key（--save-api-key 时创建）
└── kv/               — 通用 KV 存储（key 作为文件名）
```

---

## 7. 关键设计决策

### 7.1 无异常

全库不抛出异常，错误通过 `bool` 返回值或 `onDone(false, error)` 回调传递，适合嵌入式环境。

### 7.2 乐观在线策略

`EaConnectivityMonitor` 初始状态为 `kUnknown`，`isOnline()` 返回 `true`。第一次 LLM 调用失败后才切换为 `kOffline`，避免探测延迟影响正常流程。

### 7.3 会话 rollback

网络失败需要入队时，先 `rollbackLastUser()` 撤销已追加的消息，再入队原始文本，防止消息在会话和队列中重复出现。

### 7.4 Mock 模式下工具真实执行

`mockResponses` 只模拟 LLM 响应，本地工具 handler 仍真实运行，确保工具逻辑可在离线环境下完整测试。

### 7.5 C++11 严格兼容

使用 `std::unique_ptr<T>(new T())` 代替 `std::make_unique`，使用 `std::function` 代替 lambda 类型推导，确保与 GCC 4.8 + ARM 交叉编译工具链兼容。

---

## 8. 目录结构

```
embedagent/
├── include/             — 公共头文件（24 个）
│   ├── embed_agent.h    — 统一入口（umbrella header）
│   ├── ea_agent.h       — 核心 Agent
│   ├── ea_llm_client.h  — LLM HTTP 客户端
│   ├── ea_session.h     — 对话历史
│   ├── ea_tool_*.h      — 工具系统
│   ├── ea_queue_*.h     — 离线队列与协调器
│   ├── ea_storage.h     — 存储抽象
│   └── ea_secret_*.h    — 密钥管理
├── src/                 — 实现（20 个 .cpp）
├── examples/
│   ├── common/          — 示例公共工具（EaStreamPrinter 等）
│   ├── chat_agent/      — 完整聊天 Agent 示例（含 4 类工具）
│   ├── tool_call_demo/  — 嵌入式工具 demo（12 个 Linux 系统工具）
│   └── offline_queue_demo/ — 离线队列演示
├── tests/               — GTest 单元测试（9 个目标）
└── CMakeLists.txt
```
