cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
project(body_ecu_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_TESTS ON)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

enable_testing()
include(GoogleTest)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

# Port interfaces + mocks
add_subdirectory(${LIBS_ROOT}/platform/ports ports)

# Adapters: OpenBSW lifecycle wrappers + transports
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
target_link_libraries(adapters_openbsw PUBLIC ports lighting door_lock vehicle_mode can_gateway diagnostics)
add_subdirectory(${LIBS_ROOT}/adapters/openbsw/tests adapters_openbsw_tests)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/config-loader config-loader)
add_subdirectory(${LIBS_ROOT}/platform/can-gateway can-gateway)
add_subdirectory(${LIBS_ROOT}/platform/diagnostics diagnostics)
add_subdirectory(${LIBS_ROOT}/platform/cloud-gateway cloud-gateway)

# Body domain modules
add_subdirectory(${LIBS_ROOT}/body/lighting lighting)
add_subdirectory(${LIBS_ROOT}/body/door-lock door-lock)
add_subdirectory(${LIBS_ROOT}/body/vehicle-mode vehicle-mode)
