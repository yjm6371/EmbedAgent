# EmbedAgent 使用说明书

## 1. 快速开始

### 1.1 构建

```bash
# 推荐：使用 CMake preset（需 CMake 3.21+）
cmake --preset host-debug -DCPPEV_SOURCE_DIR=/path/to/cppev
cmake --build --preset host-debug

# 或手动指定
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCPPEV_SOURCE_DIR=/path/to/cppev
cmake --build build -j4

# 构建产物目录
# build/examples/   — 示例可执行文件
# build/tests/      — 单元测试
```

**依赖：**
- cppev（必需，可通过 `-DCPPEV_SOURCE_DIR` 或 git submodule `external/cppev/` 提供）
- libcurl-dev
- libssl-dev（cppev TLS 扩展）
- libgtest-dev（测试，可选）

### 1.2 头文件

```cpp
// 统一入口（推荐）
#include <embed_agent.h>

// 或按需包含
#include <ea_agent.h>        // EaAgent, EaAgentOptions
#include <ea_tool_registry.h>// EaToolRegistry
#include <ea_tool_builder.h> // EaToolBuilder
#include <ea_runtime_config.h>// EaRuntimeConfig
```

---

## 2. 最简 Hello World

不依赖任何 API key，使用内置 mock 模式：

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

## 3. 接入真实 LLM API

### 3.1 DeepSeek

```bash
./chat_agent --url=https://api.deepseek.com \
             --api-key=sk-xxxx \
             --model=deepseek-chat \
             --prompt="你好"
```

### 3.2 OpenAI

```bash
./chat_agent --url=https://api.openai.com \
             --api-key=sk-xxxx \
             --model=gpt-4o-mini \
             --prompt="Hello"
```

### 3.3 其他 OpenAI-compatible API

URL 只需传入 API base，库会自动补全 `/v1/chat/completions`：

```bash
./chat_agent --url=https://your.api.host \
             --api-key=KEY \
             --model=MODEL_NAME \
             --prompt="..."
```

> **注意：** 如果已知完整 URL 含有 `completions`，则原样使用，不追加路径。

---

## 4. API Key 管理

### 4.1 优先级

库按以下顺序解析 API key（高→低）：
1. `--api-key=KEY` 命令行参数
2. 环境变量 `EMBEDAGENT_API_KEY`
3. 文件 `{dataDir}/api_key`（由 `--save-api-key` 保存）

### 4.2 持久化 API Key

```bash
# 首次运行：保存 key 到文件
./chat_agent --url=... --api-key=sk-xxxx --save-api-key --prompt="test"

# 后续运行：无需 --api-key
./chat_agent --url=... --model=deepseek-chat --prompt="Hello"
```

### 4.3 代码中使用

```cpp
EaRuntimeConfigOptions ropts;
ropts.dataDir        = "/var/lib/myapp";  // 存储根目录
ropts.cliApiKey      = cfg.apiKey();       // 可为空
ropts.saveApiKey     = cfg.saveApiKey();   // 是否持久化
ropts.persistSession = true;               // 是否持久化会话

embedagent::EaRuntimeConfig runtime(ropts);

EaAgentOptions opts;
opts.llm.url   = cfg.url();
opts.llm.model = cfg.model();
opts.storage   = &runtime.storage;
opts.persistSession = runtime.persistSession;

std::string apiKey;
runtime.resolveApiKey(&apiKey);  // 三级解析
opts.llm.apiKey = apiKey;
```

---

## 5. 工具注册

### 5.1 无参数工具

```cpp
embedagent::EaToolRegistry tools;

tools.registerTool(
    "get_device_status",
    "Return device uptime and temperature",
    {},  // 无参数
    [](const embedagent::EaToolArgs& /*args*/, std::string* out) {
        *out = "{\"uptime_sec\":3600,\"temp_c\":42}";
        return true;
    });
```

### 5.2 带参数工具（便捷重载）

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

### 5.3 带 enum 参数（EaToolBuilder）

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

### 5.4 EaToolArgs 访问器

```cpp
args.getString("key", "default");   // 字符串
args.getInt   ("key", 0);           // 整数
args.getDouble("key", 0.0);         // 浮点（精度受限，见注）
args.getBool  ("key", false);       // 布尔
args.has      ("key");              // 是否存在
args.valid();                       // JSON 解析是否成功
```

