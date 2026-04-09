add_executable(someip_system_test SomeIpSystemTest.cpp)
target_link_libraries(someip_system_test PRIVATE
    adapters_openbsw
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(someip_system_test)

add_executable(system_wrappers_test SystemWrappersTest.cpp)
target_link_libraries(system_wrappers_test PRIVATE
    adapters_openbsw
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(system_wrappers_test)

add_executable(transport_test TransportTest.cpp)
target_link_libraries(transport_test PRIVATE
    adapters_openbsw
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(transport_test)
