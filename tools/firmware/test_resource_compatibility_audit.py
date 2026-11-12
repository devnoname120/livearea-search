from __future__ import annotations

import json
from pathlib import Path
import struct
import tempfile
import unittest

from resource_compatibility_audit import (
    audit_resource_bytes,
    discover_resource_paths,
    load_resource_manifest_data,
    main,
)


def _manifest() -> dict[str, object]:
    return {
        "schema": 1,
        "minimum_version": "3.600",
        "resources": {
            "topmenu": {
                "file_names": ["topmenu_plugin.rco"],
                "required_u32_le": {
                    "home_page": "0x5E9535B3",
                    "home_scroll": "0x9CC405A3",
                },
                "exact_count": 1,
            },
            "indicator": {
                "file_names": ["indicator_plugin.rco"],
                "required_u32_le": {
                    "indicator_page": "0xFFD7E3F0",
                    "notification_corner": "0x8BF9D3C5",
                },
                "exact_count": 1,
            }
        },
    }


class ResourceCompatibilityAuditTests(unittest.TestCase):
    def test_audits_required_little_endian_identifiers(self) -> None:
        manifest = load_resource_manifest_data(_manifest())
        requirement = manifest.resources["topmenu"]
        image = (
            b"prefix"
            + struct.pack("<I", 0x5E9535B3)
            + b"middle"
            + struct.pack("<I", 0x9CC405A3)
        )

        self.assertEqual(audit_resource_bytes(image, requirement), ())

        duplicate = image + struct.pack("<I", 0x5E9535B3)
        issues = audit_resource_bytes(duplicate, requirement)
        self.assertTrue(any("home_page" in issue and "found 2" in issue for issue in issues))

    def test_audits_notification_corner_identifier(self) -> None:
        manifest = load_resource_manifest_data(_manifest())
        requirement = manifest.resources["indicator"]
        image = struct.pack("<II", 0xFFD7E3F0, 0x8BF9D3C5)

        self.assertEqual(audit_resource_bytes(image, requirement), ())

        issues = audit_resource_bytes(
            struct.pack("<I", 0xFFD7E3F0),
            requirement,
        )
        self.assertTrue(
            any("notification_corner" in issue and "found 0" in issue for issue in issues)
        )

    def test_discovers_only_supported_resource_files(self) -> None:
        manifest = load_resource_manifest_data(_manifest())
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            supported = root / "external/testkit/3.600.011/fs/vs0/vsh/shell/topmenu_plugin.rco"
            unsupported = root / "external/testkit/3.570.011/fs/vs0/vsh/shell/topmenu_plugin.rco"
            indicator = root / "external/testkit/3.730.011/fs/vs0/vsh/shell/indicator_plugin.rco"
            for path in (supported, unsupported, indicator):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture")

            discovered = discover_resource_paths((root,), manifest)

        self.assertEqual(discovered, (supported, indicator))

    def test_cli_writes_resource_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            firmware = root / "firmware/external/official/3.60/fs/vs0/vsh/shell/topmenu_plugin.rco"
            firmware.parent.mkdir(parents=True)
            firmware.write_bytes(
                struct.pack("<II", 0x5E9535B3, 0x9CC405A3)
            )
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(_manifest()))
            json_output = root / "report.json"
            markdown_output = root / "report.md"

            result = main(
                (
                    "--manifest",
                    str(manifest_path),
                    "--root",
                    str(root / "firmware"),
                    "--json-out",
                    str(json_output),
                    "--markdown-out",
                    str(markdown_output),
                )
            )

            report = json.loads(json_output.read_text())
            markdown = markdown_output.read_text()

        self.assertEqual(result, 0)
        self.assertEqual(report["summary"], {"resources": 1, "passed": 1, "failed": 0})
        self.assertEqual(
            report["resources"][0]["path"],
            "firmware/external/official/3.60/fs/vs0/vsh/shell/topmenu_plugin.rco",
        )
        self.assertIn("1 resources: 1 passed, 0 failed", markdown)


if __name__ == "__main__":
    unittest.main()