> **精度注意：** `getDouble()` 底层通过 `getInt()` 实现，小数部分会被截断。如需精确浮点，在工具 description 中说明请模型传字符串。

### 5.5 注册注意事项

- 工具名重复注册会被拒绝（`LOGW` 日志，返回 `false`）
- 调用不存在的工具会在日志中列出所有已注册工具名
- `tools.size()` 查看已注册数量，`tools.empty()` 检查是否为空

---

## 6. 完整 Agent 示例

```cpp
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>
#include <ev_logger.h>
#include <cstdio>
#include <memory>

using namespace embedagent;
using namespace cppev::framework;

class MyApp : public EvApp {
public:
    explicit MyApp(const EvConfig& cfg) : EvApp(cfg) {}
protected:
    void onStart() override {
        // 1. 注册工具
        tools_.registerTool("get_time", "Return current Unix timestamp", {},
            [](const EaToolArgs&, std::string* out) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "{\"ts\":%ld}",
                              static_cast<long>(std::time(nullptr)));
                *out = buf;
                return true;
            });

        // 2. 配置 Agent
        EaAgentOptions opts;
        opts.llm.url    = "https://api.deepseek.com";
        opts.llm.apiKey = "sk-xxxx";
        opts.llm.model  = "deepseek-chat";
        opts.llm.stream = true;

        // 3. 设置系统提示
        EaPromptTemplate tmpl;
        tmpl.setTemplate("You are {{device_name}}, a helpful assistant.");
        opts.systemTemplate = tmpl;
        opts.deviceName = "my-device";

        // 4. 创建并配置 Agent
        agent_.reset(new EaAgent(loop(), opts));
        agent_->setToolRegistry(&tools_);
        agent_->setRoundStartCallback([](int round) {
            if (round > 0) {
                std::fputs("\n[tool call completed, continuing...]\n", stdout);
            }
        });

        // 5. 发送消息
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

## 7. 系统提示定制

### 7.1 使用模板变量

```cpp
EaPromptTemplate tmpl;
tmpl.setTemplate(
    "You are {{device_name}}, an embedded Linux device agent. "
    "Current time: {{current_time}}. "
    "Location: {{location}}.");

// device_name 和 current_time 由 EaAgent 自动注入
// 自定义变量需手动设置
opts.systemTemplate = tmpl;
opts.deviceName = "gateway-01";
// location 等自定义变量在 onStart 中注入：
opts.systemTemplate.setVariable("location", "factory-floor-3");
```

### 7.2 直接设置（跳过模板）

```cpp
agent_->setSystemPrompt(
    "You are a helpful embedded assistant. "
    "Always reply in English.");
```

---

## 8. 会话持久化

跨进程保存和恢复对话历史：

```bash
# 首次运行：创建并保存会话
./chat_agent --url=... --api-key=KEY --persist-session --prompt="Hi, I'm Alice"

# 再次运行：加载上次会话
./chat_agent --url=... --persist-session --prompt="What's my name?"
# Assistant: Your name is Alice.
```

代码实现：

```cpp
EaAgentOptions opts;
opts.storage        = &runtime.storage;   // EaFileStorage
opts.persistSession = true;

// Agent 构造时自动尝试从 session.json 加载
// 成功时跳过 refreshSystemPrompt（使用保存的 system prompt）
// finishTurn 成功后自动调用 storage.saveSession()
```

---

## 9. 离线队列与 EaQueueCoordinator

适用于网络不稳定的嵌入式场景：断网时用户消息不丢失，网络恢复后自动重发。

```cpp
// 创建离线队列（内存版；文件版见 EaFileOfflineQueue）
std::unique_ptr<embedagent::EaOfflineQueue> queue(
    new embedagent::EaMemoryOfflineQueue());

// 连通性监控
std::unique_ptr<embedagent::EaConnectivityMonitor> monitor(
    new embedagent::EaConnectivityMonitor(loop()));

// 可选：启动 HTTP 周期探测（每 30s 探测一次）
monitor->startProbe("https://api.deepseek.com", 30.0);

// 创建调度器
embedagent::EaQueueCoordinatorOptions coordOpts;
coordOpts.maxQueueItems   = 50;
coordOpts.maxItemAttempts = 3;
coordOpts.autoFlushOnOnline = true;

