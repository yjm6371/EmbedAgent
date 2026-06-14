// chat_agent.cpp — EmbedAgent complete embedded device agent demo
//
// Demonstrates: multi-tool calling, streaming, session persistence,
//               API key management, and realistic embedded device control.
//
// Registered tools:
//   get_device_status  — uptime, temperature, free memory, load average
//   set_gpio_pin       — set a GPIO output pin high (1) or low (0)
//   read_sensor        — read a named sensor value in a chosen unit
//   get_network_info   — IP address, hostname, link state
//
// Usage (offline mock):
//   ./chat_agent --mock
//
// Usage (live):
//   ./chat_agent --url=https://api.deepseek.com --api-key=KEY --model=deepseek-chat
//   ./chat_agent --url=https://api.openai.com   --api-key=KEY --model=gpt-4o-mini
//   ./chat_agent --url=... --api-key=KEY --prompt="Read sensor 2 in Celsius, then set GPIO 5 high"
//
// Session persistence:
//   ./chat_agent --url=... --api-key=KEY --save-api-key --persist-session
//
// Chat text  -> stdout
// Logs       -> stderr  (default)

#include <ea_demo_io.h>
#include <ea_runtime_config.h>
#include <ea_tool_builder.h>
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>
#include <ev_logger.h>

#include <memory>
#include <string>
#include <vector>

namespace embedagent {
namespace example {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

class ChatAgentConfig : public cppev::framework::EvConfig {
public:
    ChatAgentConfig() { setDefaultLogSink("stderr"); }

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
        std::puts("  --mock              run offline with a built-in fixture");
        std::puts("  --data-dir=PATH     storage root   (default: /tmp/embedagent)");
        std::puts("  --save-api-key      persist --api-key to the file store");
        std::puts("  --persist-session   load/save session.json across runs");
        std::puts("  --url=URL           API base URL or full chat/completions URL");
        std::puts("  --api-key=KEY       bearer token  (or EMBEDAGENT_API_KEY / file)");
        std::puts("  --model=NAME        model name    (default: gpt-4o-mini)");
        std::puts("  --prompt=TEXT       first user message");
        std::puts("");
        std::puts("  Chat text -> stdout; logs -> stderr.");
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
        "Check the device status and read sensor 1 in Celsius. "
        "If the temperature is above 40 C, set GPIO pin 5 to high."};
};

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

class ChatAgentApp : public cppev::framework::EvApp {
public:
    explicit ChatAgentApp(const ChatAgentConfig& cfg);
    ~ChatAgentApp() override;

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
    void runChat(EaAgentOptions opts);

