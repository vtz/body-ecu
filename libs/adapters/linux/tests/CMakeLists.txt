add_executable(in_process_signal_bus_test InProcessSignalBusTest.cpp)
target_link_libraries(in_process_signal_bus_test PRIVATE
    adapters_linux
    GTest::gtest_main
)
gtest_discover_tests(in_process_signal_bus_test)

add_executable(stub_cloud_transport_test StubCloudTransportTest.cpp)
target_link_libraries(stub_cloud_transport_test PRIVATE
    adapters_linux
    GTest::gtest_main
)
gtest_discover_tests(stub_cloud_transport_test)
