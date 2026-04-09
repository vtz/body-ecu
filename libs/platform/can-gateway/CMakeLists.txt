add_library(can_gateway STATIC
    src/CanGateway.cpp
    src/MessageTranslator.cpp
)
target_include_directories(can_gateway PUBLIC include)
target_link_libraries(can_gateway PUBLIC ports)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
