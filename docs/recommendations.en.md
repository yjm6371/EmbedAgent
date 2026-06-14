# Post-Release Recommendations

Checklists for maintainers and production adopters after the v0.1.0 GitHub
launch. These items are **not required** to use the library but improve
long-term project health.

## Maintainer Checklist (GitHub)

### Repository settings

- [ ] Replace all `YOUR_ORG` / `YOUR_NAME` / `YOUR_EMAIL` placeholders
- [ ] Set default branch to `main` (or document if using `master`)
- [ ] Enable **GitHub Actions** for CI workflows
- [ ] Add repository **Topics**: `cpp11`, `embedded`, `llm`, `agent`, `iot`
- [ ] Pin cppev submodule after first release (`git submodule add …`)
- [ ] Create **Release v0.1.0** with notes from [CHANGELOG.md](CHANGELOG.md)

### Branch protection (recommended)

- Require CI pass before merge to `main`
- Require 1 review for external PRs (optional for solo maintainer)

### Issue templates (optional P2)

- Bug report (cppev version, build flags, mock vs live API)
- Feature request (hardware + provider)

## Adopter Checklist (Production)

### Build and deploy

- [ ] Cross-compile with documented sysroot; pin cppev + embedagent versions
- [ ] Strip symbols in Release (`-DCMAKE_BUILD_TYPE=Release`)
- [ ] Run `ctest` or smoke `chat_agent` mock mode in CI before OTA

### Security

- [ ] Read [SECURITY.md](SECURITY.md) — do not rely on XOR file storage for keys
- [ ] Run agent as non-root; restrict tool permissions
- [ ] Use HTTPS only; maintain CA certificates on device
- [ ] Scrub `session.json` and logs before device RMA

### Operations

- [ ] Monitor offline queue depth (`EaFileOfflineQueue` directory size)
- [ ] Set `maxToolRoundTrips` and `maxMessages` for your model context
- [ ] Implement watchdog if loop thread blocked by synchronous tools

## Known Technical Debt (v0.1.0)

| Item | Impact | Suggested fix |
|------|--------|---------------|
| `test_ea_llm_url` DeepSeek expectation | CI may fail with sanitizers on | Update test to expect `/v1/chat/completions` |
| cppev JSON tests + ASan | Leak reports in cJSON duplicate paths | Fix or suppress in test teardown; track in cppev |
| cppev sanitizer mismatch in subdir build | Link errors if EA/CPPEV flags differ | Pass `-DCPPEV_ENABLE_SANITIZERS=OFF` with `-DEA_ENABLE_SANITIZERS=OFF` |
| Session trim by message count | Context overflow on long messages | Token estimate in v0.2 (ROADMAP) |

## Documentation Gaps (P2)

- GitHub Issue/PR templates
- `docs/build.cn.md` — maintained alongside `build.md` on build changes
- Video or GIF of `tool_call_demo` on real hardware (optional)
- Doxygen for public `ea_*.h` APIs

## Community

- Link embedagent from cppev README and vice versa
- Announce on embedded / edge-AI forums with mock-mode quick start
- Tag releases when cppev ABI-compatible bumps occur

## When to Cut v0.2.0

Consider a minor release when **any** of:

- URL resolution / failing test fixed
- New connectivity or prompt tests merged
- Token-aware trim or Anthropic adapter shipped
- Security fix in secret storage or TLS path

Use semver: patch for docs/fixes, minor for backward-compatible features,
major for breaking API or cppev major bump.
