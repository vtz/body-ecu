add_library(diagnostics STATIC
    src/UdsServiceHandler.cpp
    src/DtcStore.cpp
)
target_include_directories(diagnostics PUBLIC include)
target_link_libraries(diagnostics PUBLIC ports)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
