from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import shlex
import struct
import subprocess
import tarfile
import tempfile
import time
from typing import Iterable


REQUEST_MAGIC = 0x31464242
RESULT_MAGIC = 0x31524242
FORMAT_VERSION = 1

OP_READ = 1
OP_REPLACE = 2
OP_INSTALL = 4
OP_ENSURE_DIR = 5
OP_READ_OPTIONAL = 6

CONFIG_PATH = "ur0:/tai/config.txt"
SUPRX_PATH = "ur0:/tai/livearea_search/livearea_search.suprx"
RCO_PATH = "ur0:/tai/livearea_search/livearea_search_plugin.rco"
STARTUP_LOG_PATH = "ur0:/data/livearea_search/startup.log"
DATA_DIRECTORY = "ur0:/data/livearea_search"
INSTALL_DIRECTORY = "ur0:/tai/livearea_search"

ACTIVE_PATHS = (
    b"ur0:tai/livearea_search/livearea_search.suprx",
    b"ur0:/tai/livearea_search/livearea_search.suprx",
)
DISABLED_PATHS = (
    b"#r0:tai/livearea_search/livearea_search.suprx",
    b"#r0:/tai/livearea_search/livearea_search.suprx",
)

REMOTE_TOOLING = "/home/dietpi/vita-devkit/psp2-tooling"
REMOTE_RUNS = "/home/dietpi/vita-devkit/runs"
OWNER_BASENAMES = {
    "session.sh",
    "deci4p_host.py",
    "devkit_console.py",
    "vcp_capture.py",
    "psp2_input.py",
}


@dataclass(frozen=True)
class Entry:
    operation: int
    path: str
    limit: int = 0
    new_size: int = 0
    old_size: int = 0


@dataclass(frozen=True)
class ResultEntry:
    status: int
    size: int


