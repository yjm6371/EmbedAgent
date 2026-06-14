# EmbedAgent Usage Guide

## 1. Quick Start

### 1.1 Building

```bash
# Recommended: use CMake preset (requires CMake 3.21+)
cmake --preset host-debug -DCPPEV_SOURCE_DIR=/path/to/cppev
cmake --build --preset host-debug

# Or configure manually
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCPPEV_SOURCE_DIR=/path/to/cppev
cmake --build build -j4

# Build output
# build/examples/   — example executables
# build/tests/      — unit tests
```

**Dependencies:**
- cppev (required; provide via `-DCPPEV_SOURCE_DIR` or git submodule `external/cppev/`)
- libcurl-dev
- libssl-dev (for cppev TLS extension)
- libgtest-dev (tests, optional)

### 1.2 Include Headers

```cpp
// Umbrella header (recommended)
#include <embed_agent.h>

// Or include selectively
#include <ea_agent.h>         // EaAgent, EaAgentOptions
#include <ea_tool_registry.h> // EaToolRegistry
#include <ea_tool_builder.h>  // EaToolBuilder
#include <ea_runtime_config.h>// EaRuntimeConfig
```

---

## 2. Minimal Hello World

No API key required; uses the built-in mock mode:

```cpp
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>
#include <ev_logger.h>
#include <cstdio>
#include <memory>

class HelloApp : public cppev::framework::EvApp {
protected:
    void onStart() override {
        EaAgentOptions opts;
        opts.llm.model  = "mock-model";
        opts.llm.stream = false;
        opts.llm.mockResponses.push_back(
            "{\"choices\":[{\"message\":{\"role\":\"assistant\","
            "\"content\":\"Hello from EmbedAgent!\"},"
            "\"finish_reason\":\"stop\"}]}");

        agent_.reset(new embedagent::EaAgent(loop(), opts));
        agent_->submitUserMessage(
            "Hi",
            [](const std::string& delta) {
                std::fputs(delta.c_str(), stdout);
            },
            [this](bool ok, const std::string& err) {
                std::fputs("\n", stdout);
                if (!ok) { LOGE("error: %s", err.c_str()); }
                queueInLoop([this]() { quit(); });
            });
    }
    void onStop() override { agent_.reset(); }

private:
    std::unique_ptr<embedagent::EaAgent> agent_;
};

int main(int argc, char* argv[]) {
    cppev::framework::EvConfig cfg;
    cfg.parse(argc, argv);
    HelloApp app(cfg);
    app.run();
}
```

---

## 3. Connecting to a Real LLM API

### 3.1 DeepSeek

```bash
./chat_agent --url=https://api.deepseek.com \
             --api-key=sk-xxxx \
             --model=deepseek-chat \
             --prompt="Hello"
```

### 3.2 OpenAI

```bash
./chat_agent --url=https://api.openai.com \
             --api-key=sk-xxxx \
             --model=gpt-4o-mini \
             --prompt="Hello"
```

### 3.3 Any OpenAI-Compatible API

Pass the API base URL; the library automatically appends `/v1/chat/completions`:

```bash
./chat_agent --url=https://your.api.host \
             --api-key=KEY \
             --model=MODEL_NAME \
             --prompt="..."
```

> **Note:** If the URL already contains `completions`, it is used verbatim without modification.

---

## 4. API Key Management

### 4.1 Resolution Priority

The library resolves the API key in the following order (highest → lowest):
1. `--api-key=KEY` command-line argument
2. Environment variable `EMBEDAGENT_API_KEY`
3. File `{dataDir}/api_key` (saved with `--save-api-key`)

### 4.2 Persisting the API Key

```bash
# First run: save key to file
./chat_agent --url=... --api-key=sk-xxxx --save-api-key --prompt="test"

# Subsequent runs: no --api-key needed
./chat_agent --url=... --model=deepseek-chat --prompt="Hello"
```

### 4.3 Using It in Code