embedagent::EaQueueCoordinator coordinator(
    loop(), agentOpts, coordOpts,
    std::move(queue), std::move(monitor));

coordinator.setToolRegistry(&tools);

// 发送消息（在线直发，离线入队）
coordinator.submitUserMessage(
    "用户输入...",
    [](const std::string& delta) { std::fputs(delta.c_str(), stdout); },
    [](bool ok, const std::string& result) {
        if (result == "queued") {
            LOGI("离线，已加入队列");
        } else if (!ok) {
            LOGE("失败: %s", result.c_str());
        }
    });

// 手动触发 flush
coordinator.flush(onText, onDone);

// 查看队列长度
LOGI("pending: %zu", coordinator.pendingCount());
```

---

## 10. 连通性监控

```cpp
embedagent::EaConnectivityMonitor monitor(loop());

// 方式1：手动设置状态（如在 Agent onDone 中）
monitor.setOnline(true);   // 调用成功后
monitor.setOnline(false);  // 网络错误后

// 方式2：HTTP 探测
monitor.startProbe("https://api.deepseek.com", 30.0);
monitor.stopProbe();

// 注册状态变更回调
monitor.setTransitionCallback([](embedagent::EaConnectivityState state) {
    if (state == embedagent::EaConnectivityState::kOnline) {
        LOGI("网络恢复");
    } else if (state == embedagent::EaConnectivityState::kOffline) {
        LOGW("网络断开");
    }
});

// 查询当前状态
bool online = monitor.isOnline();  // kUnknown 也视为在线
```

---

## 11. Mock 模式（离线测试）

Mock 模式只模拟 LLM 响应，本地工具 handler 真实执行：

```cpp
EaAgentOptions opts;
opts.llm.model  = "mock-model";
opts.llm.stream = false;

// 预设多轮响应（按顺序消费）
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

## 12. 会话管理

```cpp
embedagent::EaSession& session = agent_->session();

// 查看消息历史
const std::vector<cppev::ext::EvLlmMessage>& msgs = session.messages();
LOGI("共 %zu 条消息", msgs.size());

// 设置会话选项
embedagent::EaSessionOptions sopts;
sopts.maxMessages     = 20;   // 最多保留 20 条消息
sopts.maxApproxTokens = 4000; // 约 4K token 上限
opts.session = sopts;

// 手动清空会话
session.clear();

// 序列化
std::string json;
embedagent::serializeSession(session, &json);

// 反序列化
embedagent::EaSessionSnapshot snap;
embedagent::loadSessionSnapshot(json, &snap);
embedagent::applySessionSnapshot(snap, &session);
```

---

## 13. 日志配置

```bash
# 日志输出到 stderr（示例默认）
./chat_agent --log-sink=stderr

# 输出到文件
./chat_agent --log-sink=file --log-file=/var/log/agent.log

# 设置日志级别
./chat_agent --log-level=debug
./chat_agent --log-level=warn

# UDP 日志（日志服务器）
./chat_agent --log-sink=udp --log-udp-host=192.168.1.100 --log-udp-port=5140
```

代码中使用：

```cpp
LOGD("debug: value=%d", val);
LOGI("info: started");
LOGW("warn: retrying...");
LOGE("error: %s", err.c_str());
```

---

## 14. 命令行参数速查

所有继承自 `cppev::framework::EvConfig` 的应用自动支持以下参数：

| 参数 | 说明 | 示例默认值 |
|---|---|---|
| `--url=URL` | API base URL 或完整 completions URL | `https://api.deepseek.com` |
| `--api-key=KEY` | Bearer token | — |
| `--model=NAME` | 模型名称 | `gpt-4o-mini` |
| `--prompt=TEXT` | 用户消息 | （示例默认提示） |
| `--mock` | 离线 mock 模式 | `false` |
| `--data-dir=PATH` | 存储根目录 | `/tmp/embedagent` |
| `--save-api-key` | 持久化 API key 到文件 | `false` |
| `--persist-session` | 跨进程保存/加载会话 | `false` |
| `--log-level=LEVEL` | `debug/info/warn/error` | `info` |
| `--log-sink=SINK` | `stdout/stderr/syslog/udp/file` | `stderr` |
| `--log-file=PATH` | 文件日志路径 | — |
| `--daemon` | 后台守护进程模式 | `false` |
| `--help` | 打印用法并退出 | — |