    ChatAgentConfig                  cfg_;
    EaToolRegistry                   tools_;
    std::unique_ptr<EaRuntimeConfig> runtime_;
    std::unique_ptr<EaAgent>         agent_;
    EaStreamPrinter                  printer_;
};

ChatAgentApp::ChatAgentApp(const ChatAgentConfig& cfg)
    : cppev::framework::EvApp(cfg)
    , cfg_(cfg)
{}

ChatAgentApp::~ChatAgentApp() {}

// ---------------------------------------------------------------------------
// Tool registration — four embedded-device tools
// ---------------------------------------------------------------------------

void ChatAgentApp::registerTools() {
    // get_device_status: no parameters
    tools_.registerTool(
        "get_device_status",
        "Return the embedded device's current uptime (seconds), CPU temperature "
        "(Celsius), free memory (KB), and 1-minute load average.",
        {},
        [](const EaToolArgs& /*args*/, std::string* out) {
            LOGI("ChatAgent: [local] get_device_status()");
            *out = "{\"uptime_sec\":7200,\"temp_c\":43,"
                   "\"mem_free_kb\":61440,\"load_avg\":0.42}";
            return true;
        });

    // set_gpio_pin: pin number + level (0 or 1)
    std::vector<std::string> levels;
    levels.push_back("0");
    levels.push_back("1");

    EaToolSpec gpioSpec = EaToolBuilder()
        .name("set_gpio_pin")
        .description("Set an embedded GPIO output pin to high (1) or low (0).")
        .param("pin",   EaParamType::kInteger,
               "GPIO pin number (0-based, BCM numbering)", true)
        .enumParam("value", EaParamType::kInteger,
                   "Output level: 0 = low, 1 = high", levels, true)
        .handler([](const EaToolArgs& args, std::string* out) {
            int pin = args.getInt("pin", -1);
            int val = args.getInt("value", -1);
            LOGI("ChatAgent: [local] set_gpio_pin(pin=%d, value=%d)", pin, val);
            if (pin < 0 || (val != 0 && val != 1)) {
                *out = "{\"ok\":false,\"error\":\"invalid pin or value\"}";
                return true;
            }
            *out = "{\"ok\":true,\"pin\":";
            *out += std::to_string(pin);
            *out += ",\"value\":";
            *out += std::to_string(val);
            *out += "}";
            return true;
        })
        .build();
    tools_.registerTool(gpioSpec);

    // read_sensor: sensor ID + unit
    std::vector<std::string> units;
    units.push_back("celsius");
    units.push_back("fahrenheit");
    units.push_back("raw");

    EaToolSpec sensorSpec = EaToolBuilder()
        .name("read_sensor")
        .description("Read a value from a connected sensor (temperature, humidity, "
                     "pressure, etc.) and return it in the requested unit.")
        .param("sensor_id", EaParamType::kInteger,
               "Sensor identifier (1-based index)", true)
        .enumParam("unit", EaParamType::kString,
                   "Unit for the returned value: celsius, fahrenheit, or raw",
                   units, false)
        .handler([](const EaToolArgs& args, std::string* out) {
            int         sid  = args.getInt("sensor_id", 1);
            std::string unit = args.getString("unit", "celsius");
            LOGI("ChatAgent: [local] read_sensor(id=%d, unit=%s)",
                 sid, unit.c_str());
            // Simulated sensor values
            double value = 38.5 + sid * 1.2;
            if (unit == "fahrenheit") {
                value = value * 9.0 / 5.0 + 32.0;
            } else if (unit == "raw") {
                value = 512 + sid * 10;
            }
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "{\"sensor_id\":%d,\"value\":%.1f,\"unit\":\"%s\"}",
                          sid, value, unit.c_str());
            *out = buf;
            return true;
        })
        .build();
    tools_.registerTool(sensorSpec);

    // get_network_info: no parameters
    tools_.registerTool(
        "get_network_info",
        "Return the device's primary network interface IP address, hostname, "
        "and whether the link is currently up.",
        {},
        [](const EaToolArgs& /*args*/, std::string* out) {
            LOGI("ChatAgent: [local] get_network_info()");
            *out = "{\"hostname\":\"embed-device-01\","
                   "\"ip\":\"192.168.1.42\","
                   "\"link_up\":true}";
            return true;
        });

    LOGI("ChatAgent: registered tools: get_device_status, set_gpio_pin, "
         "read_sensor, get_network_info");
}

// ---------------------------------------------------------------------------
// Runtime / options helpers
// ---------------------------------------------------------------------------

void ChatAgentApp::initRuntime() {
    EaRuntimeConfigOptions ropts;
    ropts.dataDir        = cfg_.dataDir();
    ropts.cliApiKey      = cfg_.apiKey();
    ropts.saveApiKey     = cfg_.saveApiKey();
    ropts.persistSession = cfg_.persistSession();
    runtime_.reset(new EaRuntimeConfig(ropts));
}

void ChatAgentApp::applyRuntime(EaAgentOptions* opts, bool needApiKey) {
    if (!opts || !runtime_) {
        return;
    }
    opts->storage        = &runtime_->storage;
    opts->persistSession = runtime_->persistSession;

    if (!needApiKey) {
        return;
    }

    std::string    apiKey;
    EaSecretSource source = EaSecretSource::kNone;
    if (runtime_->resolveApiKey(&apiKey, &source)) {
        opts->llm.apiKey = apiKey;
        if (source == EaSecretSource::kFile) {
            LOGI("ChatAgent: API key loaded from file store");
        } else if (source == EaSecretSource::kEnv) {
            LOGI("ChatAgent: API key loaded from environment variable");
        }
    }
}

void ChatAgentApp::buildSystemPrompt(EaAgentOptions* opts) {
    EaPromptTemplate tmpl;
    tmpl.setTemplate(
        "You are {{device_name}}, an intelligent embedded Linux device agent. "
        "You have access to hardware monitoring and control tools. "
        "When the user asks about hardware status, sensors, GPIO, or networking, "
        "always call the appropriate tool rather than guessing. "
        "After receiving tool results, give a concise, technical summary. "
        "Prefer SI units. State any anomalies explicitly.");
    opts->systemTemplate = tmpl;
    opts->deviceName     = "embed-agent-v1";
}

// ---------------------------------------------------------------------------
// Mock agent — full offline two-round tool-calling fixture
// ---------------------------------------------------------------------------