```cpp
embedagent::EaRuntimeConfigOptions ropts;
ropts.dataDir        = "/var/lib/myapp";
ropts.cliApiKey      = cfg.apiKey();   // may be empty
ropts.saveApiKey     = cfg.saveApiKey();
ropts.persistSession = true;

embedagent::EaRuntimeConfig runtime(ropts);

embedagent::EaAgentOptions opts;
opts.llm.url   = cfg.url();
opts.llm.model = cfg.model();
opts.storage   = &runtime.storage;
opts.persistSession = runtime.persistSession;

std::string apiKey;
runtime.resolveApiKey(&apiKey);  // three-level resolution
opts.llm.apiKey = apiKey;
```

---

## 5. Registering Tools

### 5.1 Tool with No Parameters

```cpp
embedagent::EaToolRegistry tools;

tools.registerTool(
    "get_device_status",
    "Return device uptime and temperature",
    {},  // no parameters
    [](const embedagent::EaToolArgs& /*args*/, std::string* out) {
        *out = "{\"uptime_sec\":3600,\"temp_c\":42}";
        return true;
    });
```

### 5.2 Tool with Parameters (convenience overload)

```cpp
std::vector<embedagent::EaToolParam> params;
{
    embedagent::EaToolParam p;
    p.name        = "path";
    p.type        = embedagent::EaParamType::kString;
    p.description = "Filesystem path";
    p.required    = false;
    params.push_back(p);
}

tools.registerTool(
    "get_disk_usage",
    "Return disk usage for a given path",
    params,
    [](const embedagent::EaToolArgs& args, std::string* out) {
        std::string path = args.getString("path", "/");
        // ... statvfs ...
        return true;
    });
```

### 5.3 Tool with Enum Parameter (EaToolBuilder)

```cpp
std::vector<std::string> levels;
levels.push_back("0");
levels.push_back("1");

embedagent::EaToolSpec spec = embedagent::EaToolBuilder()
    .name("gpio_write")
    .description("Set a GPIO output pin high (1) or low (0)")
    .param("pin",   embedagent::EaParamType::kInteger, "GPIO pin number")
    .enumParam("value", embedagent::EaParamType::kInteger,
               "Output level: 0=low, 1=high", levels)
    .handler([](const embedagent::EaToolArgs& args, std::string* out) {
        int pin = args.getInt("pin", -1);
        int val = args.getInt("value", -1);
        LOGI("gpio_write(pin=%d, value=%d)", pin, val);
        *out = "{\"ok\":true}";
        return true;
    })
    .build();
tools.registerTool(spec);
```

### 5.4 EaToolArgs Accessors

```cpp
args.getString("key", "default");  // string value
args.getInt   ("key", 0);          // integer
args.getDouble("key", 0.0);        // double (see note below)
args.getBool  ("key", false);      // boolean
args.has      ("key");             // check for presence
args.valid();                      // true if JSON parsed successfully
```

> **Precision note:** `getDouble()` internally uses `getInt()`, so fractional parts are truncated. For precise floating-point values, have the model pass the value as a string and parse it manually.

### 5.5 Registration Notes

- Registering a tool with a duplicate name is rejected (`LOGW` logged, returns `false`)
- Invoking an unknown tool logs all registered tool names (`LOGE`)
- Use `tools.size()` to count registered tools; `tools.empty()` to check if any are registered

---

## 6. Complete Agent Example

