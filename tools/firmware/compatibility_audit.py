from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import sys
from typing import Any, Mapping, Sequence

from vita_elf import ParsedModule, parse_module_elf


@dataclass(frozen=True, order=True)
class FirmwareVersion:
    major: int
    minor: int

    def __post_init__(self) -> None:
        if self.major < 0 or self.minor < 0:
            raise ValueError("firmware version components must be nonnegative")

    def __str__(self) -> str:
        return f"{self.major}.{self.minor:03d}"


@dataclass(frozen=True)
class LibraryRequirement:
    name: str
    nid: int
    function_nids: tuple[int, ...]
    variable_nids: tuple[int, ...] = ()
    tls_variable_nids: tuple[int, ...] = ()


@dataclass(frozen=True)
class ModuleRequirement:
    expected_name: str
    file_names: tuple[str, ...]
    allowed_export_signatures: frozenset[str]
    required_strings: tuple[str, ...]
    libraries: tuple[LibraryRequirement, ...]
    allowed_module_nids: frozenset[int] = frozenset()


@dataclass(frozen=True)
class CompatibilityManifest:
    schema: int
    minimum_version: FirmwareVersion
    modules: Mapping[str, ModuleRequirement]


@dataclass(frozen=True)
class AuditRecord:
    path: str
    module_key: str
    version: FirmwareVersion
    family: str
    module_name: str
    module_nid: int
    export_signature: str
    sha256: str
    issues: tuple[str, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "path": self.path,
            "module_key": self.module_key,
            "version": str(self.version),
            "family": self.family,
            "module_name": self.module_name,
            "module_nid": f"0x{self.module_nid:08X}",
            "export_signature": self.export_signature,
            "sha256": self.sha256,
            "issues": list(self.issues),
        }


_VERSION_COMPONENT = re.compile(r"^(?P<major>\d+)\.(?P<minor>\d{2,3})(?:\.\d+)?(?:_.*)?$")
_UNDERSCORE_VERSION = re.compile(r"^(?P<major>\d+)_(?P<minor>\d{3})(?:_\d+)?(?:\..*)?$")
_INA_VERSION = re.compile(r"^(?P<major>\d+)\.(?P<minor>\d{2})_ina$")
_EMBEDDED_VERSION = re.compile(r"(?<!\d)(?P<major>\d+)\.(?P<minor>\d{2,3})(?!\d)")


def _normalize_minor(raw: str) -> int:
    if len(raw) == 2:
        return int(raw) * 10
    return int(raw)


def extract_firmware_version(path: Path) -> FirmwareVersion:
    for part in path.parts:
        match = _INA_VERSION.match(part)
        if match is not None:
            return FirmwareVersion(int(match.group("major")), int(match.group("minor")))
        match = _VERSION_COMPONENT.match(part)
        if match is not None:
            return FirmwareVersion(
                int(match.group("major")),
                _normalize_minor(match.group("minor")),
            )
        match = _UNDERSCORE_VERSION.match(part)
        if match is not None:
            return FirmwareVersion(int(match.group("major")), int(match.group("minor")))
        match = _EMBEDDED_VERSION.search(part)
        if match is not None:
            return FirmwareVersion(
                int(match.group("major")),
                _normalize_minor(match.group("minor")),
            )
    raise ValueError(f"cannot extract firmware version from {path}")


def classify_firmware_family(path: Path) -> str:
    normalized = "/".join(path.parts).lower()
    if "vitadump" in normalized:
        return "retail-reference"
    if "/external/official/" in f"/{normalized}":
        return "retail"
    if "/external/testkit/" in f"/{normalized}":
        return "testkit"
    if "/external/devkit/" in f"/{normalized}":
        return "devkit"
    if "4.00_ina" in normalized:
        return "internal-cex"
    if "/sce/external/" in f"/{normalized}":
        if ".dex." in normalized:
            return "external-dex"
        if ".cex." in normalized:
            return "external-cex"
        return "external"
    if "/sce/internal" in f"/{normalized}":
        if ".devtool." in normalized or ".tool." in normalized:
            return "internal-devtool"
        if ".dex." in normalized:
            return "internal-dex"
        if ".cex" in normalized:
            return "internal-cex"
        return "internal"
    raise ValueError(f"cannot classify firmware family for {path}")