class DevkitError(RuntimeError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def utc_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def pack_request(entries: list[Entry], job: int) -> bytes:
    if not entries or len(entries) > 72:
        raise DevkitError("request entry count is outside the bridge bounds")
    if job <= 0 or job > 0xFFFFFFFF:
        raise DevkitError("job must be a nonzero uint32")
    output = bytearray(struct.pack("<4I", REQUEST_MAGIC, FORMAT_VERSION, len(entries), job))
    for entry in entries:
        for value in (entry.operation, entry.limit, entry.new_size, entry.old_size):
            if value < 0 or value > 0xFFFFFFFF:
                raise DevkitError("bridge entry fields must be uint32 values")
        encoded = entry.path.encode("ascii")
        if not encoded or len(encoded) >= 256 or b"\0" in encoded:
            raise DevkitError(f"invalid bridge path: {entry.path!r}")
        output.extend(
            struct.pack(
                "<4I256s",
                entry.operation,
                entry.limit,
                entry.new_size,
                entry.old_size,
                encoded,
            )
        )
    return bytes(output)


def parse_result(data: bytes, expected_count: int, expected_job: int) -> list[ResultEntry]:
    expected_size = 16 + expected_count * 8
    if len(data) != expected_size:
        raise DevkitError(f"bridge result size {len(data)} != {expected_size}")
    magic, version, count, job = struct.unpack_from("<4I", data)
    if (magic, version, count, job) != (
        RESULT_MAGIC,
        FORMAT_VERSION,
        expected_count,
        expected_job,
    ):
        raise DevkitError("bridge result header mismatch")
    return [
        ResultEntry(*struct.unpack_from("<iI", data, 16 + index * 8))
        for index in range(expected_count)
    ]


def _line_value(line: bytes) -> bytes:
    return line.rstrip(b"\r\n").rstrip(b" \t")


def _find_config_entries(config: bytes) -> tuple[list[int], list[int]]:
    active: list[int] = []
    disabled: list[int] = []
    offset = 0
    for line in config.splitlines(keepends=True):
        value = _line_value(line)
        if value in ACTIVE_PATHS:
            active.append(offset)
        elif value in DISABLED_PATHS:
            disabled.append(offset)
        elif value.lstrip(b" \t") in ACTIVE_PATHS + DISABLED_PATHS:
            raise DevkitError("indented LiveArea Search config entries are unsupported")
        offset += len(line)
    return active, disabled


def _replace_first_byte(config: bytes, offset: int, replacement: bytes) -> bytes:
    return config[:offset] + replacement + config[offset + 1 :]


def config_variants(config: bytes) -> tuple[bytes, bytes]:
    if not config or len(config) > 1024 * 1024 or b"\0" in config:
        raise DevkitError("config.txt is empty, oversized, or contains NUL")
    active, disabled = _find_config_entries(config)
    if len(active) + len(disabled) > 1:
        raise DevkitError("config.txt contains multiple LiveArea Search entries")
    if active:
        active_config = config
        disabled_config = _replace_first_byte(config, active[0], b"#")
    elif disabled:
        disabled_config = config
        active_config = _replace_first_byte(config, disabled[0], b"u")
    else:
        lines = config.splitlines(keepends=True)
        main_indices = [
            index
            for index, line in enumerate(lines)
            if _line_value(line) == b"*main"
        ]
        if len(main_indices) != 1:
            raise DevkitError("config.txt must contain exactly one *main section")
        newline = b"\r\n" if b"\r\n" in config else b"\n"
        main_index = main_indices[0]
        if not lines[main_index].endswith((b"\n", b"\r")):
            lines[main_index] += newline
        lines.insert(main_index + 1, DISABLED_PATHS[0] + newline)
        disabled_config = b"".join(lines)
        _, inserted = _find_config_entries(disabled_config)
        if len(inserted) != 1:
            raise DevkitError("failed to insert the disabled config entry")
        active_config = _replace_first_byte(disabled_config, inserted[0], b"u")
    active_after, disabled_after = _find_config_entries(active_config)
    if len(active_after) != 1 or disabled_after:
        raise DevkitError("active config variant is invalid")
    active_after, disabled_after = _find_config_entries(disabled_config)
    if active_after or len(disabled_after) != 1:
        raise DevkitError("disabled config variant is invalid")
    return disabled_config, active_config


def remote_owner_probe_script() -> str:
    names = json.dumps(sorted(OWNER_BASENAMES))
    return f"""python3 - <<'PY'
import json
from pathlib import Path
names=set({names})
owners=[]
for entry in Path('/proc').iterdir():
    if not entry.name.isdigit():
        continue
    try:
        raw=(entry/'cmdline').read_bytes()
    except OSError:
        continue
    argv=[part.decode(errors='replace') for part in raw.split(b'\\0') if part]
    if any(Path(arg).name in names for arg in argv):
        owners.append({{'pid':int(entry.name),'argv':argv}})
print(json.dumps(sorted(owners,key=lambda item:item['pid'])))
PY"""


class Remote:
    def __init__(self, host: str) -> None:
        self.host = host

    def run(
        self,
        command: str,
        *,
        input_data: bytes | None = None,
        timeout: int = 30,
        check: bool = True,
        stdout=None,
    ) -> subprocess.CompletedProcess[bytes]:
        result = subprocess.run(
            [
                "ssh",
                "-o",
                "BatchMode=yes",
                "-o",
                "ConnectTimeout=10",
                self.host,
                command,
            ],
            input=input_data,
            stdout=stdout if stdout is not None else subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
        if check and result.returncode != 0:
            raise DevkitError(
                f"remote command failed ({result.returncode}): "
                f"{result.stderr.decode(errors='replace').strip()}"
            )
        return result

    def owners(self) -> list[dict]:
        result = self.run(remote_owner_probe_script())
        return json.loads(result.stdout.decode())

    def require_unowned(self) -> None:
        owners = self.owners()
        if owners:
            rendered = "\n".join(
                f"  {owner['pid']}: {' '.join(owner['argv'])}" for owner in owners
            )
            raise DevkitError(f"devkit tooling is already owned:\n{rendered}")

    def upload_tree(self, source: Path, remote_root: str) -> None:
        with tempfile.NamedTemporaryFile(suffix=".tar") as archive:
            with tarfile.open(fileobj=archive, mode="w") as tar:
                tar.add(source, arcname="host0")
            archive.flush()
            archive.seek(0)
            command = (
                f"mkdir -p {shlex.quote(remote_root + '/out')} && "
                f"tar -C {shlex.quote(remote_root)} -xf -"
            )
            self.run(command, input_data=archive.read(), timeout=60)

    def download_evidence(self, remote_root: str, destination: Path) -> None:
        destination.mkdir(parents=True, exist_ok=False)
        archive_path = destination / "remote-evidence.tar"
        command = (
            f"tar -C {shlex.quote(remote_root)} -cf - host0/bridge out"
        )
        with archive_path.open("wb") as output:
            self.run(command, timeout=120, stdout=output)
        with tarfile.open(archive_path, mode="r") as archive:
            extract_archive_safely(archive, destination)

    def run_session(self, remote_root: str, duration: int) -> subprocess.CompletedProcess[bytes]:
        self.require_unowned()
        command = (
            f"cd {shlex.quote(REMOTE_TOOLING)} && "
            f"sudo -n env HOST0={shlex.quote(remote_root + '/host0')} "
            f"OUT={shlex.quote(remote_root + '/out')} "
            f"bash devkit/session.sh {duration} host0:/filebridge.bin"
        )
        return self.run(command, timeout=duration + 120, check=False)


def write_host0(
    root: Path,
    bridge: Path,
    entries: list[Entry],
    job: int,
    staged: dict[tuple[int, str], bytes] | None = None,
) -> None:
    bridge_dir = root / "bridge"
    bridge_dir.mkdir(parents=True)
    (root / "filebridge.bin").write_bytes(bridge.read_bytes())
    (bridge_dir / "request.bin").write_bytes(pack_request(entries, job))
    for (index, suffix), data in (staged or {}).items():
        (bridge_dir / f"{job:08d}-{index:03d}.{suffix}").write_bytes(data)


def extract_archive_safely(archive: tarfile.TarFile, destination: Path) -> None:
    destination = destination.resolve()
    for member in archive.getmembers():
        relative = PurePosixPath(member.name)
        if relative.is_absolute() or ".." in relative.parts:
            raise DevkitError(f"unsafe archive path: {member.name}")
        if member.issym() or member.islnk() or not (member.isdir() or member.isreg()):
            raise DevkitError(f"unsupported archive member: {member.name}")
        target = (destination / Path(*relative.parts)).resolve()
        if destination not in target.parents and target != destination:
            raise DevkitError(f"archive path escapes destination: {member.name}")
        archive.extract(member, destination)


def result_paths(evidence: Path, job: int) -> tuple[Path, Path]:
    bridge = evidence / "host0/bridge"
    return bridge, bridge / f"{job:08d}.done"


def collect_session_report(
    evidence: Path,
    entries: list[Entry],
    job: int,
    session_result: subprocess.CompletedProcess[bytes],
) -> dict:
    bridge, done = result_paths(evidence, job)
    results = parse_result(done.read_bytes(), len(entries), job) if done.exists() else []
    cores = sorted(
        str(path.relative_to(evidence))
        for path in (evidence / "out").rglob("*")
        if path.is_file() and "core" in path.name.lower()
    )
    return {
        "job": job,
        "session_returncode": session_result.returncode,
        "session_stderr": session_result.stderr.decode(errors="replace"),
        "results": [entry.__dict__ for entry in results],
        "core_files": cores,
        "bridge_outputs": sorted(path.name for path in bridge.glob("*.out")),
    }


def next_job() -> int:
    return int(time.time()) & 0xFFFFFFFF or 1


def baseline_entries() -> list[Entry]:
    return [
        Entry(OP_READ, CONFIG_PATH, limit=1024 * 1024),
        Entry(OP_READ_OPTIONAL, SUPRX_PATH, limit=2 * 1024 * 1024),
        Entry(OP_READ_OPTIONAL, RCO_PATH, limit=2 * 1024 * 1024),
        Entry(OP_READ_OPTIONAL, STARTUP_LOG_PATH, limit=4 * 1024 * 1024),
    ]


def load_output(evidence: Path, job: int, index: int) -> bytes:
    path = evidence / "host0/bridge" / f"{job:08d}-{index:03d}.out"
    if not path.exists():
        raise DevkitError(f"missing bridge output: {path}")
    return path.read_bytes()


def ensure_success(report: dict, expected_count: int) -> None:
    if report["session_returncode"] != 0:
        raise DevkitError("devkit session failed; inspect the evidence directory")
    results = report["results"]
    if len(results) != expected_count:
        raise DevkitError("bridge did not produce a complete result")
    failures = [index for index, result in enumerate(results) if result["status"] != 0]
    if failures:
        raise DevkitError(f"bridge entries failed: {failures}")


def run_bridge_session(
    remote: Remote,
    bridge: Path,
    entries: list[Entry],
    staged: dict[tuple[int, str], bytes],
    mode: str,
    duration: int,
    evidence_root: Path,
) -> tuple[Path, dict]:
    remote.require_unowned()
    stamp = utc_stamp()
    job = next_job()
    remote_root = f"{REMOTE_RUNS}/livearea-{mode}-{stamp}-{job}"
    local_evidence = evidence_root / f"devkit-{mode}-{stamp}-{job}"
    with tempfile.TemporaryDirectory(prefix="livearea-host0-") as temporary:
        host0 = Path(temporary) / "host0"
        write_host0(host0, bridge, entries, job, staged)
        remote.upload_tree(host0, remote_root)
    remote.require_unowned()
    session_result = remote.run_session(remote_root, duration)
    remote.download_evidence(remote_root, local_evidence)
    report = collect_session_report(local_evidence, entries, job, session_result)
    report.update(
        {
            "mode": mode,
            "remote_root": remote_root,
            "captured_utc": datetime.now(timezone.utc).isoformat(),
        }
    )
    (local_evidence / "report.json").write_text(json.dumps(report, indent=2))
    return local_evidence, report


def command_status(args: argparse.Namespace) -> None:
    owners = Remote(args.remote).owners()
    print(json.dumps({"remote": args.remote, "owners": owners}, indent=2))


def command_baseline(args: argparse.Namespace) -> None:
    entries = baseline_entries()
    evidence, report = run_bridge_session(
        Remote(args.remote),
        args.bridge,
        entries,
        {},
        "baseline",
        args.duration,
        args.evidence_root,
    )
    ensure_success(report, len(entries))
    if report["core_files"]:
        raise DevkitError("baseline session captured a core dump")
    job = report["job"]
    config = load_output(evidence, job, 0)
    suprx = load_output(evidence, job, 1)
    rco = load_output(evidence, job, 2)
    startup_log = load_output(evidence, job, 3)
    disabled, active = config_variants(config)
    report.update(
        {
            "config_sha256": sha256(config),
            "disabled_config_sha256": sha256(disabled),
            "active_config_sha256": sha256(active),
            "suprx_present": bool(suprx),
            "suprx_sha256": sha256(suprx) if suprx else None,
            "rco_present": bool(rco),
            "rco_sha256": sha256(rco) if rco else None,
            "startup_log_present": bool(startup_log),
            "startup_log_sha256": sha256(startup_log) if startup_log else None,
        }
    )
    (evidence / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps({"evidence": str(evidence), **report}, indent=2))


def baseline_file(baseline: Path, index: int) -> bytes:
    report = json.loads((baseline / "report.json").read_text())
    if report.get("mode") != "baseline":
        raise DevkitError("staging input is not a baseline evidence directory")
    ensure_success(report, len(baseline_entries()))
    if report.get("core_files"):
        raise DevkitError("baseline evidence contains a core dump")
    return load_output(baseline, report["job"], index)


def build_stage_plan(
    config: bytes,
    old_suprx: bytes,
    old_rco: bytes,
    new_suprx: bytes,
    new_rco: bytes,
) -> tuple[list[Entry], dict[tuple[int, str], bytes], bytes, bytes]:
    disabled_config, active_config = config_variants(config)
    entries = [
        Entry(OP_REPLACE, CONFIG_PATH, 1024 * 1024, len(disabled_config), len(config)),
        Entry(OP_ENSURE_DIR, DATA_DIRECTORY),
        Entry(OP_ENSURE_DIR, INSTALL_DIRECTORY),
        Entry(
            OP_REPLACE if old_suprx else OP_INSTALL,
            SUPRX_PATH,
            2 * 1024 * 1024,
            len(new_suprx),
            len(old_suprx),
        ),
        Entry(
            OP_REPLACE if old_rco else OP_INSTALL,
            RCO_PATH,
            2 * 1024 * 1024,
            len(new_rco),
            len(old_rco),
        ),
        Entry(
            OP_REPLACE,
            CONFIG_PATH,
            1024 * 1024,
            len(active_config),
            len(disabled_config),
        ),
    ]
    staged: dict[tuple[int, str], bytes] = {
        (0, "expected"): config,
        (0, "payload"): disabled_config,
        (3, "payload"): new_suprx,
        (4, "payload"): new_rco,
        (5, "expected"): disabled_config,
        (5, "payload"): active_config,
    }
    if old_suprx:
        staged[(3, "expected")] = old_suprx
    if old_rco:
        staged[(4, "expected")] = old_rco
    return entries, staged, disabled_config, active_config


def command_stage(args: argparse.Namespace) -> None:
    config = baseline_file(args.baseline, 0)
    old_suprx = baseline_file(args.baseline, 1)
    old_rco = baseline_file(args.baseline, 2)
    new_suprx = args.suprx.read_bytes()
    new_rco = args.rco.read_bytes()
    entries, staged, disabled_config, active_config = build_stage_plan(
        config,
        old_suprx,
        old_rco,
        new_suprx,
        new_rco,
    )

    evidence, report = run_bridge_session(
        Remote(args.remote),
        args.bridge,
        entries,
        staged,
        "stage",
        args.duration,
        args.evidence_root,
    )
    ensure_success(report, len(entries))
    if report["core_files"]:
        raise DevkitError("staging session captured a core dump")
    job = report["job"]
    expected_outputs = {
        0: disabled_config,
        3: new_suprx,
        4: new_rco,
        5: active_config,
    }
    for index, expected in expected_outputs.items():
        actual = load_output(evidence, job, index)
        if actual != expected:
            raise DevkitError(f"bridge evidence mismatch for entry {index}")
    report.update(
        {
            "candidate_suprx_sha256": sha256(new_suprx),
            "candidate_rco_sha256": sha256(new_rco),
            "active_config_sha256": sha256(active_config),
        }
    )
    (evidence / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps({"evidence": str(evidence), **report}, indent=2))


def command_validate(args: argparse.Namespace) -> None:
    entries = baseline_entries()
    evidence, report = run_bridge_session(
        Remote(args.remote),
        args.bridge,
        entries,
        {},
        "validate",
        args.duration,
        args.evidence_root,
    )
    ensure_success(report, len(entries))
    job = report["job"]
    config = load_output(evidence, job, 0)
    suprx = load_output(evidence, job, 1)
    rco = load_output(evidence, job, 2)
    startup_log = load_output(evidence, job, 3)
    active, disabled = _find_config_entries(config)
    if active or len(disabled) != 1:
        raise DevkitError("experimental artifact did not self-disable config.txt")
    if sha256(suprx) != args.suprx_sha256:
        raise DevkitError("installed SUPRX hash mismatch")
    if sha256(rco) != args.rco_sha256:
        raise DevkitError("installed RCO hash mismatch")
    if not startup_log:
        raise DevkitError("diagnostic build produced no startup log")
    required_log_markers = (
        b"module_start config entries disabled",
        b"overlay active",
    )
    missing_markers = [
        marker.decode() for marker in required_log_markers if marker not in startup_log
    ]
    if missing_markers:
        raise DevkitError(
            "diagnostic startup log is incomplete: " + ", ".join(missing_markers)
        )
    if report["core_files"]:
        raise DevkitError("devkit session captured a core dump")
    report.update(
        {
            "config_sha256": sha256(config),
            "suprx_sha256": sha256(suprx),
            "rco_sha256": sha256(rco),
            "startup_log_sha256": sha256(startup_log),
            "self_disabled": True,
        }
    )
    (evidence / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps({"evidence": str(evidence), **report}, indent=2))


def path_argument(value: str) -> Path:
    path = Path(value).resolve()
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"file does not exist: {path}")
    return path


def directory_argument(value: str) -> Path:
    path = Path(value).resolve()
    if not path.is_dir():
        raise argparse.ArgumentTypeError(f"directory does not exist: {path}")
    return path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--remote", default="memoryhunter")
    subparsers = parser.add_subparsers(dest="command", required=True)

    status = subparsers.add_parser("status")
    status.set_defaults(function=command_status)

    def add_session_arguments(command: argparse.ArgumentParser) -> None:
        command.add_argument(
            "--bridge",
            type=path_argument,
            default=Path("build-vita-devkit-filebridge/filebridge.bin").resolve(),
        )
        command.add_argument(
            "--evidence-root",
            type=Path,
            default=Path("build-vita-regression-diagnostic"),
        )
        command.add_argument("--duration", type=int, default=60)

    baseline = subparsers.add_parser("baseline")
    add_session_arguments(baseline)
    baseline.set_defaults(function=command_baseline)

    stage = subparsers.add_parser("stage")
    add_session_arguments(stage)
    stage.add_argument("--baseline", type=directory_argument, required=True)
    stage.add_argument("--suprx", type=path_argument, required=True)
    stage.add_argument("--rco", type=path_argument, required=True)
    stage.set_defaults(function=command_stage)

    validate = subparsers.add_parser("validate")
    add_session_arguments(validate)
    validate.add_argument("--suprx-sha256", required=True)
    validate.add_argument("--rco-sha256", required=True)
    validate.set_defaults(function=command_validate)
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        args.function(args)
    except (DevkitError, OSError, subprocess.SubprocessError) as error:
        print(json.dumps({"error": str(error)}, indent=2))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
