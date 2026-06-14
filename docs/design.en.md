# EmbedAgent Design Document

## 1. Project Overview

EmbedAgent is a **C++11** embedded Linux LLM Agent library built on top of the [cppev](../cppev/) event-driven framework. Its goal is to enable embedded Linux devices (industrial controllers, gateways, edge computing nodes) to call cloud LLM APIs with minimal resource usage, supporting local tool calling, offline message queuing, session persistence, and other intelligent agent features.

**Core Design Principles:**
- Strict C++11 compatibility; supports GCC 4.8+ cross-compilation toolchains
- Fully asynchronous, single-threaded (loop thread only); lock-free, no extra threads
- No exceptions; errors are propagated via return values or callbacks
- Minimal dependencies: cppev + libcurl + cJSON

---

## 2. Overall Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  Application Layer (examples / user code)                    │
│  EaRuntimeConfig  EvApp  EvConfig                            │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│  EaQueueCoordinator                                          │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ EaAgent                                                 │ │
│  │  ├── EaLlmClient (HTTP / SSE streaming)                 │ │
│  │  ├── EaSession (conversation history + trim)            │ │
│  │  ├── EaToolRegistry (local tool dispatch)               │ │
│  │  └── EaStorage (optional session persistence)           │ │
│  └─────────────────────────────────────────────────────────┘ │
│  ┌─────────────────┐  ┌───────────────────────────────────┐  │
│  │ EaOfflineQueue  │  │ EaConnectivityMonitor             │  │
│  │ (memory / file) │  │ (online state + HTTP probe)       │  │
│  └─────────────────┘  └───────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│  cppev                                                       │
│  EvEventLoop  EvHttpClient  EvSseParser  EvLlmJson           │
│  EvJsonDocument  EvTimerQueue                                │
└──────────────────────────────────────────────────────────────┘
```

### 2.1 Module Responsibilities

| Module | Class | Responsibility |
|---|---|---|
| Agent Core | `EaAgent` | Multi-turn orchestration, tool-calling loop, system prompt rendering |
| LLM Client | `EaLlmClient` | HTTP POST / SSE streaming chat completions, automatic URL resolution |
| Tool System | `EaToolRegistry` / `EaToolBuilder` / `EaToolArgs` | Tool registration, JSON Schema generation, local invocation |
| Session | `EaSession` / `EaSessionCodec` | Conversation history, trimming, JSON serialization |
| Prompt Template | `EaPromptTemplate` | `{{variable}}` placeholder substitution |
| Offline Queue | `EaOfflineQueue` / `EaMemoryOfflineQueue` / `EaFileOfflineQueue` | Buffer user messages when offline |
| Coordinator | `EaQueueCoordinator` | Online direct dispatch, offline enqueue, auto-flush on reconnect |
| Connectivity | `EaConnectivityMonitor` | Online/offline state, optional periodic HTTP probe |
| Storage | `EaFileStorage` | Session persistence (JSON), generic KV file store |
| Secret Management | `EaSecretResolver` / `EaFileSecretStore` / `EaEnvSecretStore` | Three-level API key resolution: CLI > env > file |
| Bootstrap | `EaRuntimeConfig` | Aggregates storage, secrets, and persistence options at startup |

---

## 3. Core Component Design

### 3.1 EaAgent — Multi-Turn Agent Orchestration

EaAgent is the library's core, driving the loop:
"user message → LLM → tool calls → LLM → … → final reply"

```
submitUserMessage(text)
        │
        ▼
refreshSystemPrompt()  // inject device_name, current_time
appendUser(text)
        │
        ▼  ┌──────────────────────────────────────┐
runTurn()  │ check roundTrip >= maxToolRoundTrips │→ finishTurn(false)
           └──────────────────────────────────────┘
        │
        ▼
llmClient_.chat(session.snapshot(), tools, onChunk, onDone)
        │
   ┌────┴──────────────────────────────┐
   │ finish_reason == "tool_calls"     │→ invokeTools() → appendToolResult()
   │                                   │  roundTrip++
   │                                   │  runTurn()  (recursive)
   └───────────────────────────────────┘
        │ finish_reason == "stop"
        ▼
