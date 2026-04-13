cmake_minimum_required(VERSION 3.14)
project(body_ecu_posix_mpu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

# OpenSOME/IP stack (real UDP transport)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/opensomeip.cmake)

# Port interfaces (header-only)
add_subdirectory(${LIBS_ROOT}/platform/ports ${CMAKE_CURRENT_BINARY_DIR}/ports)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/cloud-gateway ${CMAKE_CURRENT_BINARY_DIR}/cloud-gateway)

# OpenBSW SomeIpSystem (SOME/IP client for bridge)
add_library(someip_system STATIC
    ${LIBS_ROOT}/adapters/openbsw/src/SomeIpSystem.cpp
)
target_include_directories(someip_system PUBLIC
    ${LIBS_ROOT}/adapters/openbsw/include
)
target_link_libraries(someip_system PUBLIC ports someip-transport)
target_compile_definitions(someip_system PUBLIC HAS_OPENSOMEIP)

# AutoSD adapters (SomeIpKuksaBridge)
add_subdirectory(${LIBS_ROOT}/adapters/autosd ${CMAKE_CURRENT_BINARY_DIR}/adapters-autosd)

# Linux adapters (InProcessSignalBus, StubCloudTransport)
add_subdirectory(${LIBS_ROOT}/adapters/linux ${CMAKE_CURRENT_BINARY_DIR}/adapters-linux)

# POSIX MPU executable (bridge + cloud gateway client)
add_executable(body_ecu_posix_mpu main.cpp)
target_link_libraries(body_ecu_posix_mpu PRIVATE
    cloud_gateway_client
    adapters_autosd
    adapters_linux
    someip_system
)
