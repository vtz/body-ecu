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
