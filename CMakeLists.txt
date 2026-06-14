cmake_minimum_required(VERSION 3.14)
project(embedagent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Symlink compile_commands.json to the project root so clangd and editors find
# it without knowing the build directory name.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
    add_definitions(-DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
    option(EA_ENABLE_SANITIZERS
        "Enable AddressSanitizer + UBSan (host Debug builds only)" ON)
    if(EA_ENABLE_SANITIZERS AND NOT CMAKE_CROSSCOMPILING)
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(CPPEV_PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_definitions(CPPEV_PLATFORM_MACOS)
elseif(WIN32)
    add_compile_definitions(CPPEV_PLATFORM_WINDOWS)
endif()

# -- cppev dependency ----------------------------------------------------------
# Priority:
#   1. -DCPPEV_SOURCE_DIR=<path>   explicit source tree (side-by-side checkout)
#   2. external/cppev/             git submodule
#   3. find_package(cppev)         installed package
if(DEFINED CPPEV_SOURCE_DIR)
    message(STATUS "embedagent: cppev source at ${CPPEV_SOURCE_DIR}")
    set(CPPEV_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
    set(CPPEV_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
    set(CPPEV_BUILD_EMBEDAGENT OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CPPEV_SOURCE_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR}/cppev)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/cppev/CMakeLists.txt)
    message(STATUS "embedagent: cppev via git submodule")
    set(CPPEV_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
    set(CPPEV_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
    set(CPPEV_BUILD_EMBEDAGENT OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/cppev)
else()
    find_package(cppev REQUIRED)
endif()

# -- Build options -------------------------------------------------------------
option(EA_BUILD_TESTS    "Build unit tests"   ON)
option(EA_BUILD_EXAMPLES "Build CLI examples" ON)

if(CMAKE_CROSSCOMPILING)
    set(EA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()

if(EA_BUILD_TESTS)
    enable_testing()
endif()

# -- Library -------------------------------------------------------------------
set(EA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(embedagent
    src/ea_session.cpp
    src/ea_prompt_template.cpp
    src/ea_tool_param.cpp
    src/ea_tool_args.cpp
    src/ea_tool_registry.cpp
    src/ea_llm_client.cpp
    src/ea_agent.cpp
    src/ea_offline_queue.cpp
    src/ea_memory_offline_queue.cpp
    src/ea_file_storage.cpp
    src/ea_file_offline_queue.cpp
    src/ea_connectivity_monitor.cpp
    src/ea_network_error.cpp
    src/ea_queue_coordinator.cpp
    src/ea_secret_obfuscator.cpp
    src/ea_env_secret_store.cpp
    src/ea_file_secret_store.cpp
    src/ea_secret_resolver.cpp
    src/ea_session_codec.cpp
    src/ea_runtime_config.cpp)

target_include_directories(embedagent
    PUBLIC
        $<BUILD_INTERFACE:${EA_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>)

# Installed cppev exports namespaced targets; subdir build uses plain names.
if(TARGET cppev::cppev_framework)
    set(EA_CPPEV_FRAMEWORK cppev::cppev_framework)
    set(EA_CPPEV_EXT_CURL  cppev::cppev_ext_curl)
    set(EA_CPPEV_EXT_JSON  cppev::cppev_ext_json)
else()
    set(EA_CPPEV_FRAMEWORK cppev_framework)
    set(EA_CPPEV_EXT_CURL  cppev_ext_curl)
    set(EA_CPPEV_EXT_JSON  cppev_ext_json)
endif()

target_link_libraries(embedagent
    PUBLIC
        ${EA_CPPEV_FRAMEWORK}
        ${EA_CPPEV_EXT_CURL}
        ${EA_CPPEV_EXT_JSON})

target_compile_features(embedagent PUBLIC cxx_std_11)

# -- Tests ---------------------------------------------------------------------
if(EA_BUILD_TESTS)
    find_package(GTest REQUIRED)

    set(EA_TEST_TARGETS
        test_ea_session
        test_ea_tool_registry
        test_ea_agent_mock
        test_ea_llm_url
        test_ea_offline_queue
        test_ea_queue_coordinator
        test_ea_secret_store
        test_ea_session_codec
        test_ea_file_storage)
    foreach(target ${EA_TEST_TARGETS})
        add_executable(${target} tests/${target}.cpp)
        target_link_libraries(${target} PRIVATE embedagent GTest::gtest_main)
        target_compile_features(${target} PRIVATE cxx_std_11)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
        add_test(NAME ${target} COMMAND ${CMAKE_BINARY_DIR}/tests/${target})
    endforeach()
endif()

# -- Examples ------------------------------------------------------------------
if(EA_BUILD_EXAMPLES)
    if(TARGET ${EA_CPPEV_EXT_CURL} AND TARGET ${EA_CPPEV_EXT_JSON})
        add_library(ea_examples_common STATIC
            examples/common/ea_demo_io.cpp)
        target_include_directories(ea_examples_common PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/common)
        target_link_libraries(ea_examples_common PUBLIC embedagent)
        target_compile_features(ea_examples_common PUBLIC cxx_std_11)

        add_executable(chat_agent
            examples/chat_agent/chat_agent.cpp)
        target_link_libraries(chat_agent PRIVATE ea_examples_common)
        target_compile_features(chat_agent PRIVATE cxx_std_11)
        set_target_properties(chat_agent PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)

        add_executable(tool_call_demo
            examples/tool_call_demo/tool_call_demo.cpp
            examples/tool_call_demo/ea_tool_sysinfo.cpp
            examples/tool_call_demo/ea_tool_thermal.cpp
            examples/tool_call_demo/ea_tool_gpio.cpp
            examples/tool_call_demo/ea_tool_network.cpp
            examples/tool_call_demo/ea_tool_process.cpp)
        target_include_directories(tool_call_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/tool_call_demo)
        target_link_libraries(tool_call_demo PRIVATE ea_examples_common)
        target_compile_features(tool_call_demo PRIVATE cxx_std_11)
        set_target_properties(tool_call_demo PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)

        add_executable(offline_queue_demo
            examples/offline_queue_demo/offline_queue_demo.cpp)
        target_link_libraries(offline_queue_demo PRIVATE ea_examples_common)
        target_compile_features(offline_queue_demo PRIVATE cxx_std_11)
        set_target_properties(offline_queue_demo PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
    endif()
endif()

# -- Install rules --------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS embedagent
    EXPORT  embedagentTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/embedagentConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embedagentConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/embedagentConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/embedagent
)

install(EXPORT embedagentTargets
    FILE      embedagentTargets.cmake
    NAMESPACE embedagent::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/embedagent
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/embedagentConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/embedagentConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/embedagent
)
