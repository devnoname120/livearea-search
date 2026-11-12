from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path, PurePosixPath
import sys
from typing import Mapping, Sequence


@dataclass(frozen=True)
class EvidenceRequirement:
    path: str
    needles: tuple[str, ...]


@dataclass(frozen=True)
class SourceCheck:
    identifier: str
    source: str
    source_needles: tuple[str, ...]
    source_forbidden_needles: tuple[str, ...]
    evidence: tuple[EvidenceRequirement, ...]


@dataclass(frozen=True)
class SourceContract:
    schema: int
    checks: tuple[SourceCheck, ...]


def _require_mapping(value: object, field: str) -> Mapping[str, object]:
    if not isinstance(value, Mapping):
        raise ValueError(f"{field} must be an object")
    return value


def _require_string(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} must be a nonempty string")
    return value


def _require_string_list(value: object, field: str) -> tuple[str, ...]:
    if not isinstance(value, list):
        raise ValueError(f"{field} must be an array")
    result = tuple(_require_string(item, f"{field}[]") for item in value)
    if len(set(result)) != len(result):
        raise ValueError(f"{field} contains duplicate values")
    return result


def _require_relative_path(value: object, field: str) -> str:
    raw = _require_string(value, field)
    path = PurePosixPath(raw)
    if path.is_absolute() or ".." in path.parts or "." in path.parts:
        raise ValueError(f"{field} must stay inside the project root")
    return raw


def load_contract_data(value: object) -> SourceContract:
    data = _require_mapping(value, "contract")
    if data.get("schema") != 1:
        raise ValueError("contract.schema must equal 1")
    checks_value = data.get("checks")
    if not isinstance(checks_value, list) or not checks_value:
        raise ValueError("contract.checks must be a nonempty array")
    checks = []
    identifiers = set()
    for index, raw_check in enumerate(checks_value):
        field = f"contract.checks[{index}]"
        check = _require_mapping(raw_check, field)
        identifier = _require_string(check.get("id"), f"{field}.id")
        if identifier in identifiers:
            raise ValueError(f"duplicate check id: {identifier}")
        identifiers.add(identifier)
        evidence_value = check.get("evidence", [])
        if not isinstance(evidence_value, list):
            raise ValueError(f"{field}.evidence must be an array")
        evidence = []
        for evidence_index, raw_evidence in enumerate(evidence_value):
            evidence_field = f"{field}.evidence[{evidence_index}]"
            evidence_data = _require_mapping(raw_evidence, evidence_field)
            evidence.append(
                EvidenceRequirement(
                    path=_require_relative_path(
                        evidence_data.get("path"),
                        f"{evidence_field}.path",
                    ),
                    needles=_require_string_list(
                        evidence_data.get("needles"),
                        f"{evidence_field}.needles",
                    ),
                )
            )
        checks.append(
            SourceCheck(
                identifier=identifier,
                source=_require_relative_path(check.get("source"), f"{field}.source"),
                source_needles=_require_string_list(
                    check.get("source_needles"),
                    f"{field}.source_needles",
                ),
                source_forbidden_needles=_require_string_list(
                    check.get("source_forbidden_needles", []),
                    f"{field}.source_forbidden_needles",
                ),
                evidence=tuple(evidence),
            )
        )
    return SourceContract(schema=1, checks=tuple(checks))


def audit_source_contract(root: Path, contract: SourceContract) -> dict[str, object]:
    records = []
    for check in sorted(contract.checks, key=lambda item: item.identifier):
        issues = []
        source_path = root / check.source
        try:
            source_text = source_path.read_text(encoding="utf-8")
        except OSError as error:
            issues.append(f"cannot read source {check.source!r}: {error}")
            source_text = ""
        for needle in check.source_needles:
            if needle not in source_text:
                issues.append(f"missing source needle {needle!r} in {check.source!r}")
        for needle in check.source_forbidden_needles:
            if needle in source_text:
                issues.append(
                    f"forbidden source needle {needle!r} in {check.source!r}"
                )
        for evidence in check.evidence:
            evidence_path = root / evidence.path
            try:
                evidence_text = evidence_path.read_text(encoding="utf-8")
            except OSError as error:
                issues.append(f"cannot read evidence {evidence.path!r}: {error}")
                continue
            for needle in evidence.needles:
                if needle not in evidence_text:
                    issues.append(
                        f"missing evidence needle {needle!r} in {evidence.path!r}"
                    )
        records.append(
            {
                "id": check.identifier,
                "source": check.source,
                "issues": issues,
            }
        )
    failed = sum(1 for record in records if record["issues"])
    return {
        "schema": 1,
        "summary": {
            "checks": len(records),
            "passed": len(records) - failed,
            "failed": failed,
        },
        "checks": records,
    }


def render_markdown(report: Mapping[str, object]) -> str:
    summary = _require_mapping(report.get("summary"), "report.summary")
    checks = report.get("checks")
    if not isinstance(checks, list):
        raise ValueError("report.checks must be an array")
    lines = [
        "# LiveArea Search source-to-firmware contract audit",
        "",
        f"{summary['checks']} checks: {summary['passed']} passed, {summary['failed']} failed.",
        "",
        "| Check | Source | Result |",
        "|---|---|---|",
    ]
    failures = []
    for raw in checks:
        check = _require_mapping(raw, "report.checks[]")
        issues = check.get("issues")
        if not isinstance(issues, list):
            raise ValueError("check issues must be an array")
        lines.append(
            f"| {check.get('id', '')} | `{check.get('source', '')}` | "
            f"{'PASS' if not issues else 'FAIL'} |"
        )
        if issues:
            failures.append(check)
    if failures:
        lines.extend(("", "## Failures", ""))
        for check in failures:
            lines.append(f"### {check.get('id', '')}")
            lines.append("")
            for issue in check["issues"]:
                lines.append(f"- {issue}")
            lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def load_contract_file(path: Path) -> SourceContract:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid source contract JSON: {error}") from error
    return load_contract_data(data)


def _build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--json-out", type=Path, required=True)
    parser.add_argument("--markdown-out", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _build_argument_parser().parse_args(argv)
    try:
        contract = load_contract_file(arguments.contract)
        report = audit_source_contract(arguments.root, contract)
        arguments.json_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_out.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        arguments.markdown_out.write_text(render_markdown(report), encoding="utf-8")
    except (OSError, ValueError) as error:
        print(f"source contract audit failed: {error}", file=sys.stderr)
        return 2
    summary = report["summary"]
    print(
        f"audited {summary['checks']} checks: "
        f"{summary['passed']} passed, {summary['failed']} failed"
    )
    return 1 if summary["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
