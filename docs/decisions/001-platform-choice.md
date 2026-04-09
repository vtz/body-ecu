# ADR-001: Platform Choice

## Status

Accepted

## Context

We need an RTOS and BSW framework for a Body ECU spike running on the
NUCLEO-H755ZI-Q (STM32H755, dual-core Cortex-M7/M4). Requirements:

- Ethernet support (for SOME/IP and DoIP)
- CAN-FD support
- Modular lifecycle management
- Path to both ThreadX and Zephyr RTOS

## Decision

Use **Zephyr RTOS** via the **openbsw-zephyr** integration layer, with
**OpenBSW** providing the lifecycle management framework.

- OpenBSW provides `AsyncLifecycleComponent` for modular service design,
  plus ready-made DoIP, DoCAN, and UDS stacks.
- openbsw-zephyr adapts OpenBSW's async primitives to run on Zephyr threads.
- The NUCLEO-H755ZI-Q board is already supported by openbsw-zephyr.

## Consequences

- We get immediate board support without porting effort.
- OpenBSW's lifecycle pattern (init/run/shutdown) gives us modular,
  testable services.
- ThreadX migration is possible later: OpenBSW natively supports ThreadX
  in its main repo. Domain logic and port interfaces remain unchanged.
- Zephyr's driver model (GPIO, CAN, Ethernet) is used behind port
  interfaces in `libs/adapters/zephyr/`.

## Alternatives Considered

- **FreeRTOS + OpenBSW (main repo):** Supported but requires more porting
  for the NUCLEO-H755ZI-Q. FreeRTOS has weaker driver model for STM32.
- **Bare Zephyr (no OpenBSW):** Loses the lifecycle framework, DoIP/DoCAN
  stacks, and the path to ThreadX.
- **ThreadX directly:** OpenBSW supports ThreadX for POSIX and S32K148.
  Porting to STM32H755 is feasible but would be significant upfront work.