```cpp
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>
#include <ev_logger.h>
#include <cstdio>
#include <ctime>
#include <memory>

using namespace embedagent;
using namespace cppev::framework;

class MyApp : public EvApp {
public:
    explicit MyApp(const EvConfig& cfg) : EvApp(cfg) {}
protected:
    void onStart() override {
        // 1. Register tools
        tools_.registerTool("get_time", "Return current Unix timestamp", {},
            [](const EaToolArgs&, std::string* out) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "{\"ts\":%ld}",
                              static_cast<long>(std::time(nullptr)));
                *out = buf;
                return true;
            });

        // 2. Configure Agent options
        EaAgentOptions opts;
        opts.llm.url    = "https://api.deepseek.com";
        opts.llm.apiKey = "sk-xxxx";
        opts.llm.model  = "deepseek-chat";
        opts.llm.stream = true;

        // 3. Set system prompt
        EaPromptTemplate tmpl;
        tmpl.setTemplate("You are {{device_name}}, a helpful assistant.");
        opts.systemTemplate = tmpl;
        opts.deviceName = "my-device";

        // 4. Create and configure Agent
        agent_.reset(new EaAgent(loop(), opts));
        agent_->setToolRegistry(&tools_);
        agent_->setRoundStartCallback([](int round) {
            if (round > 0) {
                std::fputs("\n[tool call completed, continuing...]\n", stdout);
            }
        });

        // 5. Send message
        agent_->submitUserMessage(
            "What time is it right now?",
            [](const std::string& delta) {
                std::fputs(delta.c_str(), stdout);
                std::fflush(stdout);
            },
            [this](bool ok, const std::string& err) {
                std::fputs("\n", stdout);
                if (!ok) { LOGE("Agent error: %s", err.c_str()); }
                queueInLoop([this]() { quit(); });
            });
    }
    void onStop() override { agent_.reset(); }

private:
    EaToolRegistry             tools_;
    std::unique_ptr<EaAgent>   agent_;
};

int main(int argc, char* argv[]) {
    EvConfig cfg;
    cfg.parse(argc, argv);
    MyApp app(cfg);
    app.run();
}
```

---

## 7. System Prompt Customization

### 7.1 Using Template Variables

```cpp
EaPromptTemplate tmpl;
tmpl.setTemplate(
    "You are {{device_name}}, an embedded Linux device agent. "
    "Current time: {{current_time}}. "
    "Location: {{location}}.");

// device_name and current_time are injected automatically by EaAgent
// Custom variables must be set manually
opts.systemTemplate = tmpl;
opts.deviceName = "gateway-01";
opts.systemTemplate.setVariable("location", "factory-floor-3");
```

### 7.2 Direct Override (bypass template)

```cpp
// Must be called before submitUserMessage
agent_->setSystemPrompt(
    "You are a helpful embedded assistant. "
    "Always reply concisely.");
```

---

## 8. Session Persistence

Save and restore conversation history across process restarts:

```bash
# First run: create and save the session
./chat_agent --url=... --api-key=KEY --persist-session --prompt="Hi, I'm Alice"

# Second run: load the previous session
./chat_agent --url=... --persist-session --prompt="What's my name?"
# Assistant: Your name is Alice.
```

Code implementation:

```cpp
EaAgentOptions opts;
opts.storage        = &runtime.storage;   // EaFileStorage
opts.persistSession = true;

// EaAgent constructor auto-loads session.json if storage is provided
// On success, system prompt from the saved session is preserved
// After each successful turn, storage.saveSession() is called automatically
```

---

## 9. Offline Queue and EaQueueCoordinator

Designed for unstable network conditions on embedded devices: messages are not lost when offline, and are automatically retried when connectivity is restored.

```cpp
// Create offline queue (in-memory; see EaFileOfflineQueue for file-backed)
std::unique_ptr<embedagent::EaOfflineQueue> queue(
    new embedagent::EaMemoryOfflineQueue());

// Connectivity monitor
std::unique_ptr<embedagent::EaConnectivityMonitor> monitor(
    new embedagent::EaConnectivityMonitor(loop()));

// Optional: start periodic HTTP probe (every 30 seconds)
monitor->startProbe("https://api.deepseek.com", 30.0);

// Create coordinator
embedagent::EaQueueCoordinatorOptions coordOpts;
coordOpts.maxQueueItems     = 50;
coordOpts.maxItemAttempts   = 3;
coordOpts.autoFlushOnOnline = true;

embedagent::EaQueueCoordinator coordinator(
    loop(), agentOpts, coordOpts,
    std::move(queue), std::move(monitor));

coordinator.setToolRegistry(&tools);

// Send message (direct if online, queued if offline)
coordinator.submitUserMessage(
    "User input...",
    [](const std::string& delta) { std::fputs(delta.c_str(), stdout); },
    [](bool ok, const std::string& result) {
        if (result == "queued") {
            LOGI("offline — message queued");
        } else if (!ok) {
            LOGE("failed: %s", result.c_str());
        }
    });

// Manually trigger queue flush
coordinator.flush(onText, onDone);

// Check queue depth
LOGI("pending: %zu", coordinator.pendingCount());
```

---

