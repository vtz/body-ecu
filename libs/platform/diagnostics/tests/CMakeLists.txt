add_executable(diagnostics_test DiagnosticsTest.cpp)
target_link_libraries(diagnostics_test PRIVATE
    diagnostics
    port_mocks
    GTest::gtest_main
)
target_include_directories(diagnostics_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../mock
)
gtest_discover_tests(diagnostics_test)
