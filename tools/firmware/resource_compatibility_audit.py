from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
from typing import Any, Mapping, Sequence

from compatibility_audit import (
    FirmwareVersion,
    classify_firmware_family,
    extract_firmware_version,
)


@dataclass(frozen=True)
class ResourceRequirement:
    file_names: tuple[str, ...]
    required_u32_le: Mapping[str, int]
    exact_count: int


@dataclass(frozen=True)
class ResourceManifest:
    schema: int
    minimum_version: FirmwareVersion
    resources: Mapping[str, ResourceRequirement]


@dataclass(frozen=True)
class ResourceRecord:
    path: str
    resource_key: str
    version: FirmwareVersion
    family: str
    sha256: str
    issues: tuple[str, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "path": self.path,
            "resource_key": self.resource_key,
            "version": str(self.version),
            "family": self.family,
            "sha256": self.sha256,
            "issues": list(self.issues),
        }


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


def _parse_uint32(value: object, field: str) -> int:
    if isinstance(value, int) and not isinstance(value, bool):
        parsed = value
    elif isinstance(value, str):
        try:
            parsed = int(value, 0)
        except ValueError as error:
            raise ValueError(f"{field} is not an integer") from error
    else:
        raise ValueError(f"{field} must be an integer")
    if parsed < 0 or parsed > 0xFFFFFFFF:
        raise ValueError(f"{field} is outside uint32 range")
    return parsed


def _parse_version(value: object, field: str) -> FirmwareVersion:
    raw = _require_string(value, field)
    parts = raw.split(".")
    if len(parts) != 2 or not all(part.isdigit() for part in parts):
        raise ValueError(f"{field} is not a supported firmware version")
    minor = int(parts[1]) * 10 if len(parts[1]) == 2 else int(parts[1])
    return FirmwareVersion(int(parts[0]), minor)


def load_resource_manifest_data(value: object) -> ResourceManifest:
    data = _require_mapping(value, "manifest")
    if data.get("schema") != 1:
        raise ValueError("manifest.schema must equal 1")
    resources_data = _require_mapping(data.get("resources"), "manifest.resources")
    resources: dict[str, ResourceRequirement] = {}
    for key, raw_requirement in resources_data.items():
        resource_key = _require_string(key, "manifest.resources key")
        requirement = _require_mapping(
            raw_requirement,
            f"manifest.resources.{resource_key}",
        )
        raw_values = _require_mapping(
            requirement.get("required_u32_le"),
            f"manifest.resources.{resource_key}.required_u32_le",
        )
        values = {
            _require_string(label, "required_u32_le key"): _parse_uint32(
                raw_value,
                f"manifest.resources.{resource_key}.required_u32_le.{label}",
            )
            for label, raw_value in raw_values.items()
        }
        exact_count = requirement.get("exact_count")
        if not isinstance(exact_count, int) or isinstance(exact_count, bool) or exact_count < 1:
            raise ValueError(
                f"manifest.resources.{resource_key}.exact_count must be a positive integer"
            )
        resources[resource_key] = ResourceRequirement(
            file_names=_require_string_list(
                requirement.get("file_names"),
                f"manifest.resources.{resource_key}.file_names",
            ),
            required_u32_le=values,
            exact_count=exact_count,
        )
    if not resources:
        raise ValueError("manifest.resources must not be empty")
    return ResourceManifest(
        schema=1,
        minimum_version=_parse_version(
            data.get("minimum_version"),
            "manifest.minimum_version",
        ),
        resources=resources,
    )


def audit_resource_bytes(
    image: bytes,
    requirement: ResourceRequirement,
) -> tuple[str, ...]:
    issues = []
    for label, value in sorted(requirement.required_u32_le.items()):
        count = image.count(struct.pack("<I", value))
        if count != requirement.exact_count:
            issues.append(
                f"identifier {label!r} 0x{value:08X}: "
                f"expected {requirement.exact_count}, found {count}"
            )
    return tuple(issues)


def _match_requirement(
    path: Path,
    manifest: ResourceManifest,
) -> tuple[str, ResourceRequirement]:
    matches = [
        (key, requirement)
        for key, requirement in manifest.resources.items()
        if path.name in requirement.file_names
    ]
    if len(matches) != 1:
        raise ValueError(f"expected one resource requirement for {path.name!r}")
    return matches[0]


