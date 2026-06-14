# EmbedAgent 构建指南

EmbedAgent 依赖 **cppev** 提供事件循环、HTTP 客户端、JSON 与 SSE 解析。本文说明两种构建场景：

- **场景 A — 源码联合编译：** 一次 CMake 同时编译 cppev 与 embedagent，改 cppev 后重建 embedagent 即可生效。
- **场景 B — 预编译 cppev：** cppev 已安装到 prefix，embedagent 通过 `find_package(cppev)` 链接。适用于 CI 缓存、Buildroot sysroot、交叉编译等。

英文版：[build.md](build.md)

---

## 前置依赖

### 主机构建

| 包 | 最低版本 | Ubuntu / Debian |
|---|---|---|
| CMake | 3.14（preset 需 3.21+） | `apt install cmake` |
| Ninja | 任意 | `apt install ninja-build` |
| GCC / Clang | GCC 7 / Clang 6 | `apt install build-essential` |
| libcurl | 7.x | `apt install libcurl4-openssl-dev` |
| libssl | 1.1 或 3.x | `apt install libssl-dev` |
| libmosquitto | 2.x（可选，cppev MQTT） | `apt install libmosquitto-dev` |
| GTest | 任意（仅测试） | `apt install libgtest-dev` |

### 交叉编译（arm32 / aarch64）

```bash
# ARM 32 位
apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# AArch64
apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

交叉编译的 libcurl、libssl 须在目标 sysroot 或 prefix 中可用。详见下文「交叉编译」一节。

---

## 场景 A — 源码联合编译

### 方式 A-1：并列目录 + `CPPEV_SOURCE_DIR`

```
workspace/
├── cppev/
└── embedagent/
```

#### 使用 CMake Preset（推荐，需 CMake 3.21+）

preset 默认 `CPPEV_SOURCE_DIR=${sourceDir}/../cppev`，并列放置时无需额外参数。

```bash
cd embedagent

# 主机 Debug + 测试
cmake --preset host-debug -DCPPEV_ENABLE_SANITIZERS=OFF -DEA_ENABLE_SANITIZERS=OFF
cmake --build --preset host-debug

# 主机 Release
cmake --preset host-release
cmake --build --preset host-release
```

产物目录示例：

```
build/host-debug/
├── libembedagent.a
├── examples/          # chat_agent, tool_call_demo, offline_queue_demo
└── tests/
```

#### 手动配置（CMake 3.14+）

```bash
cd embedagent
cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCPPEV_SOURCE_DIR=/path/to/cppev \
      -DEA_BUILD_TESTS=ON
cmake --build build -j$(nproc)
```

若 cppev 不在 `../cppev`，必须显式传入 `CPPEV_SOURCE_DIR`，否则 CMake 会尝试 submodule 或 `find_package`。

### 方式 A-2：Git Submodule

```bash
git clone --recurse-submodules https://github.com/YOUR_ORG/embedagent.git
cd embedagent

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

CMake 检测到 `external/cppev/CMakeLists.txt` 后自动作为子目录编译。

---

## 场景 B — 预安装 cppev

### 步骤 1：安装 cppev

```bash
cd /path/to/cppev
cmake -B build/release \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/cppev \
      -DCPPEV_BUILD_TESTS=OFF \
      -DCPPEV_BUILD_EXAMPLES=OFF
cmake --build build/release -j$(nproc)
cmake --install build/release
```

安装树示例：

```
/opt/cppev/
├── include/
├── lib/
│   ├── libcppev_event.a
│   ├── libcppev_framework.a
│   ├── libcppev_ext_curl.a
│   ├── libcppev_ext_json.a
│   └── cmake/cppev/
│       ├── cppevConfig.cmake
│       └── cppevTargets.cmake
```

> 体积敏感目标可用 `cmake --preset embedded-minimal` 或 `aarch64-minimal` 安装不含 MQTT/TLS 的 cppev。

### 步骤 2：针对已安装 cppev 构建 embedagent

```bash
cd /path/to/embedagent
cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/opt/cppev \
      -DEA_BUILD_TESTS=OFF \
      -DEA_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
```

