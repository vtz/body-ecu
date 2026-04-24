add_executable(ignition_test IgnitionControllerTest.cpp)
target_link_libraries(ignition_test PRIVATE
    ignition
    vehicle_mode
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(ignition_test)
