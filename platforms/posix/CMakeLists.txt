cmake_minimum_required(VERSION 3.14)
project(body_ecu_posix CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)
set(OPENBSW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../openbsw CACHE PATH "OpenBSW root")
set(OPENBSW_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../openbswConfig)

# --- OpenBSW core libraries ---
set(PLATFORM_SUPPORT_CAN OFF)
set(PLATFORM_SUPPORT_UDS OFF)
set(INCLUDE_OPENBSW_LIBS_BSP ON)
include(${OPENBSW_DIR}/Filelists.cmake)

# macOS: remove GNU-ld-only flags added by Filelists.cmake
if(APPLE)
    get_directory_property(_link_opts LINK_OPTIONS)
    list(REMOVE_ITEM _link_opts "-Wl,--noinhibit-exec")
    set_directory_properties(PROPERTIES LINK_OPTIONS "${_link_opts}")
endif()

# POSIX platform: FreeRTOS port + selected BSP modules
add_subdirectory(${OPENBSW_DIR}/platforms/posix/3rdparty/freeRtosPosix
    ${CMAKE_CURRENT_BINARY_DIR}/freeRtosPosix)
if(APPLE)
    target_compile_options(freeRtosPosix PRIVATE -Wno-int-conversion)
endif()
add_library(freeRtosPort ALIAS freeRtosPosixPort)
add_library(freeRtosPortImpl ALIAS freeRtosPosix)

add_subdirectory(${OPENBSW_DIR}/platforms/posix/bsp/bspInterruptsImpl
    ${CMAKE_CURRENT_BINARY_DIR}/bspInterruptsImpl)
add_subdirectory(${OPENBSW_DIR}/platforms/posix/bsp/bspMcu
    ${CMAKE_CURRENT_BINARY_DIR}/bspMcu)
add_subdirectory(${OPENBSW_DIR}/platforms/posix/bsp/bspStdio
    ${CMAKE_CURRENT_BINARY_DIR}/bspStdio)
add_subdirectory(${OPENBSW_DIR}/platforms/posix/bsp/bspSystemTime
    ${CMAKE_CURRENT_BINARY_DIR}/bspSystemTime)

# Async: FreeRTOS-based for POSIX
add_library(asyncPlatform ALIAS asyncFreeRtos)
add_library(osFreeRtos ALIAS freeRtos)

# OpenBSW app configuration targets
add_subdirectory(${OPENBSW_CONFIG_DIR}/asyncCoreConfiguration
    ${CMAKE_CURRENT_BINARY_DIR}/asyncCoreConfiguration)
add_subdirectory(${OPENBSW_CONFIG_DIR}/asyncBinding
    ${CMAKE_CURRENT_BINARY_DIR}/asyncBinding)
add_subdirectory(${OPENBSW_CONFIG_DIR}/freeRtosCoreConfiguration
    ${CMAKE_CURRENT_BINARY_DIR}/freeRtosCoreConfiguration)
add_subdirectory(${OPENBSW_CONFIG_DIR}/configuration
    ${CMAKE_CURRENT_BINARY_DIR}/configuration)

# FreeRTOS OS hooks (required by freeRtos target)
add_library(osHooks STATIC src/osHooks.cpp)
target_link_libraries(osHooks PRIVATE freeRtos)

# --- Body ECU own libraries ---

# OpenSOME/IP stack
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/opensomeip.cmake)

# SOME/IP code generation from services.yaml
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/someip_codegen.cmake)

# Port interfaces (header-only)
add_subdirectory(${LIBS_ROOT}/platform/ports ${CMAKE_CURRENT_BINARY_DIR}/ports)

# Domain modules
add_subdirectory(${LIBS_ROOT}/body/lighting ${CMAKE_CURRENT_BINARY_DIR}/lighting)
add_subdirectory(${LIBS_ROOT}/body/door-lock ${CMAKE_CURRENT_BINARY_DIR}/door-lock)
add_subdirectory(${LIBS_ROOT}/body/vehicle-mode ${CMAKE_CURRENT_BINARY_DIR}/vehicle-mode)
add_subdirectory(${LIBS_ROOT}/body/speed-simulator ${CMAKE_CURRENT_BINARY_DIR}/speed-simulator)
add_subdirectory(${LIBS_ROOT}/body/ignition ${CMAKE_CURRENT_BINARY_DIR}/ignition)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/can-gateway ${CMAKE_CURRENT_BINARY_DIR}/can-gateway)
add_subdirectory(${LIBS_ROOT}/platform/diagnostics ${CMAKE_CURRENT_BINARY_DIR}/diagnostics)

# System adapter layer
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
    ${LIBS_ROOT}/adapters/system/src/IgnitionSystem.cpp
)
target_include_directories(adapters_system PUBLIC
    ${LIBS_ROOT}/adapters/system/include
)
target_link_libraries(adapters_system PUBLIC
    ports lifecycle lighting door_lock vehicle_mode speed_simulator ignition can_gateway diagnostics
    someip-transport
)
target_compile_definitions(adapters_system PUBLIC HAS_OPENSOMEIP)

# Linux platform adapters
add_subdirectory(${LIBS_ROOT}/adapters/linux ${CMAKE_CURRENT_BINARY_DIR}/adapters-linux)

# POSIX executable
add_executable(body_ecu_posix main.cpp src/app.cpp)
target_include_directories(body_ecu_posix PRIVATE include)
target_link_libraries(body_ecu_posix PRIVATE
    adapters_system
    adapters_linux
    lifecycle
    asyncBinding
    asyncFreeRtos
    $<$<PLATFORM_ID:Linux>:-Wl,--whole-archive>
    asyncFreeRtosImpl
    $<$<PLATFORM_ID:Linux>:-Wl,--no-whole-archive>
    logger
    loggerIntegration
    bspInterruptsImpl
    bspSystemTime
    bspStdio
    bspMcu
    configuration
    pthread
)
