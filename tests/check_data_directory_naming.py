from pathlib import Path
import argparse
import sys


OLD_DIRECTORY = b"livearea" + b"-search"
OLD_PATHS = (
    b"ur0:data/" + OLD_DIRECTORY,
    b"ur0:/data/" + OLD_DIRECTORY,
    b"ur0/data/" + OLD_DIRECTORY,
    b"dist/ur0/data/" + OLD_DIRECTORY,
)

REQUIRED_PATHS = {
    "README.md": (
        b"ur0:data/livearea_search/recent_apps.txt",
        b"ur0:data/livearea_search/livearea_search_plugin.rco",
    ),
    "cmake/VitaShellPlugin.cmake": (
        b"dist/ur0/data/livearea_search",
    ),
    "tools/build-vita.sh": (
        b"dist/ur0/data/livearea_search/livearea_search_plugin.rco",
    ),
    "src/vita/plugin_main.cpp": (
        b"ur0:data/livearea_search/livearea_search_plugin.rco",
    ),
    "src/vita/recent_apps_file.cpp": (
        b"ur0:data/livearea_search/recent_apps.txt",
        b"ur0:data/livearea_search/recent_apps.txt.tmp",
        b"ur0:data/livearea_search/recent_apps.txt.bak",
    ),
    "src/vita/startup_log.cpp": (
        b"ur0:data/livearea_search/startup.log",
    ),
    "tools/devkit/livearea_devkit.py": (
        b"ur0:/data/livearea_search/livearea_search_plugin.rco",
        b"ur0:/data/livearea_search/startup.log",
    ),
    "tools/devkit/livearea_filebridge/bridge.c": (
        b"ur0:/data/livearea_search/livearea_search_plugin.rco",
        b"ur0:/data/livearea_search/startup.log",
    ),
    "tools/devkit/livearea_filebridge/test_bridge.py": (
        b"ur0/data/livearea_search/livearea_search_plugin.rco",
    ),
    "tools/firmware/release_binary_contract.json": (
        b"ur0:data/livearea_search/livearea_search_plugin.rco",
        b"ur0:data/livearea_search/startup.log",
    ),
    "tools/firmware/source_firmware_contract.json": (
        b"ur0:data/livearea_search/startup.log",
    ),
}


def source_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if any(
            part == ".git" or part.startswith("build")
            for part in relative.parts[:-1]
        ):
            continue
        if len(relative.parts) == 1 and relative.name.startswith("build-"):
            continue
        yield relative, path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    root = parser.parse_args().root.resolve()
    failures: list[str] = []

    for relative, path in source_files(root):
        data = path.read_bytes()
        if b"\0" in data:
            continue
        for old_path in OLD_PATHS:
            if old_path in data:
                failures.append(
                    f"{relative}: contains obsolete Vita data path {old_path.decode()}"
                )

    for relative, needles in REQUIRED_PATHS.items():
        path = root / relative
        if not path.is_file():
            failures.append(f"{relative}: required file is missing")
            continue
        data = path.read_bytes()
        for needle in needles:
            if needle not in data:
                failures.append(
                    f"{relative}: missing canonical Vita data path {needle.decode()}"
                )

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
