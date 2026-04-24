#include "autosd_adapters/SystemdLifecycleAdapter.h"

#include <cstdio>

#ifdef HAS_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

namespace body_ecu::adapters {

void SystemdLifecycleAdapter::notifyReady() {
#ifdef HAS_SYSTEMD
    sd_notify(0, "READY=1");
#endif
    std::printf("[systemd] READY=1\n");
}

void SystemdLifecycleAdapter::notifyStopping() {
#ifdef HAS_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif
    std::printf("[systemd] STOPPING=1\n");
}

void SystemdLifecycleAdapter::notifyStatus(const char* status) {
#ifdef HAS_SYSTEMD
    sd_notifyf(0, "STATUS=%s", status);
#endif
    std::printf("[systemd] STATUS=%s\n", status);
}

}  // namespace body_ecu::adapters