def export_signature(module: ParsedModule) -> str:
    normalized = []
    for library in module.exports:
        normalized.append(
            {
                "name": library.name,
                "nid": library.nid,
                "function_nids": sorted(library.function_nids),
                "variable_nids": sorted(library.variable_nids),
                "tls_variable_nids": sorted(library.tls_variable_nids),
            }
        )
    normalized.sort(
        key=lambda item: (
            "" if item["name"] is None else str(item["name"]),
            int(item["nid"]),
            tuple(item["function_nids"]),
            tuple(item["variable_nids"]),
            tuple(item["tls_variable_nids"]),
        )
    )
    payload = json.dumps(normalized, sort_keys=True, separators=(",", ":")).encode("ascii")
    return hashlib.sha256(payload).hexdigest()


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


def _parse_nid_list(value: object, field: str, duplicate_label: str) -> tuple[int, ...]:
    if not isinstance(value, list):
        raise ValueError(f"{field} must be an array")
    parsed = tuple(_parse_uint32(item, f"{field}[]") for item in value)
    if len(set(parsed)) != len(parsed):
        raise ValueError(f"duplicate {duplicate_label}")
    return parsed


def _parse_library(value: object, field: str) -> LibraryRequirement:
    data = _require_mapping(value, field)
    return LibraryRequirement(
        name=_require_string(data.get("name"), f"{field}.name"),
        nid=_parse_uint32(data.get("nid"), f"{field}.nid"),
        function_nids=_parse_nid_list(
            data.get("function_nids", []),
            f"{field}.function_nids",
            "function NID",
        ),
        variable_nids=_parse_nid_list(
            data.get("variable_nids", []),
            f"{field}.variable_nids",
            "variable NID",
        ),
        tls_variable_nids=_parse_nid_list(
            data.get("tls_variable_nids", []),
            f"{field}.tls_variable_nids",
            "TLS variable NID",
        ),
    )


def _parse_module_requirement(value: object, field: str) -> ModuleRequirement:
    data = _require_mapping(value, field)
    libraries_value = data.get("libraries")
    if not isinstance(libraries_value, list):
        raise ValueError(f"{field}.libraries must be an array")
    libraries = tuple(
        _parse_library(item, f"{field}.libraries[{index}]")
        for index, item in enumerate(libraries_value)
    )
    identities = tuple((item.name, item.nid) for item in libraries)
    if len(set(identities)) != len(identities):
        raise ValueError(f"{field}.libraries contains duplicate library identities")
    signatures = _require_string_list(
        data.get("allowed_export_signatures"),
        f"{field}.allowed_export_signatures",
    )
    if not signatures:
        raise ValueError(f"{field}.allowed_export_signatures must not be empty")
    module_nids = _parse_nid_list(
        data.get("allowed_module_nids", []),
        f"{field}.allowed_module_nids",
        "module NID",
    )
    return ModuleRequirement(
        expected_name=_require_string(data.get("expected_name"), f"{field}.expected_name"),
        file_names=_require_string_list(data.get("file_names"), f"{field}.file_names"),
        allowed_export_signatures=frozenset(signatures),
        required_strings=_require_string_list(
            data.get("required_strings", []),
            f"{field}.required_strings",
        ),
        libraries=libraries,
        allowed_module_nids=frozenset(module_nids),
    )


def _parse_version(value: object, field: str) -> FirmwareVersion:
    raw = _require_string(value, field)
    match = _VERSION_COMPONENT.match(raw)
    if match is None:
        raise ValueError(f"{field} is not a supported firmware version")
    return FirmwareVersion(
        int(match.group("major")),
        _normalize_minor(match.group("minor")),
    )


