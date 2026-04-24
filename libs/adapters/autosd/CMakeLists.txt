add_library(adapters_autosd STATIC
    src/KuksaSignalBusAdapter.cpp
    src/NatsCloudTransportAdapter.cpp
    src/SomeIpKuksaBridge.cpp
    src/SystemdLifecycleAdapter.cpp
)
target_include_directories(adapters_autosd PUBLIC include)
target_link_libraries(adapters_autosd PUBLIC ports)

if(TARGET kuksa_proto)
    target_link_libraries(adapters_autosd PUBLIC kuksa_proto)
    target_compile_definitions(adapters_autosd PUBLIC HAS_KUKSA_GRPC)
endif()

if(TARGET nats_static)
    target_link_libraries(adapters_autosd PUBLIC nats_static)
    target_compile_definitions(adapters_autosd PUBLIC HAS_NATS)
endif()
