cmake_minimum_required(VERSION 3.20.0)

set(OPENBSW_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../openbsw
    CACHE PATH "Path to Eclipse OpenBSW")

set(OPENBSW_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../openbswConfig)

set(INCLUDE_OPENBSW_LIBS_BSP
    OFF
    CACHE BOOL "Include openbsw/libs/bsp/ in build")

add_compile_options($<TARGET_PROPERTY:zephyr_interface,INTERFACE_COMPILE_OPTIONS>)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(body_ecu LANGUAGES C CXX)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../libs)

include(${OPENBSW_DIR}/Filelists.cmake)

# OpenBSW forces CXX_STANDARD=14; body-ecu requires C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# openbsw-zephyr adaptation libraries (only asyncZephyr + bspZephyr, not zephyrEthAdapter)
set(OPENBSW_ZEPHYR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/openbsw-zephyr/libs)
add_subdirectory(${OPENBSW_ZEPHYR_DIR}/asyncZephyr ${CMAKE_CURRENT_BINARY_DIR}/asyncZephyr)
add_subdirectory(${OPENBSW_ZEPHYR_DIR}/bspZephyr ${CMAKE_CURRENT_BINARY_DIR}/bspZephyr)

add_library(asyncPlatform ALIAS asyncZephyr)

# OpenBSW configuration (use Zephyr async binding)
add_subdirectory(${OPENBSW_CONFIG_DIR}/asyncCoreConfiguration
    ${CMAKE_CURRENT_BINARY_DIR}/asyncCoreConfiguration)
add_subdirectory(${OPENBSW_CONFIG_DIR}/asyncBindingZephyr
    ${CMAKE_CURRENT_BINARY_DIR}/asyncBinding)
add_subdirectory(${OPENBSW_CONFIG_DIR}/configuration
    ${CMAKE_CURRENT_BINARY_DIR}/configuration)

# Port interfaces (header-only)
add_subdirectory(${LIBS_ROOT}/platform/ports ${CMAKE_CURRENT_BINARY_DIR}/ports)

# Domain modules
add_subdirectory(${LIBS_ROOT}/body/lighting ${CMAKE_CURRENT_BINARY_DIR}/lighting)
add_subdirectory(${LIBS_ROOT}/body/door-lock ${CMAKE_CURRENT_BINARY_DIR}/door-lock)
add_subdirectory(${LIBS_ROOT}/body/vehicle-mode ${CMAKE_CURRENT_BINARY_DIR}/vehicle-mode)

# Platform modules
add_subdirectory(${LIBS_ROOT}/platform/can-gateway ${CMAKE_CURRENT_BINARY_DIR}/can-gateway)
add_subdirectory(${LIBS_ROOT}/platform/diagnostics ${CMAKE_CURRENT_BINARY_DIR}/diagnostics)

# Platform adapters
add_subdirectory(${LIBS_ROOT}/adapters/zephyr ${CMAKE_CURRENT_BINARY_DIR}/adapters-zephyr)
add_subdirectory(${LIBS_ROOT}/adapters/system ${CMAKE_CURRENT_BINARY_DIR}/adapters-system)

target_sources(app PRIVATE src/main.cpp)
target_include_directories(app PRIVATE
    ${LIBS_ROOT}/adapters/system/include
    ${LIBS_ROOT}/adapters/zephyr/include
)
target_link_libraries(app PRIVATE
    ports
    lifecycle
    lighting
    door_lock
    vehicle_mode
    can_gateway
    diagnostics
    adapters_zephyr
    adapters_system
    asyncBinding
    asyncZephyrImpl
    configuration
    bspZephyr
    logger
    loggerIntegration
    etl
)
