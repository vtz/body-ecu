# cmake/opensomeip.cmake — Provide the someip-transport target for POSIX builds.
#
# Resolution order:
#   1. OPENSOMEIP_DIR cache/env variable (explicit override)
#   2. West module at <workspace>/modules/opensomeip (west.yml path)
#   3. FetchContent from GitHub (standalone builds without west)

if(TARGET someip-transport)
    return()
endif()

get_filename_component(_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_WEST_OPENSOMEIP "${_REPO_ROOT}/../modules/opensomeip")

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SOMEIP_DEV_TOOLS OFF CACHE BOOL "" FORCE)

if(OPENSOMEIP_DIR)
    message(STATUS "[opensomeip] Using OPENSOMEIP_DIR=${OPENSOMEIP_DIR}")
    add_subdirectory(${OPENSOMEIP_DIR} ${CMAKE_CURRENT_BINARY_DIR}/_deps/opensomeip)
elseif(EXISTS "${_WEST_OPENSOMEIP}/CMakeLists.txt")
    message(STATUS "[opensomeip] Using west module at ${_WEST_OPENSOMEIP}")
    add_subdirectory(${_WEST_OPENSOMEIP} ${CMAKE_CURRENT_BINARY_DIR}/_deps/opensomeip)
else()
    message(STATUS "[opensomeip] West module not found, falling back to FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        opensomeip
        GIT_REPOSITORY https://github.com/vtz/opensomeip.git
        GIT_TAG v0.0.5
    )
    FetchContent_MakeAvailable(opensomeip)
endif()