void ChatAgentApp::runMockAgent() {
    EaAgentOptions opts;
    opts.llm.model  = "mock-model";
    opts.llm.stream = false;

    // Round 1: LLM calls get_device_status and read_sensor simultaneously.
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":["
          "{\"id\":\"call_1\",\"type\":\"function\",\"function\":"
            "{\"name\":\"get_device_status\",\"arguments\":\"{}\"}},"
          "{\"id\":\"call_2\",\"type\":\"function\",\"function\":"
            "{\"name\":\"read_sensor\","
            "\"arguments\":\"{\\\"sensor_id\\\":1,\\\"unit\\\":\\\"celsius\\\"}\"}}"
        "]},"
        "\"finish_reason\":\"tool_calls\"}]}");

    // Round 2: LLM sees tool results and decides to set GPIO, then summarises.
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":["
          "{\"id\":\"call_3\",\"type\":\"function\",\"function\":"
            "{\"name\":\"set_gpio_pin\","
            "\"arguments\":\"{\\\"pin\\\":5,\\\"value\\\":1}\"}}"
        "]},"
        "\"finish_reason\":\"tool_calls\"}]}");

    // Round 3: final natural-language summary.
    opts.llm.mockResponses.push_back(
        "{\"choices\":[{\"message\":{\"role\":\"assistant\","
        "\"content\":\"Device is healthy: uptime 7200 s, CPU 43 C, "
        "61440 KB free, load 0.42. Sensor 1 reads 39.7 C. "
        "Temperature exceeds 40 C threshold — GPIO pin 5 has been set high. "
        "No anomalies in memory or load.\"},"
        "\"finish_reason\":\"stop\"}]}");

    buildSystemPrompt(&opts);
    applyRuntime(&opts, false);
    runChat(opts);
}

// ---------------------------------------------------------------------------
// Live agent — connects to a real LLM API
// ---------------------------------------------------------------------------

void ChatAgentApp::runLiveAgent() {
    if (cfg_.url().empty()) {
        LOGW("ChatAgent: --url is required (or use --mock for offline mode)");
        queueInLoop([this]() { quit(); });
        return;
    }

    EaAgentOptions opts;
    opts.llm.url    = cfg_.url();
    opts.llm.model  = cfg_.model();
    opts.llm.stream = true;
    applyRuntime(&opts, true);

    if (opts.llm.apiKey.empty()) {
        LOGW("ChatAgent: no API key — pass --api-key, set EMBEDAGENT_API_KEY, "
             "or use --save-api-key on a previous run");
        queueInLoop([this]() { quit(); });
        return;
    }

    // Friendly reminder when a DeepSeek URL is used with the default model.
    if (cfg_.url().find("deepseek") != std::string::npos &&
        cfg_.model() == "gpt-4o-mini") {
        LOGW("ChatAgent: DeepSeek endpoint detected; "
             "consider --model=deepseek-chat");
    }

    buildSystemPrompt(&opts);
    runChat(opts);
}

// ---------------------------------------------------------------------------
// Core chat runner
// ---------------------------------------------------------------------------

void ChatAgentApp::runChat(EaAgentOptions opts) {
    agent_.reset(new EaAgent(loop(), opts));
    agent_->setToolRegistry(&tools_);

    // Print a header line before each assistant segment (between tool rounds).
    agent_->setRoundStartCallback([this](int roundTrip) {
        if (roundTrip > 0) {
            std::fputs("\n[tool round complete — continuing]\n", stdout);
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
                LOGE("ChatAgent: %s", err.c_str());
            } else {
                LOGI("ChatAgent: conversation complete");
                if (agent_) {
                    printSessionTranscript(agent_->session(), "ChatAgent");
                }
            }
            queueInLoop([this]() { quit(); });
        });
}

// ---------------------------------------------------------------------------
// EvApp lifecycle
// ---------------------------------------------------------------------------

void ChatAgentApp::onStart() {
    registerTools();
    initRuntime();

    if (cfg_.mock()) {
        runMockAgent();
        return;
    }

    runLiveAgent();
}

void ChatAgentApp::onStop() {
    agent_.reset();
    runtime_.reset();
}

}  // namespace example
}  // namespace embedagent

int main(int argc, char* argv[]) {
    embedagent::example::ChatAgentConfig cfg;
    cfg.parse(argc, argv);

    embedagent::example::ChatAgentApp app(cfg);
    app.run();
    return 0;
}
