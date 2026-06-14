// offline_queue_demo.cpp — EmbedAgent offline queue demo
#include <ea_demo_io.h>
#include <ea_runtime_config.h>
#include <embed_agent.h>
#include <ev_app.h>
#include <ev_config.h>
#include <ev_logger.h>

#include <memory>
#include <string>

namespace embedagent {
namespace example {

class OfflineQueueDemoConfig : public cppev::framework::EvConfig {
public:
    OfflineQueueDemoConfig() { setDefaultLogSink("stderr"); }

    bool mock() const { return mock_; }
    bool offline() const { return offline_; }
    bool flushOnly() const { return flushOnly_; }
    const std::string& dataDir() const { return dataDir_; }
    const std::string& url() const { return url_; }
    const std::string& apiKey() const { return apiKey_; }
    const std::string& model() const { return model_; }
    const std::string& prompt() const { return prompt_; }
    bool saveApiKey() const { return saveApiKey_; }
    bool persistSession() const { return persistSession_; }

protected:
    void printUsage() const override {
        cppev::framework::EvConfig::printUsage();
        std::puts("  --data-dir=PATH     storage root (default: /tmp/embedagent)");
        std::puts("  --save-api-key      persist --api-key to file store");
        std::puts("  --persist-session   load/save session.json across runs");
        std::puts("  --offline           start offline; enqueue instead of calling LLM");
        std::puts("  --flush-only        drain persisted queue when online");
        std::puts("  --mock              use mock LLM responses during flush");
        std::puts("  --url=URL           API base or chat/completions URL");
        std::puts("  --api-key=KEY       bearer token (or EMBEDAGENT_API_KEY / file)");
        std::puts("  --model=NAME        model name (default: gpt-4o-mini)");
        std::puts("  --prompt=TEXT       user prompt");
        std::puts("");
        std::puts("  Chat text -> stdout; logs -> stderr (default).");
    }

    bool parseOption(const std::string& key,
                     const std::string& value) override {
        if (key == "mock") {
            mock_ = true;
            return true;
        }
        if (key == "offline") {
            offline_ = true;
            return true;
        }
        if (key == "flush-only") {
            flushOnly_ = true;
            return true;
        }
        if (key == "data-dir") {
            dataDir_ = value;
            return true;
        }
        if (key == "save-api-key") {
            saveApiKey_ = true;
            return true;
        }
        if (key == "persist-session") {
            persistSession_ = true;
            return true;
        }
        if (key == "url") {
            url_ = value;
            return true;
        }
        if (key == "api-key") {
            apiKey_ = value;
            return true;
        }
        if (key == "model") {
            model_ = value;
            return true;
        }
        if (key == "prompt") {
            prompt_ = value;
            return true;
        }
        return cppev::framework::EvConfig::parseOption(key, value);
    }

private:
    bool        mock_ {false};
    bool        offline_ {false};
    bool        flushOnly_ {false};
    bool        saveApiKey_ {false};
    bool        persistSession_ {false};
    std::string dataDir_ {"/tmp/embedagent"};
    std::string url_;
    std::string apiKey_;
    std::string model_ {"gpt-4o-mini"};
    std::string prompt_ {"What is the device status?"};
};

class OfflineQueueDemoApp : public cppev::framework::EvApp {
public:
    explicit OfflineQueueDemoApp(const OfflineQueueDemoConfig& cfg);
    ~OfflineQueueDemoApp() override;

protected:
    void onStart() override;
    void onStop() override;

private:
    EaAgentOptions buildAgentOptions();
    void initRuntime();
    void runDemo();

