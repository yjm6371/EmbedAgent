# 发布后建议清单

面向维护者与生产环境集成方，在 v0.1.0 上线 GitHub 后的改进项。**非使用库的前置条件**，
但有助于长期维护与稳定部署。

## 维护者清单（GitHub）

### 仓库设置

- [ ] 替换全部 `YOUR_ORG` / `YOUR_NAME` / `YOUR_EMAIL` 占位符
- [ ] 确认默认分支为 `main`（若用 `master` 请在 README 说明）
- [ ] 启用 **GitHub Actions** CI
- [ ] 添加 **Topics**：`cpp11`、`embedded`、`llm`、`agent`、`iot`
- [ ] cppev 首次发布后固定 submodule（`git submodule add …`）
- [ ] 创建 **Release v0.1.0**，说明摘自 [CHANGELOG.md](../CHANGELOG.md)

### 分支保护（建议）

- 合并到 `main` 前要求 CI 通过
- 外部 PR 至少 1 人 review（单人维护可省略）

### Issue 模板（可选 P2）

- Bug：cppev 版本、构建参数、mock 还是 live API
- Feature：目标硬件 + LLM 服务商

## 集成方清单（生产环境）

### 构建与部署

- [ ] 按文档交叉编译，固定 cppev + embedagent 版本
- [ ] Release 构建并 strip（`-DCMAKE_BUILD_TYPE=Release`）
- [ ] OTA 前在 CI 跑 `ctest` 或 mock 模式 smoke test

### 安全

- [ ] 阅读 [SECURITY.md](../SECURITY.md) — 勿将 XOR 文件存储当作加密
- [ ] 非 root 运行 Agent；限制工具权限
- [ ] 仅 HTTPS；维护设备 CA 证书
- [ ] 设备 RMA 前清理 `session.json` 与日志

### 运维

- [ ] 监控离线队列目录大小
- [ ] 按模型上下文设置 `maxToolRoundTrips`、`maxMessages`
- [ ] 工具同步阻塞 loop 时配置 watchdog

## 已知技术债（v0.1.0）

| 项 | 影响 | 建议 |
|----|------|------|
| `test_ea_llm_url` DeepSeek 期望 URL | 测试可能与实现不一致 | 测试改为期望 `/v1/chat/completions` |
| cppev JSON 测试 + ASan | cJSON 路径泄漏报告 | 在 cppev 修复或测试 teardown 处理 |
| 联编 sanitizer 开关不一致 | 链接失败 | 同时关 `CPPEV_ENABLE_SANITIZERS` 与 `EA_ENABLE_SANITIZERS` |
| 按条数 trim 会话 | 长消息可能撑爆上下文 | v0.2 token 估算（见 ROADMAP） |

## 文档缺口（P2）

- GitHub Issue/PR 模板
- `build.cn.md` 随 `build.md` 变更同步维护
- 真机 `tool_call_demo` 演示（可选）
- 公共 API 的 Doxygen

## 社区与发布

- cppev / embedagent README 互相链接
- 用 mock 模式做快速开始对外宣传
- cppev 兼容变更时打 tag 并写 Release Notes

## 何时发布 v0.2.0

满足以下**任一**时可考虑 minor 版本：

- URL 解析 / 失败测试修复
- 连通性、Prompt 等新测试合并
- Token trim 或 Anthropic 适配落地
- 密钥或 TLS 相关安全修复

语义化版本：文档与小修复 → patch；向后兼容功能 → minor；破坏性 API 或 cppev major → major。
