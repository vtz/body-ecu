include(FetchContent)

FetchContent_Declare(
    opensomeip
    GIT_REPOSITORY https://github.com/vtz/opensomeip.git
    GIT_TAG v0.0.5
)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SOMEIP_DEV_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(opensomeip)
