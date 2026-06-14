// ea_demo_io.h — stdout chat I/O helpers for EmbedAgent CLI examples
//
// Logs go to stderr (configure EvConfig::setDefaultLogSink("stderr")).
// User-facing chat text goes to stdout only.
#pragma once

#include <ea_session.h>

#include <cstdio>
#include <string>

namespace embedagent {

class EaStreamPrinter {
public:
    // Starts a new assistant reply segment (e.g. after a tool-call round).
    void beginSegment();

    void feed(const std::string& delta);
    void finish();

private:
    bool segmentStarted_ {false};
    bool wroteContent_   {false};
};

void printUserPrompt(const std::string& prompt);
void printSessionTranscript(const EaSession& session, const char* tag);

}  // namespace embedagent
