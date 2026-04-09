add_executable(can_gateway_test CanGatewayTest.cpp)
target_link_libraries(can_gateway_test PRIVATE
    can_gateway
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(can_gateway_test)
