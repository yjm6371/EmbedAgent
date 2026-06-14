[中文文档](README.cn.md)

# EmbedAgent

![CI](https://github.com/yjm6371/embedagent/actions/workflows/ci.yml/badge.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**EmbedAgent** is a **C++11** LLM agent library for **embedded Linux**, built on
[cppev](https://github.com/yjm6371/cppev). It calls cloud LLM APIs
(OpenAI-compatible chat completions), runs a local tool-calling loop, buffers
messages when offline, and persists sessions — all on a single event-loop
thread.

## Features

- **Agent loop** — user message → LLM → tool calls → LLM → final reply
- **Local tools** — register C++ handlers with JSON Schema via `EaToolRegistry`
- **Offline queue** — `EaQueueCoordinator` enqueues messages when the network
  is down and flushes on reconnect
- **Session persistence** — optional JSON save/load of conversation history
- **Secret resolution** — API key from CLI, environment, or obfuscated file
- **Mock mode** — test the full agent loop without network or API keys
- **Strict C++11** — no exceptions; loop-thread-only APIs

## Platform Support

| Requirement | Version |
|-------------|---------|
| OS | Linux only |
| cppev | >= 0.1.0 |
| C++ standard | C++11 |

> **Security note:** file-stored API keys use XOR obfuscation, not encryption.
> See [SECURITY.md](SECURITY.md) for production guidance.

## cppev Dependency

EmbedAgent requires [cppev](https://github.com/yjm6371/cppev). Three ways to
provide it:

| Method | How |
|--------|-----|
| Side-by-side checkout | `-DCPPEV_SOURCE_DIR=../cppev` (CMake preset default) |
| Git submodule | `git clone --recurse-submodules` → `external/cppev/` |
| Installed package | `cmake --install` cppev, then `find_package(cppev)` |

See [docs/build.md](docs/build.md) or [docs/build.cn.md](docs/build.cn.md) for full build scenarios.

## Quick Start

### Build

Place cppev next to embedagent (or set `CPPEV_SOURCE_DIR`):

```
workspace/
├── cppev/
└── embedagent/
```

```bash
cd embedagent
cmake --preset host-debug
cmake --build --preset host-debug
ctest --test-dir build/host-debug --output-on-failure
```

### Mock mode (no API key)

See [docs/usage.en.md](docs/usage.en.md) §2 for the full Hello World example.
The agent uses canned LLM responses while local tools still execute normally.

### Real LLM API

After building examples:

```bash
./build/host-release/examples/chat_agent \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --model=deepseek-chat \
    --prompt="Hello"
```

## Architecture

```
EvApp (cppev)
    └── EaQueueCoordinator (optional offline buffering)
            └── EaAgent
                    ├── EaLlmClient   (HTTP / SSE)
                    ├── EaSession     (history + trim)
                    └── EaToolRegistry (local tools)
```

## Documentation

| Document | Description |
|----------|-------------|
| [Usage (EN)](docs/usage.en.md) | API reference, tools, offline queue, secrets |
| [Usage (CN)](docs/usage.cn.md) | 中文使用说明 |
| [Design (EN)](docs/design.en.md) | Architecture and design decisions |
| [Design (CN)](docs/design.cn.md) | 中文设计说明书 |
| [Build guide (EN)](docs/build.md) | Co-build, submodule, installed cppev |
| [构建指南（中文）](docs/build.cn.md) | 联合编译、submodule、安装 cppev |
| [Contributing](CONTRIBUTING.md) | Build, test, PR guidelines |
| [Security](SECURITY.md) | API keys, tools, network trust |
| [Roadmap](ROADMAP.md) | Planned features and non-goals |
| [Post-release recommendations](docs/recommendations.en.md) | Maintainer and production checklists |

## Examples

| Program | Description |
|---------|-------------|
| `chat_agent` | Full chat agent with tools and session persistence |
| `tool_call_demo` | Embedded Linux system tools (GPIO, thermal, network, …) |
| `offline_queue_demo` | Offline message queue and reconnect flush |

## License

MIT — see [LICENSE](LICENSE).
