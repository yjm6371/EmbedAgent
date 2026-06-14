# EmbedAgent Build Guide

EmbedAgent depends on **cppev** for its event loop, HTTP client, JSON, and SSE
parsing. This guide covers two build scenarios:

- **Scenario A — Source co-build:** cppev and embedagent are compiled together
  in a single CMake invocation. Changes to cppev are immediately reflected when
  rebuilding embedagent.
- **Scenario B — Pre-built cppev:** cppev has already been compiled and
  installed. embedagent locates it via `find_package` and links against the
  pre-built libraries. This is typical for CI pipelines and cross-compilation
  workflows where cppev is built once and cached.

---

## Prerequisites

### Host build

| Package | Minimum version | Ubuntu / Debian |
|---|---|---|
| CMake | 3.14 (3.21 for presets) | `apt install cmake` |
| Ninja | any | `apt install ninja-build` |
| GCC (or Clang) | GCC 7 / Clang 6 | `apt install build-essential` |
| libcurl | 7.x | `apt install libcurl4-openssl-dev` |
| libssl | 1.1 or 3.x | `apt install libssl-dev` |
| libmosquitto | 2.x (optional, for MQTT) | `apt install libmosquitto-dev` |
| GTest | any (tests only) | `apt install libgtest-dev` |

### Cross build (arm32 / aarch64)

```bash
# ARM 32-bit (Cortex-A, hard-float)
apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# AArch64
apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

Cross-compiled dependencies (libcurl, libssl) must be available in the target
sysroot or a prefix directory.  See [Cross-Compilation](#4-cross-compilation)
for details.

---

## Scenario A — Source Co-Build

Both cppev and embedagent are configured in a single `cmake` command. cppev is
pulled in as a CMake sub-directory; the embedagent `CMakeLists.txt` handles
everything automatically.

### Option A-1: Side-by-side checkouts with `CPPEV_SOURCE_DIR`

Place the two repositories next to each other (or at any path you choose) and
pass the cppev source directory on the command line.

```
workspace/
├── cppev/            ← cppev source tree
└── embedagent/       ← embedagent source tree
```

#### Using CMake presets (CMake 3.21+, recommended)

The bundled presets already set `CPPEV_SOURCE_DIR` to `${sourceDir}/../cppev`,
so no extra flags are needed when the two repositories sit side by side.

```bash
cd embedagent

# Host — Debug build with AddressSanitizer + UBSan + unit tests
cmake --preset host-debug
cmake --build --preset host-debug

# Host — optimised Release build
cmake --preset host-release
cmake --build --preset host-release
```

Build output:

```
build/host-debug/
├── libembedagent.a
├── examples/
│   ├── chat_agent
│   ├── tool_call_demo
│   └── offline_queue_demo
└── tests/
    ├── test_ea_agent_mock
    └── ...
```

#### Manual configuration (CMake 3.14+)

```bash
cd embedagent
cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCPPEV_SOURCE_DIR=/path/to/cppev \
      -DEA_BUILD_TESTS=ON
cmake --build build -j$(nproc)
```

If `CPPEV_SOURCE_DIR` differs from the default `../cppev`, always pass it
explicitly.  Omitting it causes CMake to fall back to the git submodule or
`find_package`, which may not find cppev.

#### Overriding the `CPPEV_SOURCE_DIR` preset default

If your cppev checkout is not at `../cppev`, override the variable without
modifying `CMakePresets.json`:

```bash
cmake --preset host-release -DCPPEV_SOURCE_DIR=/home/user/projects/cppev
cmake --build --preset host-release
```

---

### Option A-2: Git submodule

When embedagent ships cppev as a git submodule under `external/cppev/`, no
extra flag is needed.

```bash
# First-time clone
git clone https://github.com/YOUR_ORG/embedagent
cd embedagent
git submodule update --init --recursive

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

CMake automatically detects `external/cppev/CMakeLists.txt` and uses it
without any additional configuration.

---

## Scenario B — Pre-Built cppev

Use this approach when cppev is already compiled and installed — for example,
in a CI cache, a package feed, a Buildroot sysroot, or a shared development
prefix on a build server.

### Step 1: Build and install cppev

Configure cppev with a chosen install prefix, then install it.

```bash
cd /path/to/cppev

# Host install (e.g. into /opt/cppev)
cmake -B build/release \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/cppev \
      -DCPPEV_BUILD_TESTS=OFF \
      -DCPPEV_BUILD_EXAMPLES=OFF
cmake --build build/release -j$(nproc)
cmake --install build/release
```

