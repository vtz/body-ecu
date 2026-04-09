add_executable(vehicle_mode_test VehicleModeManagerTest.cpp)
target_link_libraries(vehicle_mode_test PRIVATE
    vehicle_mode
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(vehicle_mode_test)