## 10. Connectivity Monitoring

```cpp
embedagent::EaConnectivityMonitor monitor(loop());

// Option 1: set state manually (e.g. in Agent onDone callback)
monitor.setOnline(true);   // after a successful call
monitor.setOnline(false);  // after a network error

// Option 2: periodic HTTP probe
monitor.startProbe("https://api.deepseek.com", 30.0);
monitor.stopProbe();

// Register state change callback
monitor.setTransitionCallback([](embedagent::EaConnectivityState state) {
    if (state == embedagent::EaConnectivityState::kOnline) {
        LOGI("network restored");
    } else if (state == embedagent::EaConnectivityState::kOffline) {
        LOGW("network lost");
    }
});

// Query current state
bool online = monitor.isOnline();  // kUnknown is treated as online
```

---

## 11. Mock Mode (Offline Testing)

Mock mode only mocks LLM responses; local tool handlers execute against the real system:

```cpp
EaAgentOptions opts;
opts.llm.model  = "mock-model";
opts.llm.stream = false;

// Pre-script multi-turn responses (consumed in order)
opts.llm.mockResponses.push_back(
    "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
    "\"tool_calls\":[{\"id\":\"c1\",\"type\":\"function\",\"function\":"
    "{\"name\":\"get_device_status\",\"arguments\":\"{}\"}}]},"
    "\"finish_reason\":\"tool_calls\"}]}");

opts.llm.mockResponses.push_back(
    "{\"choices\":[{\"message\":{\"role\":\"assistant\","
    "\"content\":\"Device is healthy: uptime 3600s, 42°C.\"},"
    "\"finish_reason\":\"stop\"}]}");
```

---

## 12. Session Management

```cpp
embedagent::EaSession& session = agent_->session();

// Inspect message history
const std::vector<cppev::ext::EvLlmMessage>& msgs = session.messages();
LOGI("%zu messages in session", msgs.size());

// Configure session limits
embedagent::EaSessionOptions sopts;
sopts.maxMessages     = 20;   // keep at most 20 messages
sopts.maxApproxTokens = 4000; // ~4K token budget
opts.session = sopts;

// Clear session
session.clear();

// Serialize to JSON
std::string json;
embedagent::serializeSession(session, &json);

// Deserialize from JSON
embedagent::EaSessionSnapshot snap;
embedagent::loadSessionSnapshot(json, &snap);
embedagent::applySessionSnapshot(snap, &session);
```

---

## 13. Logging Configuration

```bash
# Log to stderr (example default)
./chat_agent --log-sink=stderr

# Log to file
./chat_agent --log-sink=file --log-file=/var/log/agent.log

# Set log level
./chat_agent --log-level=debug
./chat_agent --log-level=warn

# UDP logging (log server)
./chat_agent --log-sink=udp --log-udp-host=192.168.1.100 --log-udp-port=5140
```

In code:

```cpp
LOGD("debug: value=%d", val);
LOGI("info: started");
LOGW("warn: retrying...");
LOGE("error: %s", err.c_str());
```

---

## 14. Command-Line Flags Reference

All applications derived from `cppev::framework::EvConfig` support these flags automatically:

| Flag | Description | Example Default |
|---|---|---|
| `--url=URL` | API base URL or full completions URL | `https://api.deepseek.com` |
| `--api-key=KEY` | Bearer token | — |
| `--model=NAME` | Model name | `gpt-4o-mini` |
| `--prompt=TEXT` | User message | (example default) |
| `--mock` | Offline mock mode | `false` |
| `--data-dir=PATH` | Storage root directory | `/tmp/embedagent` |
| `--save-api-key` | Persist API key to file | `false` |
| `--persist-session` | Save/load session across runs | `false` |
| `--log-level=LEVEL` | `debug/info/warn/error` | `info` |
| `--log-sink=SINK` | `stdout/stderr/syslog/udp/file` | `stderr` |
| `--log-file=PATH` | File log path | — |
| `--daemon` | Run as background daemon | `false` |
| `--help` | Print usage and exit | — |

---

## 15. Running the Example Programs

### chat_agent — Full Chat Agent