The install tree looks like:

```
/opt/cppev/
├── include/
│   ├── ev_event_loop.h
│   ├── ev_http_client.h
│   ├── ev_llm_json.h
│   └── ...
├── lib/
│   ├── libcppev_event.a
│   ├── libcppev_framework.a
│   ├── libcppev_ext_curl.a
│   ├── libcppev_ext_json.a
│   └── cmake/cppev/
│       ├── cppevConfig.cmake
│       ├── cppevConfigVersion.cmake
│       └── cppevTargets.cmake
└── ...
```

> **Tip:** Use `cmake --preset embedded-minimal` or `cmake --preset
> aarch64-minimal` to install a stripped-down cppev without MQTT or TLS.

### Step 2: Build embedagent against the installed cppev

Point CMake at the cppev install prefix via `CMAKE_PREFIX_PATH`.  embedagent's
`CMakeLists.txt` calls `find_package(cppev REQUIRED)` when neither
`CPPEV_SOURCE_DIR` nor the submodule is present.

```bash
cd /path/to/embedagent

cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/opt/cppev \
      -DEA_BUILD_TESTS=OFF \
      -DEA_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
```

Alternatively, set `CMAKE_PREFIX_PATH` in the environment:

```bash
export CMAKE_PREFIX_PATH=/opt/cppev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Or pass the cmake package config directory directly:

```bash
cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -Dcppev_DIR=/opt/cppev/lib/cmake/cppev
cmake --build build -j$(nproc)
```

### Verifying the right cppev is found

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/opt/cppev 2>&1 | grep -i cppev
# Expected output:
# -- Found cppev: /opt/cppev/lib/cmake/cppev/cppevConfig.cmake (found version "0.1.0")
```

### Step 3: Install embedagent and consume from a downstream project

After building embedagent against the installed cppev, install both libraries
into a shared prefix (or separate prefixes on `CMAKE_PREFIX_PATH`):

```bash
cd /path/to/embedagent
cmake --install build --prefix /opt/embedagent
```

A downstream CMake project can then link embedagent without source checkouts:

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_agent_app LANGUAGES CXX)

find_package(embedagent 0.1.0 REQUIRED)

add_executable(my_agent_app main.cpp)
target_link_libraries(my_agent_app PRIVATE embedagent::embedagent)
```

Configure with:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/opt/cppev;/opt/embedagent"
cmake --build build
```

`find_package(embedagent)` pulls in `find_dependency(cppev)` automatically.

---

## Build Options Reference

| Option | Default | Description |
|---|---|---|
| `CPPEV_SOURCE_DIR` | *(unset)* | Path to the cppev source tree (Scenario A-1) |
| `EA_BUILD_TESTS` | `ON` | Build GTest unit tests (auto-disabled when cross-compiling) |
| `EA_BUILD_EXAMPLES` | `ON` | Build CLI example programs |
| `EA_ENABLE_SANITIZERS` | `ON` (Debug only) | AddressSanitizer + UBSan on host debug builds |
| `CMAKE_BUILD_TYPE` | `Release` | `Debug` / `Release` |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Install destination for `cmake --install` |

---

## Running Tests

```bash
# After a host-debug or host-release build with EA_BUILD_TESTS=ON
cd build         # or build/host-debug, build/host-release
ctest --output-on-failure

# Run a specific test
./tests/test_ea_agent_mock
./tests/test_ea_tool_registry
```

---

## 4. Cross-Compilation

embedagent ships two ready-to-use toolchain files:

| Preset | Toolchain | Target |
|---|---|---|
| `arm32` | `cmake/toolchains/arm-linux-gnueabihf.cmake` | ARM Cortex-A 32-bit (RPi 2, i.MX6, …) |
| `aarch64` | `cmake/toolchains/aarch64-linux-gnu.cmake` | AArch64 (RPi 3/4/5 64-bit, RK3588, …) |

### Scenario A — Cross-compile cppev + embedagent together

```bash
cd embedagent

# AArch64 cross-build (Scenario A-1, cppev at ../cppev)
cmake --preset aarch64
cmake --build --preset aarch64

# ARM 32-bit with a custom cppev path
cmake --preset arm32 -DCPPEV_SOURCE_DIR=/path/to/cppev
cmake --build --preset arm32
```

Tests are automatically disabled when `CMAKE_CROSSCOMPILING` is true.

### Scenario B — Cross-compile against a pre-built cppev

First cross-compile and install cppev for the target:

