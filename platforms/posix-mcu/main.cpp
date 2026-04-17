#include <async/AsyncBinding.h>
#include <bsp/timer/SystemTimer.h>

#include <cstdio>
#include <signal.h>
#include <unistd.h>

extern void app_main();

extern "C"
{
void putchar_(char character) { std::putchar(character); }
}

void intHandler(int /* sig */)
{
    _exit(0);
}

int main()
{
    signal(SIGINT, intHandler);

    std::setbuf(stdout, nullptr);

    using AsyncAdapter = ::async::AsyncBinding::AdapterType;
    AsyncAdapter::init();
    app_main();
    return 1;
}
