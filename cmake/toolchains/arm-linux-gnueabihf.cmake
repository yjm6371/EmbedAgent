# Toolchain file for ARM Cortex-A (32-bit, hard-float ABI).
# Tested with arm-linux-gnueabihf-g++ from:
#   Ubuntu: apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
#   Linaro: https://developer.arm.com/downloads/-/gnu-a

set(CMAKE_SYSTEM_NAME    Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Override with -DCROSS_COMPILE=<prefix>- to use a different toolchain.
set(CROSS_COMPILE "arm-linux-gnueabihf-" CACHE STRING
    "Cross-compiler prefix (e.g. arm-linux-gnueabihf-)")

set(CMAKE_C_COMPILER    ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER  ${CROSS_COMPILE}g++)
set(CMAKE_AR            ${CROSS_COMPILE}ar     CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB        ${CROSS_COMPILE}ranlib  CACHE FILEPATH "Ranlib")
set(CMAKE_STRIP         ${CROSS_COMPILE}strip   CACHE FILEPATH "Strip")

# ---------------------------------------------------------------------------
# Sysroot (optional, recommended for Buildroot / Yocto targets)
# ---------------------------------------------------------------------------
# Pass -DCMAKE_SYSROOT=/path/to/sysroot on the command line, or set it in
# your CMakePresets.json cacheVariable.  When set, all find_* commands search
# inside the sysroot instead of the host system.
if(CMAKE_SYSROOT)
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
endif()

# Never use host programs (e.g. cmake itself); only libraries/headers from
# the sysroot or from CMAKE_PREFIX_PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---------------------------------------------------------------------------
# Third-party library path guide
# ---------------------------------------------------------------------------
# The extensions directory uses three external libraries.  Choose whichever
# method fits your build environment:
#
# Method A — sysroot (Buildroot / Yocto, recommended):
#   All libraries are installed inside the sysroot; no extra flags needed.
#   cmake ... -DCMAKE_SYSROOT=/path/to/buildroot/staging
#
# Method B — unified prefix (independently cross-compiled libraries):
#   cmake ... -DCMAKE_PREFIX_PATH=/opt/arm-sysroot
#
# Method C — per-library overrides:
#   -DOPENSSL_ROOT_DIR=/opt/arm-openssl   (used by find_package(OpenSSL))
#   -DCURL_ROOT=/opt/arm-curl             (used by find_package(CURL))
#   -DMOSQUITTO_ROOT=/opt/arm-mosquitto   (used by extensions/CMakeLists.txt)