def discover_resource_paths(
    roots: Sequence[Path],
    manifest: ResourceManifest,
) -> tuple[Path, ...]:
    file_names = {
        name
        for requirement in manifest.resources.values()
        for name in requirement.file_names
    }
    discovered: set[Path] = set()
    for root in roots:
        if not root.is_dir():
            raise ValueError(f"firmware root is not a directory: {root}")

        def raise_walk_error(error: OSError) -> None:
            raise error

        for directory, directories, files in os.walk(root, onerror=raise_walk_error):
            directories.sort()
            files.sort()
            for file_name in files:
                if file_name not in file_names:
                    continue
                path = Path(directory) / file_name
                version = extract_firmware_version(path)
                if version >= manifest.minimum_version:
                    discovered.add(path)
    return tuple(sorted(discovered, key=lambda path: str(path)))


def audit_roots(
    roots: Sequence[Path],
    manifest: ResourceManifest,
) -> tuple[ResourceRecord, ...]:
    ordered_roots = tuple(sorted(roots, key=lambda root: len(root.parts), reverse=True))
    records = []
    for path in discover_resource_paths(roots, manifest):
        resource_key, requirement = _match_requirement(path, manifest)
        image = path.read_bytes()
        display_path = None
        for root in ordered_roots:
            try:
                relative = path.relative_to(root)
            except ValueError:
                continue
            display_path = str(Path(root.name) / relative)
            break
        if display_path is None:
            raise ValueError(f"resource is outside every firmware root: {path}")
        records.append(
            ResourceRecord(
                path=display_path,
                resource_key=resource_key,
                version=extract_firmware_version(path),
                family=classify_firmware_family(path),
                sha256=hashlib.sha256(image).hexdigest(),
                issues=audit_resource_bytes(image, requirement),
            )
        )
    return tuple(records)


def build_report(records: Sequence[ResourceRecord]) -> dict[str, object]:
    ordered = sorted(records, key=lambda item: item.path)
    failed = sum(1 for item in ordered if item.issues)
    return {
        "schema": 1,
        "summary": {
            "resources": len(ordered),
            "passed": len(ordered) - failed,
            "failed": failed,
        },
        "resources": [item.to_dict() for item in ordered],
    }


def render_markdown(report: Mapping[str, Any]) -> str:
    summary = _require_mapping(report.get("summary"), "report.summary")
    resources = report.get("resources")
    if not isinstance(resources, list):
        raise ValueError("report.resources must be an array")
    lines = [
        "# LiveArea Search resource compatibility audit",
        "",
        f"{summary['resources']} resources: {summary['passed']} passed, {summary['failed']} failed.",
        "",
        "| Firmware | Family | Resource | Result | Image |",
        "|---|---|---|---|---|",
    ]
    for raw in resources:
        item = _require_mapping(raw, "report.resources[]")
        issues = item.get("issues")
        if not isinstance(issues, list):
            raise ValueError("resource issues must be an array")
        lines.append(
            "| {version} | {family} | {resource_key} | {result} | `{path}` |".format(
                version=item.get("version", ""),
                family=item.get("family", ""),
                resource_key=item.get("resource_key", ""),
                result="PASS" if not issues else "FAIL",
                path=item.get("path", ""),
            )
        )
    return "\n".join(lines).rstrip() + "\n"


def load_resource_manifest_file(path: Path) -> ResourceManifest:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid resource manifest JSON: {error}") from error
    return load_resource_manifest_data(data)


def _build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--root", type=Path, action="append", required=True)
    parser.add_argument("--json-out", type=Path, required=True)
    parser.add_argument("--markdown-out", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _build_argument_parser().parse_args(argv)
    try:
        manifest = load_resource_manifest_file(arguments.manifest)
        report = build_report(audit_roots(arguments.root, manifest))
        arguments.json_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_out.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        arguments.markdown_out.write_text(render_markdown(report), encoding="utf-8")
    except (OSError, ValueError) as error:
        print(f"resource compatibility audit failed: {error}", file=sys.stderr)
        return 2
    summary = report["summary"]
    print(
        f"audited {summary['resources']} resources: "
        f"{summary['passed']} passed, {summary['failed']} failed"
    )
    return 1 if summary["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
