add_library(vehicle_mode STATIC src/VehicleModeManager.cpp)
target_include_directories(vehicle_mode PUBLIC include)
target_link_libraries(vehicle_mode PUBLIC ports)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
