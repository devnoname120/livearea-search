from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import sys
from typing import Mapping
from typing import Sequence

from vita_elf import parse_module_elf


@dataclass(frozen=True)
class ImportRequirement:
    name: str
    nid: int
    function_nids: tuple[int, ...]


@dataclass(frozen=True)
class BinaryProfile:
    expected_module_name: str
    required_imports: tuple[ImportRequirement, ...]
    forbidden_imports: tuple[ImportRequirement, ...]
    required_strings: tuple[str, ...]
    forbidden_strings: tuple[str, ...]


@dataclass(frozen=True)
class BinaryContract:
    schema: int
    profiles: Mapping[str, BinaryProfile]


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
    values = tuple(_require_string(item, f"{field}[]") for item in value)
    if len(set(values)) != len(values):
        raise ValueError(f"{field} contains duplicate values")
    return values


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


def _parse_function_nids(value: object, field: str) -> tuple[int, ...]:
    if not isinstance(value, list):
        raise ValueError(f"{field} must be an array")
    nids = tuple(_parse_uint32(item, f"{field}[]") for item in value)
    if len(set(nids)) != len(nids):
        raise ValueError("duplicate function NID")
    return nids


def _parse_import(value: object, field: str) -> ImportRequirement:
    data = _require_mapping(value, field)
    return ImportRequirement(
        name=_require_string(data.get("name"), f"{field}.name"),
        nid=_parse_uint32(data.get("nid"), f"{field}.nid"),
        function_nids=_parse_function_nids(
            data.get("function_nids", []),
            f"{field}.function_nids",
        ),
    )


def _parse_import_list(value: object, field: str) -> tuple[ImportRequirement, ...]:
    if not isinstance(value, list):
        raise ValueError(f"{field} must be an array")
    imports = tuple(
        _parse_import(item, f"{field}[{index}]")
        for index, item in enumerate(value)
    )
    identities = tuple((item.name, item.nid) for item in imports)
    if len(set(identities)) != len(identities):
        raise ValueError(f"{field} contains duplicate import libraries")
    return imports


def _parse_profile(value: object, field: str) -> BinaryProfile:
    data = _require_mapping(value, field)
    return BinaryProfile(
        expected_module_name=_require_string(
            data.get("expected_module_name"),
            f"{field}.expected_module_name",
        ),
        required_imports=_parse_import_list(
            data.get("required_imports", []),
            f"{field}.required_imports",
        ),
        forbidden_imports=_parse_import_list(
            data.get("forbidden_imports", []),
            f"{field}.forbidden_imports",
        ),
        required_strings=_require_string_list(
            data.get("required_strings", []),
            f"{field}.required_strings",
        ),
        forbidden_strings=_require_string_list(
            data.get("forbidden_strings", []),
            f"{field}.forbidden_strings",
        ),
    )


def load_contract_data(value: object) -> BinaryContract:
    data = _require_mapping(value, "contract")
    if data.get("schema") != 1:
        raise ValueError("contract.schema must equal 1")
    profiles_data = _require_mapping(data.get("profiles"), "contract.profiles")
    profiles: dict[str, BinaryProfile] = {}
    for key, profile_value in profiles_data.items():
        profile_name = _require_string(key, "contract.profiles key")
        profiles[profile_name] = _parse_profile(
            profile_value,
            f"contract.profiles.{profile_name}",
        )
    if not profiles:
        raise ValueError("contract.profiles must not be empty")
    return BinaryContract(schema=1, profiles=profiles)


def _format_nid(value: int) -> str:
    return f"0x{value:08X}"


def audit_binary(image: bytes, profile: BinaryProfile) -> tuple[str, ...]:
    module = parse_module_elf(image)
    issues: list[str] = []
    if module.name != profile.expected_module_name:
        issues.append(
            f"module name {module.name!r} != expected {profile.expected_module_name!r}"
        )
    imports = {(library.name, library.nid): library for library in module.imports}
    for requirement in profile.required_imports:
        identity = (requirement.name, requirement.nid)
        library = imports.get(identity)
        if library is None:
            issues.append(
                f"missing required import library {requirement.name!r} "
                f"with NID {_format_nid(requirement.nid)}"
            )
            continue
        available = set(library.function_nids)
        for nid in requirement.function_nids:
            if nid not in available:
                issues.append(
                    f"missing required import function NID {_format_nid(nid)} "
                    f"from library {requirement.name!r}"
                )
    for requirement in profile.forbidden_imports:
        library = imports.get((requirement.name, requirement.nid))
        if library is None:
            continue
        available = set(library.function_nids)
        for nid in requirement.function_nids:
            if nid in available:
                issues.append(
                    f"forbidden import function NID {_format_nid(nid)} "
                    f"from library {requirement.name!r}"
                )
    for marker in profile.required_strings:
        encoded = marker.encode("ascii")
        if encoded not in image:
            issues.append(f"missing required string {marker!r}")
    for marker in profile.forbidden_strings:
        encoded = marker.encode("ascii")
        if encoded in image:
            issues.append(f"forbidden string {marker!r}")
    return tuple(issues)


def load_contract_file(path: Path) -> BinaryContract:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid binary contract JSON: {error}") from error
    return load_contract_data(data)


def build_report(
    image: bytes,
    profile_name: str,
    profile: BinaryProfile,
) -> dict[str, object]:
    module = parse_module_elf(image)
    issues = audit_binary(image, profile)
    imports = [
        {
            "name": library.name,
            "nid": _format_nid(library.nid),
            "function_nids": [_format_nid(nid) for nid in library.function_nids],
            "variable_nids": [_format_nid(nid) for nid in library.variable_nids],
            "tls_variable_nids": [
                _format_nid(nid) for nid in library.tls_variable_nids
            ],
        }
        for library in sorted(module.imports, key=lambda item: (item.name, item.nid))
    ]
    return {
        "schema": 1,
        "profile": profile_name,
        "passed": not issues,
        "sha256": hashlib.sha256(image).hexdigest(),
        "module_name": module.name,
        "module_nid": _format_nid(module.module_nid),
        "imports": imports,
        "issues": list(issues),
    }


def _build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--json-out", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _build_argument_parser().parse_args(argv)
    try:
        contract = load_contract_file(arguments.contract)
        try:
            profile = contract.profiles[arguments.profile]
        except KeyError as error:
            raise ValueError(f"unknown binary profile: {arguments.profile}") from error
        image = arguments.binary.read_bytes()
        report = build_report(image, arguments.profile, profile)
        arguments.json_out.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_out.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    except (OSError, ValueError) as error:
        print(f"release binary audit failed: {error}", file=sys.stderr)
        return 2
    print(
        f"audited {arguments.binary}: "
        f"{'passed' if report['passed'] else 'failed'}"
    )
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
