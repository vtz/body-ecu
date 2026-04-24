cmake_minimum_required(VERSION 3.14)
project(body_ecu_autosd CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BODY_ECU_REAL_ADAPTERS
    "Build with real Kuksa gRPC and NATS adapters (requires grpc-devel, protobuf-devel, openssl-devel)"
    OFF)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

# OpenSOME/IP stack (real UDP transport)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/opensomeip.cmake)

# --- Real adapter dependencies (Kuksa gRPC + nats.c) ---
if(BODY_ECU_REAL_ADAPTERS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/kuksa_proto.cmake)

    include(FetchContent)
    set(NATS_BUILD_STREAMING OFF CACHE BOOL "" FORCE)
    set(NATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(NATS_BUILD_LIB_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        cnats
        GIT_REPOSITORY https://github.com/nats-io/nats.c.git
        GIT_TAG        v3.9.1
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(cnats)

    # cnats exports src/ but public headers live in src/include/
    if(TARGET nats_static)
        target_include_directories(nats_static INTERFACE
            $<BUILD_INTERFACE:${cnats_SOURCE_DIR}/src/include>)
    endif()

    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(SYSTEMD libsystemd)
    endif()
endif()

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

if(BODY_ECU_REAL_ADAPTERS AND SYSTEMD_FOUND)
    target_link_libraries(body_ecu_autosd PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(body_ecu_autosd PRIVATE ${SYSTEMD_INCLUDE_DIRS})
    target_compile_definitions(body_ecu_autosd PRIVATE HAS_SYSTEMD)
endif()
