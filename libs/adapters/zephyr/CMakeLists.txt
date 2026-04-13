if(BUILD_TESTS)
    # Zephyr adapters are not built in host test mode
    return()
endif()

zephyr_library_named(adapters_zephyr)
zephyr_library_sources(
    src/GpioAdapter.cpp
    src/CanAdapter.cpp
    src/ButtonAdapter.cpp
    src/LocalSignalBus.cpp
)
zephyr_library_include_directories(include)
zephyr_library_link_libraries(ports)
