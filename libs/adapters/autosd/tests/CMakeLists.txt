# SomeIpKuksaBridge test (no external deps, always buildable)
add_executable(someip_kuksa_bridge_test SomeIpKuksaBridgeTest.cpp)
target_include_directories(someip_kuksa_bridge_test PRIVATE
    ${LIBS_ROOT}/platform/ports/mock
)
target_link_libraries(someip_kuksa_bridge_test PRIVATE
    adapters_autosd
    GTest::gtest_main
    GTest::gmock
)
gtest_discover_tests(someip_kuksa_bridge_test)

# KuksaSignalBusAdapter test (stub mode -- no gRPC needed)
add_executable(kuksa_signal_bus_adapter_test KuksaSignalBusAdapterTest.cpp)
target_link_libraries(kuksa_signal_bus_adapter_test PRIVATE
    adapters_autosd
    GTest::gtest_main
)
gtest_discover_tests(kuksa_signal_bus_adapter_test)

# NatsCloudTransportAdapter test (stub mode -- no nats.c needed)
add_executable(nats_cloud_transport_adapter_test NatsCloudTransportAdapterTest.cpp)
target_link_libraries(nats_cloud_transport_adapter_test PRIVATE
    adapters_autosd
    GTest::gtest_main
)
gtest_discover_tests(nats_cloud_transport_adapter_test)
