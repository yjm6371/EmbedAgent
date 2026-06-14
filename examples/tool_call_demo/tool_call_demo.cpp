// tool_call_demo.cpp — EmbedAgent embedded Linux tool-calling demo
//
// Demonstrates the full EmbedAgent tool-calling loop with 12 real embedded
// Linux tools organised in five categories:
//
//   System info  : get_system_info, get_memory_info, get_disk_usage
//   Thermal      : read_cpu_temperature, list_thermal_zones
//   GPIO         : gpio_read, gpio_write, gpio_set_direction
//   Network      : get_network_interfaces, get_interface_stats
//   Process      : list_top_processes, get_process_info
//
// In --mock mode all tool handlers are executed locally against the real
// Linux sysfs / proc filesystem; only the LLM responses are pre-scripted.
//
// Usage (offline, real system data):
//   ./tool_call_demo --mock
//
// Usage (live LLM):
//   ./tool_call_demo --url=https://api.deepseek.com --api-key=KEY --model=deepseek-chat
//   ./tool_call_demo --url=https://api.openai.com   --api-key=KEY --model=gpt-4o-mini
//
// Chat text -> stdout; logs -> stderr (default).

#include "ea_tool_gpio.h"
#include "ea_tool_network.h"
#include "ea_tool_process.h"
#include "ea_tool_sysinfo.h"
#include "ea_tool_thermal.h"

#include <ea_demo_io.h>
#include <ea_runtime_config.h>
#include <ea_tool_builder.h>
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>
#include <ev_logger.h>

#include <cstdio>
#include <memory>
#include <string>

namespace embedagent {
namespace example {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

class ToolCallDemoConfig : public cppev::framework::EvConfig {
public:
    ToolCallDemoConfig() { setDefaultLogSink("stderr"); }

    bool               mock()           const { return mock_; }
    const std::string& url()            const { return url_; }
    const std::string& apiKey()         const { return apiKey_; }
    const std::string& model()          const { return model_; }
    const std::string& prompt()         const { return prompt_; }
    const std::string& dataDir()        const { return dataDir_; }
    bool               saveApiKey()     const { return saveApiKey_; }
    bool               persistSession() const { return persistSession_; }

protected:
    void printUsage() const override {
        cppev::framework::EvConfig::printUsage();
        std::puts("  --mock              offline mode — real tools, scripted LLM");
        std::puts("  --data-dir=PATH     storage root (default: /tmp/embedagent)");
        std::puts("  --save-api-key      persist --api-key to file store");
        std::puts("  --persist-session   load/save session.json across runs");
        std::puts("  --url=URL           API base URL or full chat/completions URL");
        std::puts("  --api-key=KEY       bearer token (or EMBEDAGENT_API_KEY / file)");
        std::puts("  --model=NAME        model name (default: gpt-4o-mini)");
        std::puts("  --prompt=TEXT       user message");
        std::puts("");
        std::puts("  Available tools (12):");
        std::puts("    get_system_info, get_memory_info, get_disk_usage");
        std::puts("    read_cpu_temperature, list_thermal_zones");
        std::puts("    gpio_read, gpio_write, gpio_set_direction");
        std::puts("    get_network_interfaces, get_interface_stats");
        std::puts("    list_top_processes, get_process_info");
    }

    bool parseOption(const std::string& key, const std::string& value) override {
        if (key == "mock")            { mock_ = true;           return true; }
        if (key == "data-dir")        { dataDir_ = value;       return true; }
        if (key == "save-api-key")    { saveApiKey_ = true;     return true; }
        if (key == "persist-session") { persistSession_ = true; return true; }
        if (key == "url")             { url_ = value;           return true; }
        if (key == "api-key")         { apiKey_ = value;        return true; }
        if (key == "model")           { model_ = value;         return true; }
        if (key == "prompt")          { prompt_ = value;        return true; }
        return cppev::framework::EvConfig::parseOption(key, value);
    }

private:
    bool        mock_           {false};
    bool        saveApiKey_     {false};
    bool        persistSession_ {false};
    std::string dataDir_        {"/tmp/embedagent"};
    std::string url_;
    std::string apiKey_;
    std::string model_  {"gpt-4o-mini"};
    std::string prompt_ {
        "Run a full system health check: "
        "get system info, memory usage, CPU temperature, and list the top "
        "5 processes by memory. "
        "If temperature is above 60 C, set GPIO pin 18 to high to activate "
        "the cooling fan, then confirm the pin state."};
};

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

class ToolCallDemoApp : public cppev::framework::EvApp {
public:
    explicit ToolCallDemoApp(const ToolCallDemoConfig& cfg);
    ~ToolCallDemoApp() override;

protected:
    void onStart() override;
    void onStop()  override;

private:
    void registerTools();
    void initRuntime();
    void applyRuntime(EaAgentOptions* opts, bool needApiKey);
    void buildSystemPrompt(EaAgentOptions* opts);
    void runMockAgent();
    void runLiveAgent();
    void runAgent(EaAgentOptions opts);