```bash
# Offline test
./build/examples/chat_agent --mock

# Connect to DeepSeek
./build/examples/chat_agent \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --model=deepseek-chat \
    --prompt="Check system status and control GPIO"

# Save API key, persist session
./build/examples/chat_agent \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --save-api-key --persist-session \
    --prompt="Hello, my name is Alice"
```

### tool_call_demo — Embedded Tool Demo (12 tools)

```bash
# Offline demo (tools read real /proc and /sys)
./build/examples/tool_call_demo --mock

# Live mode
./build/examples/tool_call_demo \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --model=deepseek-chat \
    --prompt="Run a full system health check, list top 5 processes"
```

Registered tools:

| Category | Tool Name | Function |
|---|---|---|
| System Info | `get_system_info` | Uptime, load average |
| | `get_memory_info` | Memory usage breakdown |
| | `get_disk_usage` | Disk usage for a given path |
| Thermal | `read_cpu_temperature` | CPU/SoC temperature |
| | `list_thermal_zones` | All thermal zone temperatures |
| GPIO | `gpio_read` | Read GPIO pin level |
| | `gpio_write` | Set GPIO pin level |
| | `gpio_set_direction` | Set GPIO pin direction |
| Network | `get_network_interfaces` | Interface IP/MAC/state |
| | `get_interface_stats` | Interface TX/RX statistics |
| Process | `list_top_processes` | Top N processes by memory |
| | `get_process_info` | Detailed info for a PID |

### offline_queue_demo — Offline Queue Demo

```bash
./build/examples/offline_queue_demo --mock
```

---

## 16. Integrating into Your Own Project

### CMake Setup

```cmake
# Option 1: source reference (recommended during development)
cmake -B build -DCPPEV_SOURCE_DIR=/path/to/cppev

# Option 2: git submodule
git submodule add https://github.com/example/cppev external/cppev
cmake -B build

# Option 3: installed cppev package
cmake -B build  # find_package(cppev) searches automatically
```

```cmake
# In your CMakeLists.txt
target_link_libraries(myapp PRIVATE embedagent)
```

### Minimal main.cpp Skeleton

```cpp
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>

class MyConfig : public cppev::framework::EvConfig {
    // Add custom CLI parameters here
};

class MyApp : public cppev::framework::EvApp {
public:
    explicit MyApp(const MyConfig& cfg) : EvApp(cfg), cfg_(cfg) {}
protected:
    void onStart() override {
        // 1. Register tools
        // 2. Create EaRuntimeConfig
        // 3. Create EaAgent or EaQueueCoordinator
        // 4. submitUserMessage
    }
    void onStop() override {
        agent_.reset();
    }
private:
    MyConfig                             cfg_;
    embedagent::EaToolRegistry           tools_;
    std::unique_ptr<embedagent::EaAgent> agent_;
};

int main(int argc, char* argv[]) {
    MyConfig cfg;
    cfg.parse(argc, argv);
    MyApp app(cfg);
    app.run();
}
```

---

## 17. Frequently Asked Questions

**Q: Getting HTTP 400 errors**

The full response body is logged at `LOGE` level and usually contains the specific reason:
```
[E] EaLlmClient: HTTP 400 — {"error":{"message":"...","type":"invalid_request_error"}}
```
Common causes: misspelled model name, wrong URL path, invalid API key.

**Q: How do I debug tool calls?**

Inspect the session transcript after completion:
```cpp
const auto& msgs = agent_->session().messages();
for (const auto& m : msgs) {
    LOGI("[%s] %s", m.role.c_str(), m.content.c_str());
}
```

**Q: Can I do blocking I/O inside a tool handler?**

Not recommended. Tool handlers are called synchronously in the loop thread; blocking will stall all I/O. Offload long-running work via `EvApp::runAsync`.

**Q: What model name should I use for DeepSeek?**

Use `deepseek-chat` (general conversation) or `deepseek-reasoner` (reasoning tasks). Check the [DeepSeek API documentation](https://platform.deepseek.com/docs) for the current model list.

**Q: How do I implement a multi-turn REPL?**

Do not re-create `EaAgent` between turns. Reuse the same instance and call `submitUserMessage` repeatedly; conversation history is maintained automatically in `EaSession`.
