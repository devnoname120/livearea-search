from __future__ import annotations

import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


HERE = Path(__file__).resolve().parent


class FileBridgeTests(unittest.TestCase):
    executable: Path | None = None

    @classmethod
    def setUpClass(cls) -> None:
        cls.build = tempfile.TemporaryDirectory(prefix="livearea-filebridge-build-")
        executable = Path(cls.build.name) / "bridge"
        result = subprocess.run(
            [
                "clang",
                "-std=c11",
                "-O1",
                "-g",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fsanitize=address,undefined",
                "-D_POSIX_C_SOURCE=200809L",
                "-DFILE_BRIDGE_HOST_TEST",
                "-I",
                str(HERE),
                str(HERE / "bridge.c"),
                str(HERE / "host_io.c"),
                "-o",
                str(executable),
            ],
            capture_output=True,
            timeout=30,
        )
        if result.returncode != 0:
            raise AssertionError(result.stderr.decode())
        cls.executable = executable

    @classmethod
    def tearDownClass(cls) -> None:
        cls.build.cleanup()

    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="livearea-filebridge-case-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "host0/bridge").mkdir(parents=True)
        self.job = 1
        self.config_path = self.root / "ur0/tai/config.txt"
        self.config_path.parent.mkdir(parents=True)
        (self.root / "ur0/tai/livearea_search").mkdir()
        self.config_path.write_bytes(b"*main\nur0:tai/livearea_search/livearea_search.suprx\n")
        (self.root / "ur0/data").mkdir(parents=True)

    def run_job(
        self,
        entries: list[tuple[int, int, int, int, str]],
        env: dict[str, str] | None = None,
    ) -> list[tuple[int, int]]:
        job = self.job
        header = struct.pack("<4I", 0x31464242, 1, len(entries), job)
        raw = header + b"".join(
            struct.pack("<4I256s", operation, limit, new, old, path.encode())
            for operation, limit, new, old, path in entries
        )
        (self.root / "host0/bridge/request.bin").write_bytes(raw)
        assert self.executable is not None
        result = subprocess.run(
            [str(self.executable)],
            cwd=self.root,
            capture_output=True,
            timeout=15,
            env=dict(
                os.environ,
                ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
                UBSAN_OPTIONS="halt_on_error=1",
                **(env or {}),
            ),
        )
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        data = (self.root / f"host0/bridge/{job:08d}.done").read_bytes()
        self.assertEqual(
            struct.unpack_from("<4I", data),
            (0x31524242, 1, len(entries), job),
        )
        output = [
            struct.unpack_from("<iI", data, 16 + 8 * index)
            for index in range(len(entries))
        ]
        self.job += 1
        return output

    def stage(self, index: int, suffix: str, data: bytes) -> None:
        path = self.root / f"host0/bridge/{self.job:08d}-{index:03d}.{suffix}"
        path.write_bytes(data)

    def test_read_and_guarded_replace_return_exact_bytes(self) -> None:
        old = self.config_path.read_bytes()
        new = old.replace(b"ur0:tai", b"#r0:tai")
        self.stage(1, "expected", old)
        self.stage(1, "payload", new)
        status = self.run_job(
            [
                (1, 8192, 0, 0, "ur0:/tai/config.txt"),
                (2, 8192, len(new), len(old), "ur0:/tai/config.txt"),
            ]
        )
        self.assertEqual(status, [(0, len(old)), (0, len(new))])
        self.assertEqual(self.config_path.read_bytes(), new)
        evidence = self.root / "host0/bridge/00000001-001.out"
        self.assertEqual(evidence.read_bytes(), new)

    def test_wrong_expected_never_truncates_remote_and_stops_batch(self) -> None:
        old = self.config_path.read_bytes()
        self.stage(0, "expected", b"wrong")
        self.stage(0, "payload", b"new")
        status = self.run_job(
            [
                (2, 8192, 3, 5, "ur0:/tai/config.txt"),
                (1, 8192, 0, 0, "ur0:/tai/config.txt"),
            ]
        )
        self.assertNotEqual(status[0][0], 0)
        self.assertEqual(status[1][0], -9999)
        self.assertEqual(self.config_path.read_bytes(), old)

    def test_install_creates_only_an_absent_allowlisted_file(self) -> None:
        payload = b"experimental suprx"
        self.stage(0, "payload", payload)
        status = self.run_job(
            [(4, 8192, len(payload), 0, "ur0:/tai/livearea_search/livearea_search.suprx")]
        )
        target = self.root / "ur0/tai/livearea_search/livearea_search.suprx"
        self.assertEqual(status, [(0, len(payload))])
        self.assertEqual(target.read_bytes(), payload)
        evidence = self.root / "host0/bridge/00000001-000.out"
        self.assertEqual(evidence.read_bytes(), payload)

    def test_install_refuses_an_existing_file(self) -> None:
        target = self.root / "ur0/tai/livearea_search/livearea_search.suprx"
        target.write_bytes(b"existing")
        payload = b"replacement"
        self.stage(0, "payload", payload)
        status = self.run_job(
            [(4, 8192, len(payload), 0, "ur0:/tai/livearea_search/livearea_search.suprx")]
        )
        self.assertNotEqual(status[0][0], 0)
        self.assertEqual(target.read_bytes(), b"existing")

    def test_optional_read_reports_an_absent_log_without_stopping(self) -> None:
        status = self.run_job(
            [
                (6, 8192, 0, 0, "ur0:/data/livearea_search/startup.log"),
                (1, 8192, 0, 0, "ur0:/tai/config.txt"),
            ]
        )
        self.assertEqual(status, [(0, 0), (0, len(self.config_path.read_bytes()))])
        evidence = self.root / "host0/bridge/00000001-000.out"
        self.assertTrue(evidence.exists())
        self.assertEqual(evidence.read_bytes(), b"")

    def test_optional_read_rejects_an_unlisted_path_even_when_absent(self) -> None:
        status = self.run_job([(6, 8192, 0, 0, "ur0:/tai/private.bin")])
        self.assertNotEqual(status[0][0], 0)

    def test_zero_length_replacement_and_install_are_rejected(self) -> None:
        old = self.config_path.read_bytes()
        self.stage(0, "expected", old)
        self.stage(0, "payload", b"")
        status = self.run_job(
            [(2, 8192, 0, len(old), "ur0:/tai/config.txt")]
        )
        self.assertNotEqual(status[0][0], 0)

        status = self.run_job(
            [(4, 8192, 0, 0, "ur0:/tai/livearea_search/livearea_search.suprx")]
        )
        self.assertNotEqual(status[0][0], 0)

    def test_failed_install_removes_a_partial_target(self) -> None:
        payload = b"candidate requiring rollback"
        self.stage(0, "payload", payload)
        status = self.run_job(
            [(4, 8192, len(payload), 0, "ur0:/tai/livearea_search/livearea_search.suprx")],
            {"BRIDGE_FAIL_WRITE_CALL": "1"},
        )
        self.assertNotEqual(status[0][0], 0)
        self.assertFalse((self.root / "ur0/tai/livearea_search/livearea_search.suprx").exists())

    def test_ensure_directory_then_install_rco(self) -> None:
        (self.root / "ur0/tai/livearea_search").rmdir()
        payload = b"plugin rco"
        self.stage(1, "payload", payload)
        status = self.run_job(
            [
                (5, 0, 0, 0, "ur0:/tai/livearea_search"),
                (
                    4,
                    8192,
                    len(payload),
                    0,
                    "ur0:/tai/livearea_search/livearea_search_plugin.rco",
                ),
            ]
        )
        self.assertEqual(status, [(0, 0), (0, len(payload))])
        target = self.root / "ur0/tai/livearea_search/livearea_search_plugin.rco"
        self.assertEqual(target.read_bytes(), payload)

    def test_unlisted_write_path_is_rejected(self) -> None:
        protected = self.root / "ur0/tai/other.suprx"
        protected.write_bytes(b"protected")
        self.stage(0, "expected", b"protected")
        self.stage(0, "payload", b"changed")
        status = self.run_job([(2, 8192, 7, 9, "ur0:/tai/other.suprx")])
        self.assertNotEqual(status[0][0], 0)
        self.assertEqual(protected.read_bytes(), b"protected")

    def test_short_writes_are_completed(self) -> None:
        old = self.config_path.read_bytes()
        new = b"*main\n#r0:tai/livearea_search/livearea_search.suprx\n"
        self.stage(0, "expected", old)
        self.stage(0, "payload", new)
        status = self.run_job(
            [(2, 8192, len(new), len(old), "ur0:/tai/config.txt")],
            {"BRIDGE_SHORT_WRITE": "1"},
        )
        self.assertEqual(status, [(0, len(new))])
        self.assertEqual(self.config_path.read_bytes(), new)

    def test_listing_is_bounded_to_the_two_diagnostic_roots(self) -> None:
        data = self.root / "ux0/data"
        data.mkdir(parents=True)
        (data / "psp2core-shell.self-1.psp2dmp").write_bytes(b"core")
        status = self.run_job(
            [
                (3, 8192, 0, 0, "ux0:/data"),
                (3, 8192, 0, 0, "ur0:/tai"),
            ]
        )
        self.assertEqual(status[0][0], 0)
        self.assertNotEqual(status[1][0], 0)
        listing = (self.root / "host0/bridge/00000001-000.out").read_text()
        self.assertIn("psp2core-shell.self-1.psp2dmp", listing)


if __name__ == "__main__":
    unittest.main()
