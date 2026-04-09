add_library(lighting STATIC src/LightingController.cpp)
target_include_directories(lighting PUBLIC include)
target_link_libraries(lighting PUBLIC ports)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