def load_manifest_data(value: object) -> CompatibilityManifest:
    data = _require_mapping(value, "manifest")
    schema = data.get("schema")
    if schema != 1:
        raise ValueError("manifest.schema must equal 1")
    modules_data = _require_mapping(data.get("modules"), "manifest.modules")
    modules: dict[str, ModuleRequirement] = {}
    for key, item in modules_data.items():
        module_key = _require_string(key, "manifest.modules key")
        modules[module_key] = _parse_module_requirement(
            item,
            f"manifest.modules.{module_key}",
        )
    if not modules:
        raise ValueError("manifest.modules must not be empty")
    return CompatibilityManifest(
        schema=1,
        minimum_version=_parse_version(data.get("minimum_version"), "manifest.minimum_version"),
        modules=modules,
    )


def _format_nid(value: int) -> str:
    return f"0x{value:08X}"


def audit_parsed_module(
    module: ParsedModule,
    image: bytes,
    requirement: ModuleRequirement,
) -> tuple[str, ...]:
    issues: list[str] = []
    if module.name != requirement.expected_name:
        issues.append(
            f"module name {module.name!r} != expected {requirement.expected_name!r}"
        )
    if requirement.allowed_module_nids and module.module_nid not in requirement.allowed_module_nids:
        issues.append(f"module NID {_format_nid(module.module_nid)} is not allowed")
    signature = export_signature(module)
    if signature not in requirement.allowed_export_signatures:
        issues.append(f"export signature {signature} is not allowed")

    by_identity = {(library.name, library.nid): library for library in module.exports}
    for library_requirement in requirement.libraries:
        identity = (library_requirement.name, library_requirement.nid)
        library = by_identity.get(identity)
        if library is None:
            issues.append(
                f"missing library {library_requirement.name!r} "
                f"with NID {_format_nid(library_requirement.nid)}"
            )
            continue
        for label, required, available in (
            ("function", library_requirement.function_nids, library.function_nids),
            ("variable", library_requirement.variable_nids, library.variable_nids),
            (
                "TLS variable",
                library_requirement.tls_variable_nids,
                library.tls_variable_nids,
            ),
        ):
            available_set = set(available)
            for nid in required:
                if nid not in available_set:
                    issues.append(
                        f"missing {label} NID {_format_nid(nid)} "
                        f"from library {library_requirement.name!r}"
                    )

    for marker in requirement.required_strings:
        try:
            encoded = marker.encode("ascii")
        except UnicodeEncodeError:
            issues.append(f"required string {marker!r} is not ASCII")
            continue
        if encoded not in image:
            issues.append(f"missing required string {marker!r}")
    return tuple(issues)


def _match_requirement(
    path: Path,
    manifest: CompatibilityManifest,
) -> tuple[str, ModuleRequirement]:
    matches = [
        (key, requirement)
        for key, requirement in manifest.modules.items()
        if path.name in requirement.file_names
    ]
    if not matches:
        raise ValueError(f"no manifest module matches {path.name!r}")
    if len(matches) != 1:
        keys = ", ".join(sorted(key for key, _ in matches))
        raise ValueError(f"multiple manifest modules match {path.name!r}: {keys}")
    return matches[0]


def audit_image(
    path: Path,
    image: bytes,
    manifest: CompatibilityManifest,
    *,
    display_path: str | None = None,
) -> AuditRecord:
    module_key, requirement = _match_requirement(path, manifest)
    version = extract_firmware_version(path)
    if version < manifest.minimum_version:
        raise ValueError(
            f"firmware {version} is below minimum {manifest.minimum_version}"
        )
    family = classify_firmware_family(path)
    module = parse_module_elf(image, expected_name=requirement.expected_name)
    return AuditRecord(
        path=str(path) if display_path is None else display_path,
        module_key=module_key,
        version=version,
        family=family,
        module_name=module.name,
        module_nid=module.module_nid,
        export_signature=export_signature(module),
        sha256=hashlib.sha256(image).hexdigest(),
        issues=audit_parsed_module(module, image, requirement),
    )