session.appendAssistant(content)
finishTurn(ok=true)  → [optional] storage.saveSession()
```

**Key Design Points:**
- Only one active turn (`activeTurn_`) at a time; concurrent requests fail immediately
- `maxToolRoundTrips` (default 8) prevents infinite tool-calling loops
- Every `submitUserMessage` re-renders the system prompt, injecting fresh `current_time`
- `skipAppendUser=true` is used for flush replay (message already in session, not appended again)

### 3.2 EaLlmClient — HTTP Chat Completions

#### Automatic URL Resolution

```cpp
// Pass API base URL → path is appended automatically
"https://api.deepseek.com"      → "https://api.deepseek.com/v1/chat/completions"
"https://api.openai.com"        → "https://api.openai.com/v1/chat/completions"
"https://api.example.com/v1"    → "https://api.example.com/v1/chat/completions"
// Already contains "completions" → used verbatim
"https://api.example.com/v1/chat/completions"  → unchanged
```

#### Streaming Mode (SSE)

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
EvLlmStreamAccumulator::applyDelta()  // assembles tool_call fragments
onChunk(delta)                         // streams content to caller in real time
        │
  stream ends
        ▼
accumulator.toolCalls()  // complete tool_call list
onDone(result)
```

#### Mock Mode

When `EaLlmClientOptions::mockResponses` is non-empty, HTTP is skipped and responses are consumed sequentially from the list. Tool handlers still execute against the real system, making mock mode ideal for offline integration testing.

### 3.3 Tool Calling System

#### Parameter Definition Layers

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

#### Three Registration Styles

**Style A — High-level convenience overload (recommended):**
```cpp
registry.registerTool("tool_name", "description",
    {/* std::vector<EaToolParam> */},
    [](const EaToolArgs& args, std::string* out) {
        // args.getString / getInt / getDouble / getBool / has
        *out = "{...}";
        return true;
    });
```

**Style B — EaToolBuilder fluent API:**
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

**Style C — Low-level EaToolSpec (raw JSON Schema):**
```cpp
EaToolSpec spec;
spec.def.name           = "name";
spec.def.parametersJson = "{\"type\":\"object\",...}";
spec.handler = [](const std::string& argsJson, std::string* out) { ... };
registry.registerTool(spec);
```

### 3.4 EaSession — Conversation History

```
messages_:
  [ system | user | assistant | tool | tool | assistant | user | ... ]
```

- `snapshot()` returns the full message list including the system message, sent to the LLM
- `trim()` removes the oldest non-system messages when `maxMessages` or `maxApproxTokens` is exceeded
- `rollbackLastUser()` removes the last user message on network failure, preventing it from appearing in both the session and the queue
- `serializeSession` / `loadSessionSnapshot` serialize to/from JSON (`session.json`)

### 3.5 EaQueueCoordinator — Offline Scheduling

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
  └── peek front → agent.submit → dequeue or update attemptCount
```

**Key Design Points:**
- `onDone` returns the string `"queued"` when a message has been buffered offline; `ok=true` means the request was accepted
- Items exceeding `maxItemAttempts` (default 3) are discarded
- Network recovery is triggered by a state-change event from `EaConnectivityMonitor`

### 3.6 EaConnectivityMonitor — Connectivity Awareness

State machine: `kUnknown → kOnline / kOffline`

```
State transitions are triggered by:
1. setOnline(bool)              — set externally (e.g. after LLM call succeeds/fails)
2. HTTP probe (optional)        — startProbe(url, intervalSec)
   GET probeUrl, timeout=10s
   2xx-3xx → kOnline
   error / 4xx+ → kOffline
```

`isOnline()` returns `state_ != kOffline`, so `kUnknown` is treated as online (optimistic strategy).

### 3.7 EaSecretResolver — Three-Level API Key Resolution

```
Priority (highest → lowest):
  1. CLI override    (--api-key=KEY set by the application layer)
  2. Environment     (EMBEDAGENT_API_KEY)
  3. File store      ({dataDir}/api_key)  — XOR-obfuscated
