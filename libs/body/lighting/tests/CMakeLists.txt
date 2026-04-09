add_executable(lighting_test LightingControllerTest.cpp)
target_link_libraries(lighting_test PRIVATE
    lighting
    port_mocks
    GTest::gtest_main
)
gtest_discover_tests(lighting_test)
