#pragma once

namespace body_ecu::adapters {

/// Thin wrapper around sd_notify for systemd service lifecycle.
/// When HAS_SYSTEMD is not defined, all methods are no-ops.
class SystemdLifecycleAdapter {
public:
    static void notifyReady();
    static void notifyStopping();
    static void notifyStatus(const char* status);
};

}  // namespace body_ecu::adapters
