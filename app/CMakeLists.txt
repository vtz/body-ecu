cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(body_ecu LANGUAGES C CXX)

set(LIBS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../libs)

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
add_subdirectory(${LIBS_ROOT}/adapters/openbsw ${CMAKE_CURRENT_BINARY_DIR}/adapters-openbsw)

target_sources(app PRIVATE src/main.cpp)
target_include_directories(app PRIVATE
    ${LIBS_ROOT}/adapters/openbsw/include
    ${LIBS_ROOT}/adapters/zephyr/include
)
target_link_libraries(app PRIVATE
    ports
    lighting
    door_lock
    vehicle_mode
    can_gateway
    diagnostics
)