    ToolCallDemoConfig               cfg_;
    EaToolRegistry                   tools_;
    std::unique_ptr<EaRuntimeConfig> runtime_;
    std::unique_ptr<EaAgent>         agent_;
    EaStreamPrinter                  printer_;
};

ToolCallDemoApp::ToolCallDemoApp(const ToolCallDemoConfig& cfg)
    : cppev::framework::EvApp(cfg)
    , cfg_(cfg)
{}

ToolCallDemoApp::~ToolCallDemoApp() {}

// ---------------------------------------------------------------------------
// Tool registration — five categories, 12 tools total
// ---------------------------------------------------------------------------

void ToolCallDemoApp::registerTools() {
    registerSysinfoTools(&tools_);   // get_system_info, get_memory_info, get_disk_usage
    registerThermalTools(&tools_);   // read_cpu_temperature, list_thermal_zones
    registerGpioTools   (&tools_);   // gpio_read, gpio_write, gpio_set_direction
    registerNetworkTools(&tools_);   // get_network_interfaces, get_interface_stats
    registerProcessTools(&tools_);   // list_top_processes, get_process_info

    LOGI("ToolCallDemo: %s",
         tools_.empty() ? "no tools registered (error)" : "all tools registered");
}

// ---------------------------------------------------------------------------
// Runtime / options helpers
// ---------------------------------------------------------------------------

void ToolCallDemoApp::initRuntime() {
    EaRuntimeConfigOptions ropts;
    ropts.dataDir        = cfg_.dataDir();
    ropts.cliApiKey      = cfg_.apiKey();
    ropts.saveApiKey     = cfg_.saveApiKey();
    ropts.persistSession = cfg_.persistSession();
    runtime_.reset(new EaRuntimeConfig(ropts));
}

void ToolCallDemoApp::applyRuntime(EaAgentOptions* opts, bool needApiKey) {
    if (!opts || !runtime_) { return; }
    opts->storage        = &runtime_->storage;
    opts->persistSession = runtime_->persistSession;
    if (!needApiKey) { return; }

    std::string    apiKey;
    EaSecretSource source = EaSecretSource::kNone;
    if (runtime_->resolveApiKey(&apiKey, &source)) {
        opts->llm.apiKey = apiKey;
        if (source == EaSecretSource::kFile) {
            LOGI("ToolCallDemo: API key loaded from file store");
        } else if (source == EaSecretSource::kEnv) {
            LOGI("ToolCallDemo: API key loaded from environment variable");
        }
    }
}

void ToolCallDemoApp::buildSystemPrompt(EaAgentOptions* opts) {
    EaPromptTemplate tmpl;
    tmpl.setTemplate(
        "You are {{device_name}}, an intelligent embedded Linux device agent. "
        "You have access to tools for monitoring and controlling the hardware. "
        "When the user requests a health check or hardware operation, "
        "always call the relevant tools to obtain real data — never guess. "
        "After receiving tool results, give a concise, technical summary "
        "with clear pass/fail or action-taken statements. "
        "Use SI units. Flag any anomaly explicitly.");
    opts->systemTemplate = tmpl;
    opts->deviceName     = "embed-tool-demo";
}

// ---------------------------------------------------------------------------
// Mock agent — 4-round scripted fixture, tools execute against real /proc /sys
// ---------------------------------------------------------------------------

void ToolCallDemoApp::runMockAgent() {
    EaAgentOptions opts;
    opts.llm.model  = "mock-model";
    opts.llm.stream = false;

    // Round 1: gather system metrics in parallel.
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":["
          "{\"id\":\"c1\",\"type\":\"function\",\"function\":"
            "{\"name\":\"get_system_info\",\"arguments\":\"{}\"}},"
          "{\"id\":\"c2\",\"type\":\"function\",\"function\":"
            "{\"name\":\"get_memory_info\",\"arguments\":\"{}\"}},"
          "{\"id\":\"c3\",\"type\":\"function\",\"function\":"
            "{\"name\":\"read_cpu_temperature\",\"arguments\":\"{}\"}}"
        "]},"
        "\"finish_reason\":\"tool_calls\"}]}");

