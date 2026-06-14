# Contributing to EmbedAgent

Thank you for contributing to EmbedAgent. This library builds on
[cppev](https://github.com/yjm6371/cppev); read both projects' design docs
before large changes.

## Before You Start

- [docs/design.en.md](docs/design.en.md) / [docs/design.cn.md](docs/design.cn.md)
- [docs/build.md](docs/build.md) / [docs/build.cn.md](docs/build.cn.md) for cppev
  dependency setup
- **C++11 only** — same constraints as cppev
- **call in loop thread only** — public APIs run on `EvEventLoop` thread; use
  `EvApp::runAsync` for blocking work in tool handlers
- **No exceptions** in library code

## Development Setup

Side-by-side checkout (recommended):

```
workspace/
├── cppev/
└── embedagent/
```

```bash
cd embedagent
cmake --preset host-debug -DCPPEV_ENABLE_SANITIZERS=OFF -DEA_ENABLE_SANITIZERS=OFF
cmake --build --preset host-debug
ctest --test-dir build/host-debug --output-on-failure
```

Without Ninja:

```bash
cmake -S embedagent -B embedagent/build -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCPPEV_SOURCE_DIR=/path/to/cppev \
      -DCPPEV_ENABLE_SANITIZERS=OFF \
      -DEA_ENABLE_SANITIZERS=OFF \
      -DEA_BUILD_TESTS=ON
cmake --build embedagent/build -j$(nproc)
ctest --test-dir embedagent/build --output-on-failure
```

Run examples:

```bash
./build/host-release/examples/chat_agent --help
./build/host-debug/tests/test_ea_agent_mock
```

## Pull Request Checklist

- [ ] Builds with cppev via `CPPEV_SOURCE_DIR` or installed `find_package(cppev)`
- [ ] Unit tests pass (`ctest`)
- [ ] Mock-mode tests pass without network/API keys
- [ ] New tools or agent behaviour documented in `docs/usage.*.md`
- [ ] No secrets committed (use `.gitignore` patterns for `api_key`, `session.json`)
- [ ] English comments only in source files

## Code Layout

| Directory | Purpose |
|-----------|---------|
| `include/` | Public headers (`ea_*.h`, `embed_agent.h`) |
| `src/` | Implementation |
| `examples/` | `chat_agent`, `tool_call_demo`, `offline_queue_demo` |
| `tests/` | GTest (session, agent mock, offline queue, secrets, …) |

## Commit Messages

```
add EaConnectivityMonitor probe interval option
fix session rollback on partial LLM failure
docs: Chinese build guide for Scenario B
```

## Reporting Issues

Include cppev version, LLM provider URL (redact API keys), and whether mock mode
reproduces the issue.

Security issues: [SECURITY.md](SECURITY.md).

## License

Contributions are licensed under the [MIT License](LICENSE).
