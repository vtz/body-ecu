add_executable(door_lock_test DoorLockControllerTest.cpp)
target_link_libraries(door_lock_test PRIVATE
    door_lock
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(door_lock_test)