def discover_image_paths(
    roots: Sequence[Path],
    manifest: CompatibilityManifest,
) -> tuple[Path, ...]:
    file_names = {
        name
        for requirement in manifest.modules.values()
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


def build_report(records: Sequence[AuditRecord]) -> dict[str, object]:
    ordered = sorted(
        records,
        key=lambda item: (
            item.path,
            item.module_key,
            item.family,
            item.version,
            item.module_name,
        ),
    )
    failed = sum(1 for item in ordered if item.issues)
    return {
        "schema": 1,
        "summary": {
            "images": len(ordered),
            "passed": len(ordered) - failed,
            "failed": failed,
        },
        "images": [item.to_dict() for item in ordered],
    }


def render_markdown(report: Mapping[str, Any]) -> str:
    summary = _require_mapping(report.get("summary"), "report.summary")
    images = report.get("images")
    if not isinstance(images, list):
        raise ValueError("report.images must be an array")
    image_count = int(summary.get("images", 0))
    passed = int(summary.get("passed", 0))
    failed = int(summary.get("failed", 0))
    lines = [
        "# LiveArea Search firmware compatibility audit",
        "",
        f"{image_count} images: {passed} passed, {failed} failed.",
        "",
        "| Firmware | Family | Module | Result | Image |",
        "|---|---|---|---|---|",
    ]
    failures: list[Mapping[str, Any]] = []
    for raw in images:
        item = _require_mapping(raw, "report.images[]")
        issues = item.get("issues")
        if not isinstance(issues, list):
            raise ValueError("report image issues must be an array")
        result = "PASS" if not issues else "FAIL"
        lines.append(
            "| {version} | {family} | {module_key} | {result} | `{path}` |".format(
                version=item.get("version", ""),
                family=item.get("family", ""),
                module_key=item.get("module_key", ""),
                result=result,
                path=item.get("path", ""),
            )
        )
        if issues:
            failures.append(item)
    if failures:
        lines.extend(("", "## Failures", ""))
        for item in failures:
            lines.append(f"### `{item.get('path', '')}`")
            lines.append("")
            for issue in item["issues"]:
                lines.append(f"- {issue}")
            lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def load_manifest_file(path: Path) -> CompatibilityManifest:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid manifest JSON: {error}") from error
    return load_manifest_data(data)


def audit_roots(
    roots: Sequence[Path],
    manifest: CompatibilityManifest,
) -> tuple[AuditRecord, ...]:
    records = []
    ordered_roots = tuple(sorted(roots, key=lambda root: len(root.parts), reverse=True))
    for path in discover_image_paths(roots, manifest):
        display_path = None
        for root in ordered_roots:
            try:
                relative = path.relative_to(root)
            except ValueError:
                continue
            display_path = str(Path(root.name) / relative)
            break
        if display_path is None:
            raise ValueError(f"discovered image is outside every firmware root: {path}")
        records.append(
            audit_image(
                path,
                path.read_bytes(),
                manifest,
                display_path=display_path,
            )
        )
    return tuple(records)


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
        manifest = load_manifest_file(arguments.manifest)
        records = audit_roots(arguments.root, manifest)
        report = build_report(records)
        arguments.json_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_out.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        arguments.markdown_out.write_text(render_markdown(report), encoding="utf-8")
    except (OSError, ValueError) as error:
        print(f"compatibility audit failed: {error}", file=sys.stderr)
        return 2
    summary = report["summary"]
    print(
        f"audited {summary['images']} images: "
        f"{summary['passed']} passed, {summary['failed']} failed"
    )
    return 1 if summary["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
