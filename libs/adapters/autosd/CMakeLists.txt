add_library(adapters_autosd STATIC
    src/KuksaSignalBusAdapter.cpp
    src/NatsCloudTransportAdapter.cpp
    src/SomeIpKuksaBridge.cpp
)
target_include_directories(adapters_autosd PUBLIC include)
target_link_libraries(adapters_autosd PUBLIC ports)
