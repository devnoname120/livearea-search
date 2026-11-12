from __future__ import annotations

import json
import unittest
from pathlib import Path
import tempfile
from unittest.mock import patch

from compatibility_audit import (
    AuditRecord,
    FirmwareVersion,
    audit_image,
    audit_parsed_module,
    build_report,
    classify_firmware_family,
    discover_image_paths,
    export_signature,
    extract_firmware_version,
    load_manifest_data,
    main,
    render_markdown,
)
from test_vita_elf import _build_fixture
from vita_elf import ExportLibrary, ParsedModule


def _module(*, functions: tuple[int, ...] = (0x11111111, 0x22222222)) -> ParsedModule:
    return ParsedModule(
        name="SceExample",
        module_nid=0xAABBCCDD,
        version=0x0101,
        info_segment=0,
        info_offset=0x80,
        exports=(
            ExportLibrary(
                name="SceExampleLibrary",
                nid=0x12345678,
                function_nids=functions,
                variable_nids=(),
                tls_variable_nids=(),
            ),
        ),
    )


def _manifest(signature: str) -> dict[str, object]:
    return {
        "schema": 1,
        "minimum_version": "3.600",
        "modules": {
            "example": {
                "expected_name": "SceExample",
                "file_names": ["example.elf"],
                "allowed_export_signatures": [signature],
                "required_strings": ["required_plugin"],
                "libraries": [
                    {
                        "name": "SceExampleLibrary",
                        "nid": "0x12345678",
                        "function_nids": ["0x11111111", "0x22222222"],
                    }
                ],
            }
        },
    }


