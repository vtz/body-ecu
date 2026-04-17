cmake_minimum_required(VERSION 3.14)
project(body_ecu_autosd CXX)

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

# AutoSD adapters (Kuksa, NATS, SomeIpKuksaBridge)
add_subdirectory(${LIBS_ROOT}/adapters/autosd ${CMAKE_CURRENT_BINARY_DIR}/adapters-autosd)

# AutoSD executable
add_executable(body_ecu_autosd main.cpp)
target_link_libraries(body_ecu_autosd PRIVATE
    cloud_gateway_client
    adapters_autosd
    someip_system
    lifecycle
)
