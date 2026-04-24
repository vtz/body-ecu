# TODO

## Networking: Mac as router between NUCLEO and AutoSD board

**Goal:** Run MCU on NUCLEO (192.168.100.10) and MPU on Qualcomm/AutoSD board
(10.26.28.69) with SOME/IP communication between them.

**Problem:** The two boards are on different subnets with no direct route.
The Mac is the only device that can reach both (en6 for NUCLEO, VPN for AutoSD).

**Steps:**
- [ ] Enable IP forwarding on macOS: `sudo sysctl -w net.inet.ip.forwarding=1`
- [ ] Set up NAT/routing between en6 (192.168.100.x) and the VPN tunnel
- [ ] Configure NUCLEO's default gateway to the Mac (192.168.100.1)
- [ ] Ensure the AutoSD board can route back to 192.168.100.x via the VPN
- [ ] Note: SOME/IP SD multicast will NOT work across VPN — may need
      unicast SD or static endpoint configuration

## CI: Enable Zephyr / Renode builds

- [ ] Set up west workspace in CI (checkout Zephyr, OpenBSW, opensomeip)
- [ ] Re-enable Zephyr build matrix (nucleo_h753zi at minimum)
- [ ] Re-enable Renode smoke test