class CompatibilityAuditTests(unittest.TestCase):
    def test_extracts_supported_version_spellings(self) -> None:
        self.assertEqual(
            extract_firmware_version(Path("external/official/3.60/fs_dec/module.elf")),
            FirmwareVersion(3, 600),
        )
        self.assertEqual(
            extract_firmware_version(Path("external/devkit/3.730.011/fs_dec/module.elf")),
            FirmwareVersion(3, 730),
        )
        self.assertEqual(
            extract_firmware_version(Path("SCE/Internal/3.600/3_600_011.I.cex/fs_dec/module.elf")),
            FirmwareVersion(3, 600),
        )
        self.assertEqual(
            extract_firmware_version(Path("SCE/4.00_ina/inai400/fs_dec/module.elf")),
            FirmwareVersion(4, 0),
        )
        self.assertEqual(
            extract_firmware_version(Path("vitadump 3.60 ELF/ScePaf.enriched.elf")),
            FirmwareVersion(3, 600),
        )

    def test_classifies_firmware_families(self) -> None:
        cases = {
            "external/official/3.74/fs_dec/module.elf": "retail",
            "external/testkit/3.730.011/fs_dec/module.elf": "testkit",
            "external/devkit/3.730.011/fs_dec/module.elf": "devkit",
            "SCE/External/3.670/E.dex.release/fs_dec/module.elf": "external-dex",
            "SCE/Internal/3.720/3_720_011.I.devtool.release/fs_dec/module.elf": "internal-devtool",
            "SCE/Internal/3.650/3_650_011.I.cex_for_tool.release/fs_dec/module.elf": "internal-cex",
            "SCE/Internal (No Watermarks)/3.650/3_650._011.I.dex.release/fs_dec/module.elf": "internal-dex",
            "SCE/4.00_ina/inai400/fs_dec/module.elf": "internal-cex",
            "vitadump 3.60 ELF/ScePaf.enriched.elf": "retail-reference",
        }
        for raw, expected in cases.items():
            with self.subTest(path=raw):
                self.assertEqual(classify_firmware_family(Path(raw)), expected)

    def test_export_signature_is_order_independent_and_sensitive(self) -> None:
        original = _module()
        reordered = ParsedModule(
            name=original.name,
            module_nid=original.module_nid,
            version=original.version,
            info_segment=original.info_segment,
            info_offset=original.info_offset,
            exports=(
                ExportLibrary(
                    name="SceExampleLibrary",
                    nid=0x12345678,
                    function_nids=(0x22222222, 0x11111111),
                    variable_nids=(),
                    tls_variable_nids=(),
                ),
            ),
        )
        divergent = _module(functions=(0x11111111, 0x33333333))

        self.assertEqual(export_signature(original), export_signature(reordered))
        self.assertNotEqual(export_signature(original), export_signature(divergent))

    def test_audits_identity_signature_nids_and_strings(self) -> None:
        module = _module()
        manifest = load_manifest_data(_manifest(export_signature(module)))
        requirement = manifest.modules["example"]

        self.assertEqual(
            audit_parsed_module(module, b"prefix required_plugin suffix", requirement),
            (),
        )

        divergent = _module(functions=(0x11111111, 0x33333333))
        issues = audit_parsed_module(divergent, b"no marker", requirement)
        self.assertTrue(any("export signature" in issue for issue in issues))
        self.assertTrue(any("function NID 0x22222222" in issue for issue in issues))
        self.assertTrue(any("required string 'required_plugin'" in issue for issue in issues))

    def test_manifest_rejects_invalid_or_duplicate_values(self) -> None:
        module = _module()
        data = _manifest(export_signature(module))
        libraries = data["modules"]["example"]["libraries"]
        libraries[0]["function_nids"].append("0x11111111")

        with self.assertRaisesRegex(ValueError, "duplicate function NID"):
            load_manifest_data(data)

    def test_report_is_deterministic_and_surfaces_failures(self) -> None:
        records = (
            AuditRecord(
                path="z/module.elf",
                module_key="example",
                version=FirmwareVersion(3, 730),
                family="devkit",
                module_name="SceExample",
                module_nid=0x22222222,
                export_signature="bb",
                sha256="22",
                issues=("missing function NID 0x11111111",),
            ),
            AuditRecord(
                path="a/module.elf",
                module_key="example",
                version=FirmwareVersion(3, 600),
                family="retail",
                module_name="SceExample",
                module_nid=0x11111111,
                export_signature="aa",
                sha256="11",
                issues=(),
            ),
        )

        report = build_report(records)

        self.assertEqual(report["summary"], {"images": 2, "passed": 1, "failed": 1})
        self.assertEqual(
            [item["path"] for item in report["images"]],
            ["a/module.elf", "z/module.elf"],
        )
        markdown = render_markdown(report)
        self.assertIn("2 images: 1 passed, 1 failed", markdown)
        self.assertIn("missing function NID 0x11111111", markdown)

    def test_audits_real_elf_bytes_against_matching_requirement(self) -> None:
        image = _build_fixture()
        parsed = ParsedModule(
            name="TestModule",
            module_nid=0xAABBCCDD,
            version=0x0101,
            info_segment=0,
            info_offset=0x80,
            exports=(
                ExportLibrary(
                    name="TestLibrary",
                    nid=0x12345678,
                    function_nids=(0x11111111, 0x22222222),
                    variable_nids=(),
                    tls_variable_nids=(),
                ),
            ),
        )
        manifest = load_manifest_data(
            {
                "schema": 1,
                "minimum_version": "3.600",
                "modules": {
                    "test": {
                        "expected_name": "TestModule",
                        "file_names": ["module.elf"],
                        "allowed_export_signatures": [export_signature(parsed)],
                        "allowed_module_nids": ["0xAABBCCDD"],
                        "required_strings": ["TestLibrary"],
                        "libraries": [
                            {
                                "name": "TestLibrary",
                                "nid": "0x12345678",
                                "function_nids": ["0x11111111", "0x22222222"],
                            }
                        ],
                    }
                },
            }
        )

        record = audit_image(
            Path("external/official/3.60/fs_dec/module.elf"),
            image,
            manifest,
        )

        self.assertEqual(record.module_key, "test")
        self.assertEqual(record.family, "retail")
        self.assertEqual(record.version, FirmwareVersion(3, 600))
        self.assertEqual(record.issues, ())

    def test_discovers_only_manifest_images_at_or_above_minimum(self) -> None:
        manifest = load_manifest_data(_manifest(export_signature(_module())))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            supported = root / "external/official/3.60/fs_dec/example.elf"
            unsupported = root / "external/official/3.59/fs_dec/example.elf"
            unrelated = root / "external/official/3.74/fs_dec/other.elf"
            for path in (supported, unsupported, unrelated):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture")

            discovered = discover_image_paths((root,), manifest)

        self.assertEqual(discovered, (supported,))

    def test_discovery_does_not_depend_on_pathlib_rglob(self) -> None:
        manifest = load_manifest_data(_manifest(export_signature(_module())))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            supported = root / "external/devkit/3.600.011/fs_dec/example.elf"
            supported.parent.mkdir(parents=True)
            supported.write_bytes(b"fixture")

            with patch.object(Path, "rglob", return_value=iter(())):
                discovered = discover_image_paths((root,), manifest)

        self.assertEqual(discovered, (supported,))

    def test_cli_writes_deterministic_json_and_markdown(self) -> None:
        image = _build_fixture()
        parsed = ParsedModule(
            name="TestModule",
            module_nid=0xAABBCCDD,
            version=0x0101,
            info_segment=0,
            info_offset=0x80,
            exports=(
                ExportLibrary(
                    name="TestLibrary",
                    nid=0x12345678,
                    function_nids=(0x11111111, 0x22222222),
                    variable_nids=(),
                    tls_variable_nids=(),
                ),
            ),
        )
        manifest_data = {
            "schema": 1,
            "minimum_version": "3.600",
            "modules": {
                "test": {
                    "expected_name": "TestModule",
                    "file_names": ["module.elf"],
                    "allowed_export_signatures": [export_signature(parsed)],
                    "allowed_module_nids": ["0xAABBCCDD"],
                    "required_strings": ["TestLibrary"],
                    "libraries": [
                        {
                            "name": "TestLibrary",
                            "nid": "0x12345678",
                            "function_nids": ["0x11111111", "0x22222222"],
                        }
                    ],
                }
            },
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            firmware = root / "firmware/external/official/3.60/fs_dec/module.elf"
            firmware.parent.mkdir(parents=True)
            firmware.write_bytes(image)
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest_data))
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
        self.assertEqual(report["summary"], {"images": 1, "passed": 1, "failed": 0})
        self.assertEqual(
            report["images"][0]["path"],
            "firmware/external/official/3.60/fs_dec/module.elf",
        )
        self.assertTrue(markdown.endswith("\n"))
        self.assertIn("1 images: 1 passed, 0 failed", markdown)


if __name__ == "__main__":
    unittest.main()
