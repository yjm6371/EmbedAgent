# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | Yes       |

## Reporting a Vulnerability

**Do not file public issues for security vulnerabilities.**

Contact maintainers privately:

- Email: `yjm6371@163.com`
- GitHub private security advisory

We aim to acknowledge within **7 days** and ship a fix or mitigation within
**30 days** for confirmed issues.

## Threat Model

EmbedAgent is designed for **embedded Linux devices** that call **cloud LLM
APIs** and execute **local tools** (shell, GPIO, file I/O in examples). Typical
deployment: factory gateway, edge controller, on-prem appliance — not a
multi-tenant SaaS.

Assume:

- Attackers may control **network path** (MITM) if TLS is misconfigured
- Attackers with **filesystem access** can read obfuscated API keys
- **LLM prompt injection** can influence tool calls if tools are over-privileged

## API Key Storage

`EaFileSecretStore` uses **XOR obfuscation**, not encryption.

| Storage | Security level |
|---------|----------------|
| CLI `--api-key` | Process memory only; visible in `ps` on some systems |
| Env `EMBEDAGENT_API_KEY` | Visible to same-user processes |
| File `{dataDir}/api_key` | Obfuscated at rest; **recoverable** with library code |

**Recommendations for production:**

1. Prefer **OS secure storage** (TPM, keystore) via custom `EaSecretStore` impl
2. Restrict `{dataDir}` permissions (`chmod 700`)
3. Never commit `api_key` or logs containing bearer tokens
4. Rotate keys if a device is decommissioned or imaged

## Network Security

- Always use **HTTPS** for LLM endpoints (`https://…`)
- Validate TLS via system CA store; pin certificates only with a rotation plan
- `EaConnectivityMonitor` HTTP probe uses GET with timeout — do not point at
  sensitive internal URLs without authentication

## Tool Execution Risks

Examples register powerful tools (process list, network config, GPIO). In
production:

- Register **minimum necessary** tools with strict argument validation
- Never expose raw shell to the model without sandboxing
- Run the agent process with **least privilege** (non-root user, capabilities)
- Blocking tool handlers on the loop thread can cause DoS — offload via
  `EvApp::runAsync`

## Session Data

`session.json` may contain user prompts and tool outputs. Treat as **sensitive**
if prompts include PII or credentials. Encrypt at rest if required by policy.

## LLM Provider Trust

EmbedAgent sends conversation history and tool schemas to the configured
cloud API. Review provider data retention and compliance (GDPR, etc.) before
production deployment.

## Dependency Security

- Keep **cppev**, **libcurl**, and **OpenSSL** updated in your sysroot
- See [cppev SECURITY.md](https://github.com/yjm6371/cppev/blob/main/SECURITY.md)
  for TLS and HTTP client notes

## Known Limitations (v0.1.0)

- No built-in rate limiting or spend caps
- No prompt-injection filtering
- Single in-flight request per `EaAgent` instance (no request queue hardening)
- Session trim by **message count**, not token budget (may exceed model context)

These are tracked in [ROADMAP.md](ROADMAP.md).
