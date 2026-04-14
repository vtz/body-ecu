cmake_minimum_required(VERSION 3.14)
project(body_ecu_posix CXX)

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

# Domain modules
add_subdirectory(${LIBS_ROOT}/body/lighting ${CMAKE_CURRENT_BINARY_DIR}/lighting)
add_subdirectory(${LIBS_ROOT}/body/door-lock ${CMAKE_CURRENT_BINARY_DIR}/door-lock)
add_subdirectory(${LIBS_ROOT}/body/vehicle-mode ${CMAKE_CURRENT_BINARY_DIR}/vehicle-mode)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/can-gateway ${CMAKE_CURRENT_BINARY_DIR}/can-gateway)
add_subdirectory(${LIBS_ROOT}/platform/diagnostics ${CMAKE_CURRENT_BINARY_DIR}/diagnostics)

# System adapter layer (lifecycle wrappers, SOME/IP, DoIP/DoCAN)
add_library(adapters_system STATIC
    ${LIBS_ROOT}/adapters/system/src/SomeIpSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/LightingSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/DoorLockSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/VehicleModeSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/CanGatewaySystem.cpp
    ${LIBS_ROOT}/adapters/system/src/DiagnosticsSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/DoIpTransport.cpp
    ${LIBS_ROOT}/adapters/system/src/DoCanTransport.cpp
)
target_include_directories(adapters_system PUBLIC
    ${LIBS_ROOT}/adapters/system/include
)
target_link_libraries(adapters_system PUBLIC
    ports lifecycle lighting door_lock vehicle_mode can_gateway diagnostics
    someip-transport
)
target_compile_definitions(adapters_system PUBLIC HAS_OPENSOMEIP)

# Linux platform adapters
add_subdirectory(${LIBS_ROOT}/adapters/linux ${CMAKE_CURRENT_BINARY_DIR}/adapters-linux)

# POSIX executable
add_executable(body_ecu_posix main.cpp)
target_link_libraries(body_ecu_posix PRIVATE
    adapters_system
    adapters_linux
    lifecycle
)