```

`EaFileSecretStore` uses simple XOR obfuscation (not encryption) to store the key, preventing plaintext visibility on disk.

---

## 4. Threading Model

All core classes are annotated **call in loop thread only** and must be called from the `cppev::EvEventLoop` thread.

```
cppev::EvApp::onStart()            ← loop thread
    EaRuntimeConfig config(opts)
    EaAgent agent(loop(), agentOpts)
    agent.setToolRegistry(&tools)
    agent.submitUserMessage(...)    ← initiated in loop thread
                │
                ▼
    llmClient_.chat(...)            ← internally async via EvHttpClient I/O
                │
    onChunk / onDone callback       ← delivered in loop thread
```

Tool handlers are called synchronously in the loop thread. Long-running tools (file I/O, blocking system calls) should be offloaded via `EvApp::runAsync`.

---

## 5. Error Handling

| Error Type | Handling |
|---|---|
| HTTP 4xx/5xx | `onDone(false, "EaLlmClient: HTTP xxx")` + `LOGE` prints the response body |
| Network timeout / connection failure | `onDone(false, curl error message)` |
| JSON parse failure | `onDone(false, "EaLlmClient: parseChatResponse failed")` |
| Unknown tool | `onDone(false, "EaAgent: tool invoke failed: name")` + `LOGE` lists registered tools |
| Duplicate tool name | `registerTool` returns `false` + `LOGW` |
| Max tool round trips exceeded | `onDone(false, "EaAgent: max tool round trips exceeded")` |
| Missing API key | `onDone(false, "EaLlmClient: url/apiKey/model required")` |
| Concurrent request | `onDone(false, "EaAgent: request already in progress")` |

---

## 6. Storage Layout

`EaFileStorage` default directory is `/tmp/embedagent` (overridable with `--data-dir`):

```
{dataDir}/
├── session.json      — serialized EaSession (system prompt + history)
├── api_key           — XOR-obfuscated API key (created with --save-api-key)
└── kv/               — generic KV store (key used as filename)
```

---

## 7. Key Design Decisions

### 7.1 No Exceptions

The entire library does not throw exceptions. Errors are communicated via `bool` return values or `onDone(false, error)` callbacks, which is appropriate for embedded environments.

### 7.2 Optimistic Online Strategy

`EaConnectivityMonitor` starts in `kUnknown` state, and `isOnline()` returns `true`. The first LLM call failure transitions to `kOffline`, avoiding probe-latency impact on normal operation.

### 7.3 Session Rollback on Failure

When a network failure requires queuing, the library calls `rollbackLastUser()` to undo the already-appended message before enqueuing the original text, preventing duplicate entries in both session and queue.

### 7.4 Real Tool Execution in Mock Mode

`mockResponses` only mocks LLM responses; local tool handlers still execute against the real system, enabling complete end-to-end testing of tool logic without network access.

### 7.5 Strict C++11 Compatibility

Uses `std::unique_ptr<T>(new T())` instead of `std::make_unique`, and `std::function` instead of deduced lambda types, ensuring compatibility with GCC 4.8 and ARM cross-compilation toolchains.

---

## 8. Directory Structure

```
embedagent/
├── include/             — public headers (24 files)
│   ├── embed_agent.h    — umbrella header (include this)
│   ├── ea_agent.h       — core Agent
│   ├── ea_llm_client.h  — LLM HTTP client
│   ├── ea_session.h     — conversation history
│   ├── ea_tool_*.h      — tool system
│   ├── ea_queue_*.h     — offline queue and coordinator
│   ├── ea_storage.h     — storage abstraction
│   └── ea_secret_*.h    — secret management
├── src/                 — implementations (20 .cpp files)
├── examples/
│   ├── common/          — shared demo utilities (EaStreamPrinter, etc.)
│   ├── chat_agent/      — full chat agent demo (4 tool categories)
│   ├── tool_call_demo/  — embedded tool demo (12 Linux system tools)
│   └── offline_queue_demo/ — offline queue demonstration
├── tests/               — GTest unit tests (9 targets)
└── CMakeLists.txt
```
