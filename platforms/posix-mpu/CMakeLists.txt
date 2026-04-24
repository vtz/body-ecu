cmake_minimum_required(VERSION 3.14)
project(body_ecu_posix_mpu CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

# OpenSOME/IP stack (real UDP transport)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/opensomeip.cmake)

# Port interfaces (header-only)
add_subdirectory(${LIBS_ROOT}/platform/ports ${CMAKE_CURRENT_BINARY_DIR}/ports)

# Lifecycle manager
add_subdirectory(${LIBS_ROOT}/platform/lifecycle ${CMAKE_CURRENT_BINARY_DIR}/lifecycle)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/cloud-gateway ${CMAKE_CURRENT_BINARY_DIR}/cloud-gateway)

# SomeIpSystem (SOME/IP client for bridge)
add_library(someip_system STATIC
    ${LIBS_ROOT}/adapters/system/src/SomeIpSystem.cpp
)
target_include_directories(someip_system PUBLIC
    ${LIBS_ROOT}/adapters/system/include
)
target_link_libraries(someip_system PUBLIC ports lifecycle someip-transport)
target_compile_definitions(someip_system PUBLIC HAS_OPENSOMEIP)

# AutoSD adapters (SomeIpKuksaBridge)
add_subdirectory(${LIBS_ROOT}/adapters/autosd ${CMAKE_CURRENT_BINARY_DIR}/adapters-autosd)

# Linux adapters (InProcessSignalBus, StubCloudTransport)
add_subdirectory(${LIBS_ROOT}/adapters/linux ${CMAKE_CURRENT_BINARY_DIR}/adapters-linux)

# Linenoise (terminal line editing)
add_library(linenoise STATIC third_party/linenoise.c)
target_include_directories(linenoise PUBLIC third_party)

# POSIX MPU executable (bridge + cloud gateway client + CLI)
add_executable(body_ecu_posix_mpu main.cpp cli.cpp)
target_link_libraries(body_ecu_posix_mpu PRIVATE
    cloud_gateway_client
    adapters_autosd
    adapters_linux
    someip_system
    lifecycle
    linenoise
)
