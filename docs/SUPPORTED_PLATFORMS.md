# Supported Platforms

## Build Targets

| Platform | Board ID | RTOS | Notes |
|----------|----------|------|-------|
| native_sim | `native_sim` | Zephyr (POSIX) | Host simulation, GPIO mocked |
| NUCLEO-H755ZI-Q | `nucleo_h755zi_q/stm32h755xx/m7` | Zephyr | M7 core only, Ethernet + CAN-FD |

## Emulation

| Platform | Tool | Script | Notes |
|----------|------|--------|-------|
| STM32H753 | [Renode](https://renode.io) | `renode/body_ecu.resc` | Uses nucleo_h753zi.repl (single-core sibling) |

The Renode platform uses the STM32H753 (single-core sibling of the dual-core H755)
because Renode has an existing platform description for it. OpenBSW runs on the M7
core only, so single-core emulation is sufficient.

## Future

| Platform | Notes |
|----------|-------|
| Linux (HPC) | Replace `libs/adapters/zephyr/` with `libs/adapters/linux/` |
