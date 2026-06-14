# EmbedAgent Roadmap

Planned work and non-goals as of **v0.1.0**.

## v0.1.x — Current

- `EaAgent` multi-turn loop with OpenAI-compatible chat completions
- `EaToolRegistry` / `EaToolBuilder` with JSON Schema
- `EaQueueCoordinator` offline queue + `EaConnectivityMonitor`
- Session persistence, secret resolver (CLI / env / file)
- Mock LLM mode for offline testing
- Three examples, nine unit tests
- `find_package(embedagent)` install export

## v0.2.x — Planned (High Priority)

| Item | Notes |
|------|-------|
| Fix `test_ea_llm_url` / DeepSeek URL resolution | Align test with `resolveChatCompletionsUrl` behaviour (`/v1/chat/completions`) |
| Tests for `EaConnectivityMonitor`, `EaPromptTemplate` | Close coverage gaps |
| `SECURITY.md` hardening guide → optional `EaSecretStore` TPM example | Document secure key storage pattern |
| Agent-level retry/backoff policy | Expose beyond libcurl retry |
| Token-aware session trim | Approximate token count before LLM call |
| Document `cancel()` usage | Usage guide + example for in-flight abort |

## v0.3.x — Planned (Medium Priority)

| Item | Notes |
|------|-------|
| Anthropic Messages API adapter | Native Claude format (today: OpenAI-compatible only) |
| Structured logging hook | Metrics callback for production observability |
| Concurrent session support | Multiple `EaAgent` instances documented; optional pool helper |
| MCP client (stdio) | Interop with external tool servers — evaluate scope vs. size |

## Explicit Non-Goals

| Topic | Reason |
|-------|--------|
| Local GGUF / llama.cpp inference | Use [agent.cpp](https://github.com/mozilla-ai/agent.cpp) or cloud API |
| Multi-agent orchestration | Keep library small; compose in application layer |
| Built-in RAG / vector DB | Application-specific |
| C++14+ migration | Embedded toolchain constraint |
| Windows / non-Linux ports | cppev is Linux-only |

## cppev Dependency

EmbedAgent releases track **cppev >= 0.1.0**. Breaking cppev API changes will
 bump embedagent minor/major accordingly. See
 [cppev ROADMAP](https://github.com/yjm6371/cppev/blob/main/ROADMAP.md).

## Post-Release Recommendations

See [docs/recommendations.en.md](docs/recommendations.en.md) for maintainer
and adopter checklists (CI, tagging, GitHub settings, production hardening).

## Feedback

Open issues with tag `enhancement`. Include target hardware and LLM provider.
