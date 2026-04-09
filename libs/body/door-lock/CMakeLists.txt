add_library(door_lock STATIC src/DoorLockController.cpp)
target_include_directories(door_lock PUBLIC include)
target_link_libraries(door_lock PUBLIC ports)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
