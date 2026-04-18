cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
project(body_ecu_tests CXX C)

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
set(OPENBSW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../openbsw CACHE PATH "OpenBSW root")

# --- OpenBSW lifecycle (directly, without full Filelists.cmake) ---
# Minimal async mock for unit tests
add_library(asyncImpl INTERFACE)
target_include_directories(asyncImpl INTERFACE
    ${OPENBSW_DIR}/libs/bsw/asyncImpl/include)

add_library(platform INTERFACE)
target_include_directories(platform INTERFACE
    ${OPENBSW_DIR}/libs/bsw/platform/include)

add_library(timer INTERFACE)
target_include_directories(timer INTERFACE
    ${OPENBSW_DIR}/libs/bsw/timer/include)

add_library(common INTERFACE)
target_include_directories(common INTERFACE
    ${OPENBSW_DIR}/libs/bsw/common/include)

# ETL
add_library(etl INTERFACE)
target_include_directories(etl INTERFACE
    ${OPENBSW_DIR}/libs/3rdparty/etl/include)

# Util (needed by lifecycle logger)
add_library(util STATIC
    ${OPENBSW_DIR}/libs/bsw/util/src/util/logger/Logger.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/logger/ComponentInfo.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/logger/LevelInfo.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/format/PrintfFormatter.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/format/PrintfFormatScanner.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/format/PrintfArgumentReader.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/format/StringWriter.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/format/SharedStringWriter.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/format/AttributedString.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/format/Vt100AttributedStringFormatter.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/stream/NullOutputStream.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/stream/SharedOutputStream.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/stream/TaggedOutputStream.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/string/ConstString.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/command/CommandContext.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/command/GroupCommand.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/command/SimpleCommand.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/command/HelpCommand.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/command/ParentCommand.cpp
    ${OPENBSW_DIR}/libs/bsw/util/src/util/estd/assert.cpp
)
target_include_directories(util PUBLIC
    ${OPENBSW_DIR}/libs/bsw/util/include)
target_link_libraries(util PUBLIC platform etl)

# BSP interrupts (header-only for tests, with mock)
add_library(bspInterrupts INTERFACE)
target_include_directories(bspInterrupts INTERFACE
    ${OPENBSW_DIR}/libs/bsp/bspInterrupts/include
    ${OPENBSW_DIR}/libs/bsp/bspInterrupts/mock/include)

# Mock async for tests (no real scheduler)
add_library(async INTERFACE)
target_include_directories(async INTERFACE
    ${OPENBSW_DIR}/libs/bsw/asyncFreeRtos/test/include
    ${OPENBSW_DIR}/libs/bsw/asyncFreeRtos/include
    ${OPENBSW_DIR}/libs/bsw/async/include)
target_link_libraries(async INTERFACE asyncImpl timer bspInterrupts)

# OpenBSW lifecycle
add_library(lifecycle STATIC
    ${OPENBSW_DIR}/libs/bsw/lifecycle/src/lifecycle/AsyncLifecycleComponent.cpp
    ${OPENBSW_DIR}/libs/bsw/lifecycle/src/lifecycle/LifecycleComponent.cpp
    ${OPENBSW_DIR}/libs/bsw/lifecycle/src/lifecycle/LifecycleLogger.cpp
    ${OPENBSW_DIR}/libs/bsw/lifecycle/src/lifecycle/LifecycleManager.cpp
    ${OPENBSW_DIR}/libs/bsw/lifecycle/src/lifecycle/LifecycleManagerForwarder.cpp
    ${OPENBSW_DIR}/libs/bsw/lifecycle/src/lifecycle/SimpleLifecycleComponent.cpp
    ${OPENBSW_DIR}/libs/bsw/lifecycle/src/lifecycle/SingleContextLifecycleComponent.cpp
)
target_include_directories(lifecycle PUBLIC
    ${OPENBSW_DIR}/libs/bsw/lifecycle/include)
target_link_libraries(lifecycle PUBLIC async util etl)

# Port interfaces + mocks
add_subdirectory(${LIBS_ROOT}/platform/ports ports)

# Adapters: system lifecycle wrappers + transports
add_library(adapters_system STATIC
    ${LIBS_ROOT}/adapters/system/src/SomeIpSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/LightingSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/DoorLockSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/VehicleModeSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/CanGatewaySystem.cpp
    ${LIBS_ROOT}/adapters/system/src/DiagnosticsSystem.cpp
    ${LIBS_ROOT}/adapters/system/src/DoIpTransport.cpp
    ${LIBS_ROOT}/adapters/system/src/DoCanTransport.cpp
    ${LIBS_ROOT}/adapters/system/src/SpeedSimulatorSystem.cpp
)
target_include_directories(adapters_system PUBLIC
    ${LIBS_ROOT}/adapters/system/include
)
target_link_libraries(adapters_system PUBLIC
    ports lifecycle lighting door_lock vehicle_mode can_gateway diagnostics
    speed_simulator pthread)
add_subdirectory(${LIBS_ROOT}/adapters/system/tests adapters_system_tests)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/config-loader config-loader)
add_subdirectory(${LIBS_ROOT}/platform/can-gateway can-gateway)
add_subdirectory(${LIBS_ROOT}/platform/diagnostics diagnostics)
add_subdirectory(${LIBS_ROOT}/platform/cloud-gateway cloud-gateway)

# Body domain modules
add_subdirectory(${LIBS_ROOT}/body/lighting lighting)
add_subdirectory(${LIBS_ROOT}/body/door-lock door-lock)
add_subdirectory(${LIBS_ROOT}/body/vehicle-mode vehicle-mode)
add_subdirectory(${LIBS_ROOT}/body/speed-simulator speed-simulator)

# Linux adapters (InProcessSignalBus, StubCloudTransport, etc.)
add_subdirectory(${LIBS_ROOT}/adapters/linux adapters-linux)
add_subdirectory(${LIBS_ROOT}/adapters/linux/tests adapters_linux_tests)

# AutoSD adapters (SomeIpKuksaBridge -- stub mode, no gRPC/NATS needed)
add_subdirectory(${LIBS_ROOT}/adapters/autosd adapters-autosd)
add_subdirectory(${LIBS_ROOT}/adapters/autosd/tests adapters_autosd_tests)
