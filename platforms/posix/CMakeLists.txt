cmake_minimum_required(VERSION 3.14)
project(body_ecu_posix CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

# Port interfaces (header-only)
add_subdirectory(${LIBS_ROOT}/platform/ports ${CMAKE_CURRENT_BINARY_DIR}/ports)

# Domain modules
add_subdirectory(${LIBS_ROOT}/body/lighting ${CMAKE_CURRENT_BINARY_DIR}/lighting)
add_subdirectory(${LIBS_ROOT}/body/door-lock ${CMAKE_CURRENT_BINARY_DIR}/door-lock)
add_subdirectory(${LIBS_ROOT}/body/vehicle-mode ${CMAKE_CURRENT_BINARY_DIR}/vehicle-mode)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/can-gateway ${CMAKE_CURRENT_BINARY_DIR}/can-gateway)
add_subdirectory(${LIBS_ROOT}/platform/diagnostics ${CMAKE_CURRENT_BINARY_DIR}/diagnostics)

# OpenBSW adapter layer (SomeIpSystem, lifecycle wrappers, DoIP/DoCAN)
add_library(adapters_openbsw STATIC
    ${LIBS_ROOT}/adapters/openbsw/src/SomeIpSystem.cpp
    ${LIBS_ROOT}/adapters/openbsw/src/LightingSystem.cpp
    ${LIBS_ROOT}/adapters/openbsw/src/DoorLockSystem.cpp
    ${LIBS_ROOT}/adapters/openbsw/src/VehicleModeSystem.cpp
    ${LIBS_ROOT}/adapters/openbsw/src/CanGatewaySystem.cpp
    ${LIBS_ROOT}/adapters/openbsw/src/DiagnosticsSystem.cpp
    ${LIBS_ROOT}/adapters/openbsw/src/DoIpTransport.cpp
    ${LIBS_ROOT}/adapters/openbsw/src/DoCanTransport.cpp
)
target_include_directories(adapters_openbsw PUBLIC
    ${LIBS_ROOT}/adapters/openbsw/include
)
target_link_libraries(adapters_openbsw PUBLIC
    ports lighting door_lock vehicle_mode can_gateway diagnostics
)

# Linux platform adapters
add_subdirectory(${LIBS_ROOT}/adapters/linux ${CMAKE_CURRENT_BINARY_DIR}/adapters-linux)

# POSIX executable
add_executable(body_ecu_posix main.cpp)
target_link_libraries(body_ecu_posix PRIVATE
    adapters_openbsw
    adapters_linux
)
