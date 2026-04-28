#!/usr/bin/env python3
"""Generate C++ SOME/IP config headers from services.yaml.

Usage: generate_someip_config.py <services.yaml> <output_dir>

Produces:
  <output_dir>/someip_service_ids.h   -- shared constexpr IDs (MCU + MPU)
  <output_dir>/someip_mpu_config.h    -- MpuClientConfig struct (MPU only)
"""

import os
import sys
from pathlib import Path

import yaml


def snake_to_pascal(name: str) -> str:
    return "".join(word.capitalize() for word in name.split("_"))


def emit_service_ids(services: dict) -> str:
    lines = [
        "// Auto-generated from services.yaml -- DO NOT EDIT",
        "#pragma once",
        "#include <cstdint>",
        "",
        "namespace body_ecu::someip {",
    ]

    for svc_name, svc in services.items():
        if "service_id" not in svc:
            continue

        lines.append("")
        lines.append(f"namespace {svc_name} {{")
        lines.append(f"    inline constexpr uint16_t kServiceId  = {svc['service_id']:#06x};")
        lines.append(f"    inline constexpr uint16_t kInstanceId = {svc['instance_id']:#06x};")

        if "methods" in svc:
            lines.append("    namespace method {")
            for method_name, method_id in svc["methods"].items():
                lines.append(
                    f"        inline constexpr uint16_t k{snake_to_pascal(method_name)} = {method_id:#06x};"
                )
            lines.append("    }")

        if "fields" in svc:
            lines.append("    namespace field {")
            for field_name, field in svc["fields"].items():
                prefix = snake_to_pascal(field_name)
                lines.append(
                    f"        inline constexpr uint16_t k{prefix}Getter  = {field['getter']:#06x};"
                )
                lines.append(
                    f"        inline constexpr uint16_t k{prefix}Setter  = {field['setter']:#06x};"
                )
                lines.append(
                    f"        inline constexpr uint16_t k{prefix}Notifier = {field['notifier']:#06x};"
                )
            lines.append("    }")

        if "events" in svc:
            lines.append("    namespace event {")
            for event_name, event_id in svc["events"].items():
                lines.append(
                    f"        inline constexpr uint16_t k{snake_to_pascal(event_name)} = {event_id:#06x};"
                )
            lines.append("    }")

        if "eventgroups" in svc:
            lines.append("    namespace eventgroup {")
            for eg_name, eg_id in svc["eventgroups"].items():
                lines.append(
                    f"        inline constexpr uint16_t k{snake_to_pascal(eg_name)} = {eg_id:#06x};"
                )
            lines.append("    }")

        lines.append("}")

    lines.append("")
    lines.append("}  // namespace body_ecu::someip")
    lines.append("")
    return "\n".join(lines)


def emit_mpu_config(services: dict) -> str:
    lines = [
        "// Auto-generated from services.yaml -- DO NOT EDIT",
        "#pragma once",
        '#include "someip_service_ids.h"',
        "",
        "namespace body_ecu::someip {",
        "",
        "struct MpuClientConfig {",
    ]

    for svc_name, svc in services.items():
        if "service_id" not in svc:
            continue

        lines.append(f"    // {svc_name}")
        lines.append(
            f"    uint16_t {svc_name}_service_id = {svc_name}::kServiceId;"
        )

        if "methods" in svc:
            for method_name, _ in svc["methods"].items():
                lines.append(
                    f"    uint16_t {svc_name}_{method_name}_method = "
                    f"{svc_name}::method::k{snake_to_pascal(method_name)};"
                )

        if "fields" in svc:
            for field_name, _ in svc["fields"].items():
                prefix = snake_to_pascal(field_name)
                lines.append(
                    f"    uint16_t {svc_name}_{field_name}_getter = "
                    f"{svc_name}::field::k{prefix}Getter;"
                )
                lines.append(
                    f"    uint16_t {svc_name}_{field_name}_setter = "
                    f"{svc_name}::field::k{prefix}Setter;"
                )

        if "events" in svc:
            for event_name, _ in svc["events"].items():
                lines.append(
                    f"    uint16_t {svc_name}_{event_name}_event = "
                    f"{svc_name}::event::k{snake_to_pascal(event_name)};"
                )

        if "eventgroups" in svc:
            for eg_name, _ in svc["eventgroups"].items():
                lines.append(
                    f"    uint16_t {svc_name}_{eg_name}_eventgroup = "
                    f"{svc_name}::eventgroup::k{snake_to_pascal(eg_name)};"
                )

        lines.append("")

    lines.append("};")
    lines.append("")
    lines.append("}  // namespace body_ecu::someip")
    lines.append("")
    return "\n".join(lines)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <services.yaml> <output_dir>", file=sys.stderr)
        sys.exit(1)

    yaml_path = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])

    with open(yaml_path) as f:
        config = yaml.safe_load(f)

    services = config.get("services", {})

    out_dir.mkdir(parents=True, exist_ok=True)

    ids_path = out_dir / "someip_service_ids.h"
    ids_content = emit_service_ids(services)
    ids_path.write_text(ids_content)

    mpu_path = out_dir / "someip_mpu_config.h"
    mpu_content = emit_mpu_config(services)
    mpu_path.write_text(mpu_content)

    print(f"Generated {ids_path}")
    print(f"Generated {mpu_path}")


if __name__ == "__main__":
    main()
