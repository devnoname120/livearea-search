from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from release_binary_audit import audit_binary, load_contract_data, main
from test_vita_elf import _build_fixture


def _contract() -> dict[str, object]:
    return {
        "schema": 1,
        "profiles": {
            "release": {
                "expected_module_name": "TestModule",
                "required_imports": [
                    {
                        "name": "TestImport",
                        "nid": "0x87654321",
                        "function_nids": ["0x33333333", "0x44444444"],
                    }
                ],
                "forbidden_imports": [
                    {
                        "name": "TestImport",
                        "nid": "0x87654321",
                        "function_nids": ["0xDEADBEEF"],
                    }
                ],
                "required_strings": ["required-marker"],
                "forbidden_strings": ["forbidden-marker"],
            }
        },
    }


class ReleaseBinaryAuditTests(unittest.TestCase):
    def test_release_profile_accepts_required_imports_and_strings(self) -> None:
        contract = load_contract_data(_contract()).profiles["release"]
        image = _build_fixture(import_format="short") + b"required-marker\0"

        self.assertEqual(audit_binary(image, contract), ())

    def test_release_profile_reports_import_and_string_failures(self) -> None:
        data = _contract()
        profile = data["profiles"]["release"]
        profile["required_imports"][0]["function_nids"].append("0x99999999")
        profile["forbidden_imports"][0]["function_nids"] = ["0x33333333"]
        contract = load_contract_data(data).profiles["release"]
        image = _build_fixture(import_format="short") + b"forbidden-marker\0"

        issues = audit_binary(image, contract)

        self.assertTrue(any("missing required import function NID 0x99999999" in issue for issue in issues))
        self.assertTrue(any("forbidden import function NID 0x33333333" in issue for issue in issues))
        self.assertTrue(any("missing required string 'required-marker'" in issue for issue in issues))
        self.assertTrue(any("forbidden string 'forbidden-marker'" in issue for issue in issues))

    def test_contract_rejects_duplicate_import_nids(self) -> None:
        data = _contract()
        functions = data["profiles"]["release"]["required_imports"][0]["function_nids"]
        functions.append("0x33333333")

        with self.assertRaisesRegex(ValueError, "duplicate function NID"):
            load_contract_data(data)

    def test_cli_writes_binary_evidence(self) -> None:
        image = _build_fixture(import_format="short") + b"required-marker\0"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            contract_path = root / "contract.json"
            binary_path = root / "module.velf"
            report_path = root / "report.json"
            contract_path.write_text(json.dumps(_contract()))
            binary_path.write_bytes(image)

            result = main(
                (
                    "--contract",
                    str(contract_path),
                    "--profile",
                    "release",
                    "--binary",
                    str(binary_path),
                    "--json-out",
                    str(report_path),
                )
            )

            report = json.loads(report_path.read_text())

        self.assertEqual(result, 0)
        self.assertTrue(report["passed"])
        self.assertEqual(report["profile"], "release")
        self.assertEqual(report["module_name"], "TestModule")
        self.assertEqual(report["issues"], [])
        self.assertEqual(report["imports"][0]["name"], "TestImport")


if __name__ == "__main__":
    unittest.main()