验证找到的 cppev：

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/opt/cppev 2>&1 | grep -i cppev
# -- Found cppev: /opt/cppev/lib/cmake/cppev/cppevConfig.cmake (found version "0.1.0")
```

### 步骤 3：安装 embedagent 并在下游项目中使用

```bash
cmake --install build --prefix /opt/embedagent
```

下游 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_agent_app LANGUAGES CXX)

find_package(embedagent 0.1.0 REQUIRED)

add_executable(my_agent_app main.cpp)
target_link_libraries(my_agent_app PRIVATE embedagent::embedagent)
```

配置：

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/opt/cppev;/opt/embedagent"
cmake --build build
```

`find_package(embedagent)` 会通过 `find_dependency(cppev)` 自动拉取 cppev。

---

## 构建选项

| 选项 | 默认 | 说明 |
|---|---|---|
| `CPPEV_SOURCE_DIR` | 未设置 | cppev 源码路径（场景 A-1） |
| `EA_BUILD_TESTS` | `ON` | 单元测试（交叉编译时自动 OFF） |
| `EA_BUILD_EXAMPLES` | `ON` | 示例程序 |
| `EA_ENABLE_SANITIZERS` | Debug 下 `ON` | ASan + UBSan（仅主机 Debug） |
| `CPPEV_ENABLE_SANITIZERS` | Debug 下 `ON` | 联编 cppev 时需与上一项一致 |
| `CMAKE_BUILD_TYPE` | `Release` | `Debug` / `Release` |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | `cmake --install` 目标 |

---

## 运行测试

```bash
cd build/host-debug   # 或 build/
ctest --output-on-failure

./tests/test_ea_agent_mock
./tests/test_ea_tool_registry
```

---

## 交叉编译

| Preset | 工具链 | 目标 |
|---|---|---|
| `arm32` | `cmake/toolchains/arm-linux-gnueabihf.cmake` | ARM 32 位 |
| `aarch64` | `cmake/toolchains/aarch64-linux-gnu.cmake` | AArch64 |

### 场景 A — 联合交叉编译

```bash
cd embedagent
cmake --preset aarch64
cmake --build --preset aarch64
```

### 场景 B — 针对已交叉安装的 cppev

```bash
# 先交叉安装 cppev
cd cppev
cmake --preset aarch64-minimal -DCMAKE_INSTALL_PREFIX=/opt/cppev-aarch64
cmake --build --preset aarch64-minimal
cmake --install build/aarch64-minimal

# 再构建 embedagent
cd embedagent
cmake -B build/aarch64 \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCMAKE_PREFIX_PATH=/opt/cppev-aarch64 \
      -DEA_BUILD_TESTS=OFF \
      -DEA_BUILD_EXAMPLES=OFF
cmake --build build/aarch64 -j$(nproc)
```

### 提供交叉编译依赖（libcurl / libssl）

**方法 A — Buildroot / Yocto sysroot（推荐）**

```bash
cmake -B build/aarch64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCMAKE_SYSROOT=/path/to/staging \
      -DCMAKE_PREFIX_PATH=/opt/cppev-aarch64 \
      -DEA_BUILD_TESTS=OFF
```

**方法 B — 统一 prefix**

```bash
-DCMAKE_PREFIX_PATH="/opt/cppev-aarch64;/opt/aarch64-sysroot"
```

**方法 C — 按库指定路径**

```bash
-DOPENSSL_ROOT_DIR=/opt/aarch64-openssl \
-DCURL_ROOT=/opt/aarch64-curl
```

---

## 故障排查

**找不到 cppev**

```bash
-DCPPEV_SOURCE_DIR=/path/to/cppev          # 场景 A
-DCMAKE_PREFIX_PATH=/opt/cppev              # 场景 B
```

**找不到 Ninja**

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ...
```

**联编时 sanitizer 链接错误**

同时关闭 cppev 与 embedagent 的 sanitizer：

```bash
-DCPPEV_ENABLE_SANITIZERS=OFF -DEA_ENABLE_SANITIZERS=OFF
```

**Debug cppev 与 Release embedagent 混用**

两者使用相同 `CMAKE_BUILD_TYPE`，或分别安装 Debug/Release prefix 并匹配使用。

---

## 相关文档

- [使用说明（中文）](usage.cn.md)
- [设计说明书（中文）](design.cn.md)
- [发布后建议](recommendations.cn.md)
- [cppev 交叉编译](https://github.com/YOUR_ORG/cppev/blob/main/docs/cross-compilation.md)