---

## 15. 运行示例程序

### chat_agent — 完整聊天 Agent

```bash
# 离线测试
./build/examples/chat_agent --mock

# 接入 DeepSeek
./build/examples/chat_agent \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --model=deepseek-chat \
    --prompt="查询系统状态并控制GPIO"

# 保存 API key，持久化会话
./build/examples/chat_agent \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --save-api-key --persist-session \
    --prompt="你好，我叫小明"
```

### tool_call_demo — 嵌入式工具演示（12 个工具）

```bash
# 离线演示（工具真实读取 /proc /sys）
./build/examples/tool_call_demo --mock

# 在线模式
./build/examples/tool_call_demo \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --model=deepseek-chat \
    --prompt="做一次系统健康检查，列出 top 5 进程"
```

已注册工具：

| 类别 | 工具名 | 功能 |
|---|---|---|
| 系统信息 | `get_system_info` | uptime、load average |
| | `get_memory_info` | 内存使用详情 |
| | `get_disk_usage` | 指定路径磁盘用量 |
| 温度 | `read_cpu_temperature` | CPU/SoC 温度 |
| | `list_thermal_zones` | 所有热区温度 |
| GPIO | `gpio_read` | 读取 GPIO 电平 |
| | `gpio_write` | 设置 GPIO 电平 |
| | `gpio_set_direction` | 设置 GPIO 方向 |
| 网络 | `get_network_interfaces` | 网卡 IP/MAC/状态 |
| | `get_interface_stats` | 网卡收发统计 |
| 进程 | `list_top_processes` | 内存占用 Top N |
| | `get_process_info` | 指定 PID 详情 |

### offline_queue_demo — 离线队列演示

```bash
./build/examples/offline_queue_demo --mock
```

---

## 16. 在自己的项目中集成

### CMakeLists.txt

```cmake
# 方式1：源码引用（推荐开发阶段）
cmake -B build -DCPPEV_SOURCE_DIR=/path/to/cppev

# 方式2：git submodule
git submodule add https://github.com/example/cppev external/cppev
cmake -B build

# 方式3：已安装的 cppev 包
cmake -B build  # find_package(cppev) 自动查找
```

```cmake
# CMakeLists.txt
target_link_libraries(myapp PRIVATE embedagent)
target_include_directories(myapp PRIVATE ${EMBEDAGENT_INCLUDE_DIR})
```

### 最小 main.cpp 骨架

```cpp
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>

class MyConfig : public cppev::framework::EvConfig {
    // 在这里添加自定义 CLI 参数
};

class MyApp : public cppev::framework::EvApp {
public:
    explicit MyApp(const MyConfig& cfg) : EvApp(cfg), cfg_(cfg) {}
protected:
    void onStart() override {
        // 1. 注册工具
        // 2. 创建 EaRuntimeConfig
        // 3. 创建 EaAgent / EaQueueCoordinator
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

## 17. 常见问题

**Q: HTTP 400 错误**

日志中会打印完整的 response body，通常包含具体原因：
```
[E] EaLlmClient: HTTP 400 — {"error":{"message":"...","type":"invalid_request_error"}}
```
常见原因：模型名称拼写错误、URL 路径错误、API key 无效。

**Q: 如何调试工具调用？**

查看 session transcript：
```cpp
const auto& msgs = agent_->session().messages();
for (auto& m : msgs) {
    LOGI("[%s] %s", m.role.c_str(), m.content.c_str());
}
```

**Q: 工具 handler 中可以做阻塞 I/O 吗？**

不建议。工具 handler 在 loop 线程同步调用，阻塞会影响所有 I/O。耗时操作应使用 `EvApp::runAsync` 卸载到后台线程。

**Q: DeepSeek 应该用什么模型名？**

推荐 `deepseek-chat`（通用对话）或 `deepseek-reasoner`（推理任务）。`deepseek-v4-flash` 是虚构名称。

**Q: 如何实现多轮对话（REPL）？**

不要每次重新创建 `EaAgent`，复用同一个实例并重复调用 `submitUserMessage`，历史消息会自动保持在 `EaSession` 中。
