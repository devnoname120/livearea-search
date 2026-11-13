"""Package only the installable plugin, resource and installation instructions."""

from pathlib import Path
import argparse
import tempfile
import zipfile


PACKAGE_FILES = (
    "livearea_search/livearea_search.suprx",
    "livearea_search/livearea_search_plugin.rco",
    "install.txt",
)


def package_release(dist: Path, output: Path) -> None:
    # Read the explicit file list before touching an existing archive. Stale
    # build outputs, logs and checksum files must never enter the release ZIP.
    contents = [(name, (dist / name).read_bytes()) for name in PACKAGE_FILES]
    if any(not data for _, data in contents):
        raise ValueError("release files must not be empty")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as temporary:
        temporary_path = Path(temporary.name)
    try:
        with zipfile.ZipFile(temporary_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            directory = zipfile.ZipInfo("livearea_search/")
            directory.create_system = 3
            directory.external_attr = (0o40755 << 16) | 0x10
            archive.writestr(directory, b"")
            for name, data in contents:
                info = zipfile.ZipInfo(name)
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, data)
        temporary_path.replace(output)
    finally:
        temporary_path.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    package_release(args.dist, args.output)


if __name__ == "__main__":
    main()