    OfflineQueueDemoConfig                    cfg_;
    EaToolRegistry                            tools_;
    std::unique_ptr<EaRuntimeConfig>          runtime_;
    std::unique_ptr<EaQueueCoordinator>       coordinator_;
    EaStreamPrinter                           printer_;
};

OfflineQueueDemoApp::OfflineQueueDemoApp(const OfflineQueueDemoConfig& cfg)
    : cppev::framework::EvApp(cfg)
    , cfg_(cfg)
{}

OfflineQueueDemoApp::~OfflineQueueDemoApp() {}

void OfflineQueueDemoApp::onStart() {
    EaToolSpec spec;
    spec.def.name = "get_device_status";
    spec.def.description = "Return device uptime and temperature";
    spec.def.parametersJson = "{\"type\":\"object\",\"properties\":{}}";
    spec.handler = [](const std::string& /*args*/, std::string* out) {
        *out = "{\"uptime_sec\":3600,\"temp_c\":42}";
        return true;
    };
    tools_.registerTool(spec);

    runDemo();
}

void OfflineQueueDemoApp::initRuntime() {
    EaRuntimeConfigOptions ropts;
    ropts.dataDir = cfg_.dataDir();
    ropts.cliApiKey = cfg_.apiKey();
    ropts.saveApiKey = cfg_.saveApiKey();
    ropts.persistSession = cfg_.persistSession();
    runtime_.reset(new EaRuntimeConfig(ropts));
}

EaAgentOptions OfflineQueueDemoApp::buildAgentOptions() {
    EaAgentOptions opts;

    if (cfg_.mock()) {
        opts.llm.model = "mock-model";
        opts.llm.stream = false;
        opts.llm.mockResponses.push_back(
            "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
            "\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\","
            "\"function\":{\"name\":\"get_device_status\",\"arguments\":\"{}\"}}]},"
            "\"finish_reason\":\"tool_calls\"}]}");
        opts.llm.mockResponses.push_back(
            "{\"choices\":[{\"message\":{\"role\":\"assistant\","
            "\"content\":\"Device is healthy (mock flush).\"},"
            "\"finish_reason\":\"stop\"}]}");
    } else {
        opts.llm.url = cfg_.url();
        opts.llm.model = cfg_.model();
        opts.llm.stream = true;
        if (runtime_) {
            runtime_->resolveApiKey(&opts.llm.apiKey);
        }
    }

    if (runtime_) {
        opts.storage = &runtime_->storage;
        opts.persistSession = runtime_->persistSession;
    }

    EaPromptTemplate tmpl;
    tmpl.setTemplate(
        "You are {{device_name}}, an embedded assistant with offline queue.");
    opts.systemTemplate = tmpl;
    opts.deviceName = "offline-queue-demo";
    return opts;
}

void OfflineQueueDemoApp::runDemo() {
    initRuntime();

    EaQueueCoordinatorOptions coordOpts;
    coordOpts.autoFlushOnOnline = false;

    std::unique_ptr<EaFileOfflineQueue> queue(
        new EaFileOfflineQueue(cfg_.dataDir()));
    std::unique_ptr<EaConnectivityMonitor> monitor(
        new EaConnectivityMonitor(loop()));

    coordinator_.reset(new EaQueueCoordinator(
        loop(), buildAgentOptions(), coordOpts,
        std::move(queue), std::move(monitor)));

    coordinator_->setToolRegistry(&tools_);
    coordinator_->setRoundStartCallback([this](int /*roundTrip*/) {
        printer_.beginSegment();
    });

    LOGI("OfflineQueueDemo: data-dir=%s pending=%zu",
         cfg_.dataDir().c_str(), coordinator_->pendingCount());

    if (cfg_.offline()) {
        coordinator_->connectivity().setOnline(false);
    } else if (!cfg_.mock()) {
        EaAgentOptions probe = buildAgentOptions();
        if (cfg_.url().empty() || probe.llm.apiKey.empty()) {
            LOGW("OfflineQueueDemo: --url and API key required (or --mock)");
            queueInLoop([this]() { quit(); });
            return;
        }
    }

    if (cfg_.flushOnly()) {
        if (coordinator_->pendingCount() == 0) {
            LOGI("OfflineQueueDemo: queue empty, nothing to flush");
            queueInLoop([this]() { quit(); });
            return;
        }

        LOGI("OfflineQueueDemo: flushing %zu queued item(s)",
             coordinator_->pendingCount());

        coordinator_->flush(
            [this](const std::string& delta) { printer_.feed(delta); },
            [this](bool ok, const std::string& err) {
                printer_.finish();
                if (!ok) {
                    LOGE("OfflineQueueDemo: flush failed: %s", err.c_str());
                } else {
                    LOGI("OfflineQueueDemo: flush complete");
                    printSessionTranscript(coordinator_->agent().session(),
                                           "OfflineQueueDemo");
                }
                queueInLoop([this]() { quit(); });
            });
        return;
    }

    printUserPrompt(cfg_.prompt());

    coordinator_->submitUserMessage(
        cfg_.prompt(),
        [this](const std::string& delta) { printer_.feed(delta); },
        [this](bool ok, const std::string& err) {
            printer_.finish();
            if (!ok) {
                LOGE("OfflineQueueDemo: %s", err.c_str());
            } else if (err == "queued") {
                LOGI("OfflineQueueDemo: message queued (pending=%zu)",
                     coordinator_->pendingCount());
            } else {
                LOGI("OfflineQueueDemo: request complete");
                printSessionTranscript(coordinator_->agent().session(),
                                       "OfflineQueueDemo");
            }
            queueInLoop([this]() { quit(); });
        });
}

void OfflineQueueDemoApp::onStop() {
    coordinator_.reset();
    runtime_.reset();
}

}  // namespace example
}  // namespace embedagent

int main(int argc, char* argv[]) {
    embedagent::example::OfflineQueueDemoConfig cfg;
    cfg.parse(argc, argv);

    embedagent::example::OfflineQueueDemoApp app(cfg);
    app.run();
    return 0;
}
