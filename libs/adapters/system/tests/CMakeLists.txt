add_executable(someip_system_test SomeIpSystemTest.cpp)
target_link_libraries(someip_system_test PRIVATE
    adapters_system
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(someip_system_test)

add_executable(system_wrappers_test SystemWrappersTest.cpp)
target_link_libraries(system_wrappers_test PRIVATE
    adapters_system
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(system_wrappers_test)

add_executable(transport_test TransportTest.cpp)
target_link_libraries(transport_test PRIVATE
    adapters_system
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(transport_test)

add_executable(can_rx_multicast_test CanRxMulticastTest.cpp)
target_link_libraries(can_rx_multicast_test PRIVATE
    adapters_system
    can_gateway
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(can_rx_multicast_test)

add_executable(vehicle_info_test VehicleInfoTest.cpp)
target_link_libraries(vehicle_info_test PRIVATE
    adapters_system
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(vehicle_info_test)