```bash
cd /path/to/cppev
cmake --preset aarch64-minimal \
      -DCMAKE_INSTALL_PREFIX=/opt/cppev-aarch64
cmake --build --preset aarch64-minimal
cmake --install build/aarch64-minimal
```

Then build embedagent:

```bash
cd embedagent
cmake -B build/aarch64 \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCMAKE_PREFIX_PATH=/opt/cppev-aarch64 \
      -DEA_BUILD_TESTS=OFF \
      -DEA_BUILD_EXAMPLES=OFF
cmake --build build/aarch64 -j$(nproc)
```

### Providing cross-compiled dependencies (libcurl, libssl)

The toolchain files support three methods for supplying third-party libraries.
Pick whichever fits your build environment:

**Method A — Buildroot / Yocto sysroot (recommended)**

```bash
cmake -B build/aarch64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCMAKE_SYSROOT=/path/to/buildroot/output/staging \
      -DCMAKE_PREFIX_PATH=/opt/cppev-aarch64 \
      -DEA_BUILD_TESTS=OFF
```

**Method B — Unified prefix directory**

```bash
cmake -B build/aarch64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCMAKE_PREFIX_PATH="/opt/cppev-aarch64;/opt/aarch64-sysroot" \
      -DEA_BUILD_TESTS=OFF
```

**Method C — Per-library overrides**

```bash
cmake -B build/aarch64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCMAKE_PREFIX_PATH=/opt/cppev-aarch64 \
      -DOPENSSL_ROOT_DIR=/opt/aarch64-openssl \
      -DCURL_ROOT=/opt/aarch64-curl \
      -DEA_BUILD_TESTS=OFF
```

### Custom toolchain prefix

If your cross-compiler is not named `aarch64-linux-gnu-gcc`, override the
prefix without editing the toolchain file:

```bash
cmake -B build/aarch64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCROSS_COMPILE=my-custom-aarch64-
```

---

## Combining Scenarios in a CI Pipeline

A typical pipeline that caches cppev for all embedagent builds:

```yaml
# (Pseudo-YAML, adapt to your CI provider)
stages:
  - build-cppev
  - build-embedagent

build-cppev:
  cache:
    key: "cppev-aarch64-${CPPEV_GIT_SHA}"
    paths: [/opt/cppev-aarch64]
  script:
    - cmake -B cppev/build
        -DCMAKE_TOOLCHAIN_FILE=embedagent/cmake/toolchains/aarch64-linux-gnu.cmake
        -DCMAKE_INSTALL_PREFIX=/opt/cppev-aarch64
        -DCPPEV_BUILD_TESTS=OFF -DCPPEV_BUILD_EXAMPLES=OFF
        /path/to/cppev
    - cmake --build cppev/build -j4
    - cmake --install cppev/build

build-embedagent:
  needs: [build-cppev]
  script:
    - cmake -B embedagent/build
        -DCMAKE_TOOLCHAIN_FILE=embedagent/cmake/toolchains/aarch64-linux-gnu.cmake
        -DCMAKE_PREFIX_PATH=/opt/cppev-aarch64
        -DEA_BUILD_TESTS=OFF -DEA_BUILD_EXAMPLES=OFF
        /path/to/embedagent
    - cmake --build embedagent/build -j4
```

---

## Troubleshooting

**`CMake Error: By not providing 'Findcppev.cmake'…`**

CMake fell through to `find_package(cppev REQUIRED)` but could not locate the
package. Fix by providing one of:

```bash
# Scenario A-1: pass the cppev source tree
-DCPPEV_SOURCE_DIR=/path/to/cppev

# Scenario B: pass the install prefix
-DCMAKE_PREFIX_PATH=/opt/cppev
```

---

**`CMake Error: could not find Ninja`**

Install Ninja or use a different generator:

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
```

---

**`/usr/bin/arm-linux-gnueabihf-g++: No such file or directory`**

Install the cross-compiler:

```bash
apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

---

**`Could not find a package configuration file provided by "cppev"`** when
using `CMAKE_PREFIX_PATH`

Verify the install actually created the cmake config:

```bash
ls /opt/cppev/lib/cmake/cppev/
# Should show: cppevConfig.cmake  cppevConfigVersion.cmake  cppevTargets.cmake
```

If the directory is missing, cppev was not installed correctly — re-run
`cmake --install`.

---

**`target_link_libraries: IMPORTED_LOCATION not set`** when mixing Debug cppev
with Release embedagent

Build both components with the same `CMAKE_BUILD_TYPE`, or install a Release
cppev separately from a Debug cppev and use the matching prefix.
