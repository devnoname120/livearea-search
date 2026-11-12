from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from source_contract_audit import audit_source_contract, load_contract_data, main


def _contract() -> dict[str, object]:
    return {
        "schema": 1,
        "checks": [
            {
                "id": "example",
                "source": "include/example.h",
                "source_needles": ["kValue = 0x12345678"],
                "source_forbidden_needles": ["kRejected = 0xDE0DA538"],
                "evidence": [
                    {
                        "path": "tools/evidence.json",
                        "needles": ["0x12345678", "TestLibrary"],
                    }
                ],
            }
        ],
    }


class SourceContractAuditTests(unittest.TestCase):
    def test_audits_source_and_evidence_needles(self) -> None:
        contract = load_contract_data(_contract())
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "include/example.h"
            evidence = root / "tools/evidence.json"
            source.parent.mkdir(parents=True)
            evidence.parent.mkdir(parents=True)
            source.write_text("static const int kValue = 0x12345678;\n")
            evidence.write_text('{"nid":"0x12345678","name":"TestLibrary"}\n')

            report = audit_source_contract(root, contract)

        self.assertEqual(report["summary"], {"checks": 1, "passed": 1, "failed": 0})
        self.assertEqual(report["checks"][0]["issues"], [])

    def test_reports_missing_source_and_evidence_needles(self) -> None:
        contract = load_contract_data(_contract())
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "include/example.h"
            evidence = root / "tools/evidence.json"
            source.parent.mkdir(parents=True)
            evidence.parent.mkdir(parents=True)
            source.write_text("unrelated\n")
            evidence.write_text("{}\n")

            report = audit_source_contract(root, contract)

        issues = report["checks"][0]["issues"]
        self.assertTrue(any("source needle" in issue for issue in issues))
        self.assertTrue(any("evidence needle" in issue for issue in issues))

    def test_reports_forbidden_source_needles(self) -> None:
        contract = load_contract_data(_contract())
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "include/example.h"
            evidence = root / "tools/evidence.json"
            source.parent.mkdir(parents=True)
            evidence.parent.mkdir(parents=True)
            source.write_text(
                "static const int kValue = 0x12345678;\n"
                "static const int kRejected = 0xDE0DA538;\n"
            )
            evidence.write_text('{"nid":"0x12345678","name":"TestLibrary"}\n')

            report = audit_source_contract(root, contract)

        issues = report["checks"][0]["issues"]
        self.assertTrue(any("forbidden source needle" in issue for issue in issues))

    def test_cli_writes_deterministic_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "include/example.h"
            evidence = root / "tools/evidence.json"
            source.parent.mkdir(parents=True)
            evidence.parent.mkdir(parents=True)
            source.write_text("static const int kValue = 0x12345678;\n")
            evidence.write_text('{"nid":"0x12345678","name":"TestLibrary"}\n')
            contract_path = root / "contract.json"
            contract_path.write_text(json.dumps(_contract()))
            json_output = root / "report.json"
            markdown_output = root / "report.md"

            result = main(
                (
                    "--root",
                    str(root),
                    "--contract",
                    str(contract_path),
                    "--json-out",
                    str(json_output),
                    "--markdown-out",
                    str(markdown_output),
                )
            )

            report = json.loads(json_output.read_text())
            markdown = markdown_output.read_text()

        self.assertEqual(result, 0)
        self.assertEqual(report["summary"], {"checks": 1, "passed": 1, "failed": 0})
        self.assertIn("1 checks: 1 passed, 0 failed", markdown)


if __name__ == "__main__":
    unittest.main()
