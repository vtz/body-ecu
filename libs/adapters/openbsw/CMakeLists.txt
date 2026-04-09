if(BUILD_TESTS)
    return()
endif()

zephyr_library_named(adapters_openbsw)
zephyr_library_sources(
    src/SomeIpSystem.cpp
    src/LightingSystem.cpp
    src/DoorLockSystem.cpp
    src/VehicleModeSystem.cpp
    src/CanGatewaySystem.cpp
    src/DiagnosticsSystem.cpp
    src/DoIpTransport.cpp
    src/DoCanTransport.cpp
)
zephyr_library_include_directories(include)
zephyr_library_link_libraries(ports lighting door_lock vehicle_mode can_gateway diagnostics)