    // Round 2: drill into top processes.
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":["
          "{\"id\":\"c4\",\"type\":\"function\",\"function\":"
            "{\"name\":\"list_top_processes\","
            "\"arguments\":\"{\\\"count\\\":5}\"}}"
        "]},"
        "\"finish_reason\":\"tool_calls\"}]}");

    // Round 3: activate cooling fan because temperature > 60 C.
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":["
          "{\"id\":\"c5\",\"type\":\"function\",\"function\":"
            "{\"name\":\"gpio_write\","
            "\"arguments\":\"{\\\"pin\\\":18,\\\"value\\\":1}\"}},"
          "{\"id\":\"c6\",\"type\":\"function\",\"function\":"
            "{\"name\":\"gpio_read\","
            "\"arguments\":\"{\\\"pin\\\":18}\"}}"
        "]},"
        "\"finish_reason\":\"tool_calls\"}]}");

    // Round 4: final natural-language health report.
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\","
        "\"content\":\"System health check complete.\\n"
        "- Uptime and load: retrieved from /proc/uptime and /proc/loadavg.\\n"
        "- Memory: obtained from /proc/meminfo (used% reported).\\n"
        "- CPU temperature: read from thermal_zone0. "
        "Temperature exceeded 60 C threshold.\\n"
        "- Top 5 processes by RSS listed from /proc.\\n"
        "- Action: GPIO 18 set HIGH to activate cooling fan; "
        "pin state confirmed via gpio_read.\\n"
        "All checks passed. No other anomalies detected.\"},"
        "\"finish_reason\":\"stop\"}]}");

    buildSystemPrompt(&opts);
    applyRuntime(&opts, false);
    runAgent(opts);
}

// ---------------------------------------------------------------------------
// Live agent — connects to a real LLM API
// ---------------------------------------------------------------------------

void ToolCallDemoApp::runLiveAgent() {
    if (cfg_.url().empty()) {
        LOGW("ToolCallDemo: --url is required (or use --mock for offline mode)");
        queueInLoop([this]() { quit(); });
        return;
    }

    EaAgentOptions opts;
    opts.llm.url    = cfg_.url();
    opts.llm.model  = cfg_.model();
    opts.llm.stream = true;
    applyRuntime(&opts, true);

    if (opts.llm.apiKey.empty()) {
        LOGW("ToolCallDemo: no API key — pass --api-key, set EMBEDAGENT_API_KEY, "
             "or use --save-api-key on a previous run");
        queueInLoop([this]() { quit(); });
        return;
    }

    if (cfg_.url().find("deepseek") != std::string::npos &&
        cfg_.model() == "gpt-4o-mini") {
        LOGW("ToolCallDemo: DeepSeek endpoint detected; "
             "consider --model=deepseek-chat");
    }

    buildSystemPrompt(&opts);
    runAgent(opts);
}

// ---------------------------------------------------------------------------
// Core agent runner
// ---------------------------------------------------------------------------

void ToolCallDemoApp::runAgent(EaAgentOptions opts) {
    agent_.reset(new EaAgent(loop(), opts));
    agent_->setToolRegistry(&tools_);

    agent_->setRoundStartCallback([this](int roundTrip) {
        if (roundTrip > 0) {
            std::fputs("\n[tool round complete — LLM continuing]\n", stdout);
            std::fflush(stdout);
        }
        printer_.beginSegment();
    });

    printUserPrompt(cfg_.prompt());

    agent_->submitUserMessage(
        cfg_.prompt(),
        [this](const std::string& delta) { printer_.feed(delta); },
        [this](bool ok, const std::string& err) {
            printer_.finish();
            if (!ok) {
                LOGE("ToolCallDemo: %s", err.c_str());
            } else {
                LOGI("ToolCallDemo: tool-calling loop complete");
                if (agent_) {
                    printSessionTranscript(agent_->session(), "ToolCallDemo");
                }
            }
            queueInLoop([this]() { quit(); });
        });
}

// ---------------------------------------------------------------------------
// EvApp lifecycle
// ---------------------------------------------------------------------------

void ToolCallDemoApp::onStart() {
    registerTools();
    initRuntime();

    if (cfg_.mock()) {
        runMockAgent();
        return;
    }

    runLiveAgent();
}

void ToolCallDemoApp::onStop() {
    agent_.reset();
    runtime_.reset();
}

}  // namespace example
}  // namespace embedagent

int main(int argc, char* argv[]) {
    embedagent::example::ToolCallDemoConfig cfg;
    cfg.parse(argc, argv);

    embedagent::example::ToolCallDemoApp app(cfg);
    app.run();
    return 0;
}
