# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-06-13

### Added

- **EaAgent** — multi-turn agent orchestration with tool-calling loop
- **EaLlmClient** — OpenAI-compatible chat completions (HTTP + SSE streaming)
- **EaToolRegistry / EaToolBuilder** — local tool registration with JSON Schema
- **EaSession / EaSessionCodec** — conversation history, trim, JSON persistence
- **EaQueueCoordinator** — offline message queue with reconnect flush
- **EaConnectivityMonitor** — online/offline state and optional HTTP probe
- **EaSecretResolver** — three-level API key resolution (CLI / env / file)
- **EaRuntimeConfig** — startup bootstrap for storage and secrets
- Three example programs: `chat_agent`, `tool_call_demo`, `offline_queue_demo`
- Nine GTest unit tests
- CMake presets and cross-compilation toolchains
- Design, usage, and build documentation (English and Chinese usage/design)

### Dependencies

- Requires [cppev](https://github.com/yjm6371/cppev) >= 0.1.0

[0.1.0]: https://github.com/yjm6371/embedagent/releases/tag/v0.1.0
