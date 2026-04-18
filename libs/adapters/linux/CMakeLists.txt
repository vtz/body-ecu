add_library(adapters_linux STATIC
    src/ConsoleGpioAdapter.cpp
    src/SocketCanAdapter.cpp
    src/StdinButtonAdapter.cpp
    src/InProcessSignalBus.cpp
    src/StubCloudTransport.cpp
    src/ThreadTimerService.cpp
    src/SimulatedAdcAdapter.cpp
)
target_include_directories(adapters_linux PUBLIC include)
target_link_libraries(adapters_linux PUBLIC ports)

find_package(Threads REQUIRED)
target_link_libraries(adapters_linux PUBLIC Threads::Threads)
