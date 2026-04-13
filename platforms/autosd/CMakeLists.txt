cmake_minimum_required(VERSION 3.14)
project(body_ecu_autosd CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

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
target_link_libraries(someip_system PUBLIC ports)

# AutoSD adapters (Kuksa, NATS, SomeIpKuksaBridge)
add_subdirectory(${LIBS_ROOT}/adapters/autosd ${CMAKE_CURRENT_BINARY_DIR}/adapters-autosd)

# AutoSD executable
add_executable(body_ecu_autosd main.cpp)
target_link_libraries(body_ecu_autosd PRIVATE
    cloud_gateway_client
    adapters_autosd
    someip_system
)
