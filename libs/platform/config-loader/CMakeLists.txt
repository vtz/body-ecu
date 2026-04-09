add_library(config_loader STATIC src/ConfigLoader.cpp)
target_include_directories(config_loader PUBLIC include)
target_link_libraries(config_loader PUBLIC yaml-cpp::yaml-cpp)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
