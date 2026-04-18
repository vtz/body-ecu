add_executable(speed_simulator_test SpeedSimulatorTest.cpp)
target_link_libraries(speed_simulator_test PRIVATE
    speed_simulator
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(speed_simulator_test)
