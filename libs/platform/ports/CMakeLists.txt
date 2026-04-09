add_library(ports INTERFACE)
target_include_directories(ports INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(port_mocks INTERFACE)
target_include_directories(port_mocks INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/mock
)
target_link_libraries(port_mocks INTERFACE ports GTest::gmock)
