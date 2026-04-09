add_executable(config_loader_test ConfigLoaderTest.cpp)
target_link_libraries(config_loader_test PRIVATE
    config_loader
    GTest::gtest_main
)
gtest_discover_tests(config_loader_test)
