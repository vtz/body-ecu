add_executable(cloud_gateway_client_test CloudGatewayClientTest.cpp)
target_link_libraries(cloud_gateway_client_test PRIVATE
    cloud_gateway_client port_mocks GTest::gtest_main GTest::gmock
)
gtest_discover_tests(cloud_gateway_client_test)
