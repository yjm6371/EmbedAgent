# Toolchain file for ARM Cortex-A (64-bit, AArch64).
# Tested with aarch64-linux-gnu-g++ from:
#   Ubuntu: apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#   Linaro: https://developer.arm.com/downloads/-/gnu-a
#
# Common targets: Raspberry Pi 3/4/5 (64-bit), RK3566, RK3588, i.MX8, AM62x.

set(CMAKE_SYSTEM_NAME    Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Override with -DCROSS_COMPILE=<prefix>- to use a different toolchain.
set(CROSS_COMPILE "aarch64-linux-gnu-" CACHE STRING
    "Cross-compiler prefix (e.g. aarch64-linux-gnu-)")

set(CMAKE_C_COMPILER    ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER  ${CROSS_COMPILE}g++)
set(CMAKE_AR            ${CROSS_COMPILE}ar     CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB        ${CROSS_COMPILE}ranlib  CACHE FILEPATH "Ranlib")
set(CMAKE_STRIP         ${CROSS_COMPILE}strip   CACHE FILEPATH "Strip")

# ---------------------------------------------------------------------------
# Sysroot (optional, recommended for Buildroot / Yocto targets)
# ---------------------------------------------------------------------------
if(CMAKE_SYSROOT)
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---------------------------------------------------------------------------
# Third-party library path guide
# ---------------------------------------------------------------------------
# Method A — sysroot (Buildroot / Yocto, recommended):
#   cmake ... -DCMAKE_SYSROOT=/path/to/buildroot/staging
#
# Method B — unified prefix:
#   cmake ... -DCMAKE_PREFIX_PATH=/opt/aarch64-sysroot
#
# Method C — per-library overrides:
#   -DOPENSSL_ROOT_DIR=/opt/aarch64-openssl
#   -DCURL_ROOT=/opt/aarch64-curl
#   -DMOSQUITTO_ROOT=/opt/aarch64-mosquitto
