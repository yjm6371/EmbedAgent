[English](README.md)

# EmbedAgent

![CI](https://github.com/yjm6371/embedagent/actions/workflows/ci.yml/badge.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**EmbedAgent** 是一个面向**嵌入式 Linux** 的 **C++11** LLM Agent 库，构建于
[cppev](https://github.com/yjm6371/cppev) 之上。它调用云端大语言模型 API
（OpenAI 兼容 Chat Completions），在单事件循环线程上完成本地工具调用、离线
消息缓冲与会话持久化。

## 特性

- **Agent 循环** — 用户消息 → LLM → 工具调用 → LLM → 最终回复
- **本地工具** — 通过 `EaToolRegistry` 注册带 JSON Schema 的 C++ handler
- **离线队列** — `EaQueueCoordinator` 在网络断开时缓冲消息，恢复后自动刷新
- **会话持久化** — 可选 JSON 序列化/加载对话历史
- **密钥解析** — API key 支持 CLI、环境变量、混淆文件三级来源
- **Mock 模式** — 无需网络和 API key 即可测试完整 agent 流程
- **严格 C++11** — 无异常；API 仅在 loop 线程调用

## 平台支持

| 要求 | 版本 |
|------|------|
| 操作系统 | 仅 Linux |
| cppev | >= 0.1.0 |
| C++ 标准 | C++11 |

> **安全提示：** 文件 API key 为 XOR 混淆，非加密。生产环境请参阅 [SECURITY.md](SECURITY.md)。

## cppev 依赖

EmbedAgent 依赖 [cppev](https://github.com/yjm6371/cppev)，有三种接入方式：

| 方式 | 说明 |
|------|------|
| 并列 checkout | `-DCPPEV_SOURCE_DIR=../cppev`（CMake preset 默认） |
| Git submodule | `git clone --recurse-submodules` → `external/cppev/` |
| 已安装包 | `cmake --install` cppev 后 `find_package(cppev)` |

详见 [docs/build.cn.md](docs/build.cn.md) 或 [docs/build.md](docs/build.md)（英文）。

## 快速开始

### 构建

将 cppev 与 embedagent 并列放置（或通过 `CPPEV_SOURCE_DIR` 指定路径）：

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

### Mock 模式（无需 API key）

完整 Hello World 示例见 [docs/usage.cn.md](docs/usage.cn.md) §2。Mock 模式
只模拟 LLM 响应，本地工具 handler 仍真实执行。

### 真实 LLM API

构建 examples 后：

```bash
./build/host-release/examples/chat_agent \
    --url=https://api.deepseek.com \
    --api-key=sk-xxxx \
    --model=deepseek-chat \
    --prompt="你好"
```

## 架构

```
EvApp (cppev)
    └── EaQueueCoordinator（可选离线缓冲）
            └── EaAgent
                    ├── EaLlmClient   (HTTP / SSE)
                    ├── EaSession     (对话历史 + trim)
                    └── EaToolRegistry (本地工具)
```

## 文档

| 文档 | 说明 |
|------|------|
| [使用说明（中文）](docs/usage.cn.md) | API、工具、离线队列、密钥管理 |
| [Usage (EN)](docs/usage.en.md) | English usage guide |
| [设计说明书（中文）](docs/design.cn.md) | 架构与设计决策 |
| [Design (EN)](docs/design.en.md) | English design document |
| [Build guide (EN)](docs/build.md) | 英文构建指南 |
| [构建指南（中文）](docs/build.cn.md) | 联合编译、submodule、安装 cppev |
| [贡献指南](CONTRIBUTING.md) | 构建、测试、PR 规范 |
| [安全策略](SECURITY.md) | API 密钥、工具、网络信任 |
| [路线图](ROADMAP.md) | 计划功能与非目标 |
| [发布后建议](docs/recommendations.cn.md) | 维护者与生产环境清单 |

## 示例

| 程序 | 说明 |
|------|------|
| `chat_agent` | 完整聊天 Agent（工具 + 会话持久化） |
| `tool_call_demo` | 嵌入式 Linux 系统工具（GPIO、温度、网络等） |
| `offline_queue_demo` | 离线消息队列与重连刷新 |

## 许可证

MIT — 见 [LICENSE](LICENSE)。
