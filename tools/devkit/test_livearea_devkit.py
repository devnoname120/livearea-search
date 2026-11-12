from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parent / "livearea_devkit.py"
SPEC = importlib.util.spec_from_file_location("livearea_devkit", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
livearea_devkit = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = livearea_devkit
SPEC.loader.exec_module(livearea_devkit)


class ProtocolTests(unittest.TestCase):
    def test_request_and_result_round_trip(self) -> None:
        entries = [
            livearea_devkit.Entry(
                livearea_devkit.OP_READ,
                livearea_devkit.CONFIG_PATH,
                limit=4096,
            ),
            livearea_devkit.Entry(
                livearea_devkit.OP_READ_OPTIONAL,
                livearea_devkit.STARTUP_LOG_PATH,
                limit=8192,
            ),
        ]
        request = livearea_devkit.pack_request(entries, 7)
        self.assertEqual(
            struct.unpack_from("<4I", request),
            (livearea_devkit.REQUEST_MAGIC, 1, 2, 7),
        )
        self.assertIn(b"ur0:/tai/config.txt", request)

        result = struct.pack(
            "<4IiIiI",
            livearea_devkit.RESULT_MAGIC,
            1,
            2,
            7,
            0,
            123,
            0,
            0,
        )
        self.assertEqual(
            livearea_devkit.parse_result(result, 2, 7),
            [
                livearea_devkit.ResultEntry(0, 123),
                livearea_devkit.ResultEntry(0, 0),
            ],
        )

    def test_protocol_rejects_invalid_bounds_and_headers(self) -> None:
        with self.assertRaises(livearea_devkit.DevkitError):
            livearea_devkit.pack_request([], 1)
        with self.assertRaises(livearea_devkit.DevkitError):
            livearea_devkit.pack_request(
                [livearea_devkit.Entry(1, "ur0:/tai/config.txt", limit=-1)],
                1,
            )
        with self.assertRaises(livearea_devkit.DevkitError):
            livearea_devkit.parse_result(b"bad", 1, 1)


class ConfigTests(unittest.TestCase):
    def test_active_entry_produces_exact_disabled_and_active_variants(self) -> None:
        original = b"*KERNEL\n*main\nur0:tai/livearea_search.suprx\n"
        disabled, active = livearea_devkit.config_variants(original)
        self.assertEqual(active, original)
        self.assertEqual(
            disabled,
            b"*KERNEL\n*main\n#r0:tai/livearea_search.suprx\n",
        )

    def test_disabled_entry_reactivates_without_reformatting(self) -> None:
        original = b"*main\r\n#r0:/tai/livearea_search.suprx\r\n"
        disabled, active = livearea_devkit.config_variants(original)
        self.assertEqual(disabled, original)
        self.assertEqual(
            active,
            b"*main\r\nur0:/tai/livearea_search.suprx\r\n",
        )

    def test_missing_entry_is_inserted_disabled_after_main(self) -> None:
        original = b"*KERNEL\nfoo.skprx\n*main\nbar.suprx\n"
        disabled, active = livearea_devkit.config_variants(original)
        self.assertEqual(
            disabled,
            b"*KERNEL\nfoo.skprx\n*main\n#r0:tai/livearea_search.suprx\nbar.suprx\n",
        )
        self.assertEqual(
            active,
            b"*KERNEL\nfoo.skprx\n*main\nur0:tai/livearea_search.suprx\nbar.suprx\n",
        )

    def test_ambiguous_or_indented_entries_are_rejected(self) -> None:
        with self.assertRaises(livearea_devkit.DevkitError):
            livearea_devkit.config_variants(
                b"*main\nur0:tai/livearea_search.suprx\n#r0:tai/livearea_search.suprx\n"
            )
        with self.assertRaises(livearea_devkit.DevkitError):
            livearea_devkit.config_variants(
                b"*main\n  ur0:tai/livearea_search.suprx\n"
            )


class StagePlanTests(unittest.TestCase):
    def test_absent_artifacts_are_installed_between_disable_and_activate(self) -> None:
        config = b"*main\nur0:tai/livearea_search.suprx\n"
        entries, staged, disabled, active = livearea_devkit.build_stage_plan(
            config,
            b"",
            b"",
            b"new-suprx",
            b"new-rco",
        )
        self.assertEqual(
            [entry.operation for entry in entries],
            [
                livearea_devkit.OP_REPLACE,
                livearea_devkit.OP_ENSURE_DIR,
                livearea_devkit.OP_INSTALL,
                livearea_devkit.OP_INSTALL,
                livearea_devkit.OP_REPLACE,
            ],
        )
        self.assertEqual(staged[(0, "expected")], config)
        self.assertEqual(staged[(0, "payload")], disabled)
        self.assertEqual(staged[(4, "expected")], disabled)
        self.assertEqual(staged[(4, "payload")], active)
        self.assertNotIn((2, "expected"), staged)
        self.assertNotIn((3, "expected"), staged)

    def test_existing_artifacts_require_exact_old_bytes(self) -> None:
        entries, staged, _, _ = livearea_devkit.build_stage_plan(
            b"*main\n#r0:tai/livearea_search.suprx\n",
            b"old-suprx",
            b"old-rco",
            b"new-suprx",
            b"new-rco",
        )
        self.assertEqual(entries[2].operation, livearea_devkit.OP_REPLACE)
        self.assertEqual(entries[3].operation, livearea_devkit.OP_REPLACE)
        self.assertEqual(staged[(2, "expected")], b"old-suprx")
        self.assertEqual(staged[(3, "expected")], b"old-rco")

    def test_baseline_reads_are_nonmutating(self) -> None:
        entries = livearea_devkit.baseline_entries()
        self.assertEqual(
            [entry.operation for entry in entries],
            [
                livearea_devkit.OP_READ,
                livearea_devkit.OP_READ_OPTIONAL,
                livearea_devkit.OP_READ_OPTIONAL,
                livearea_devkit.OP_READ_OPTIONAL,
            ],
        )

    def test_stage_plan_never_emits_zero_length_writes(self) -> None:
        entries, _, _, _ = livearea_devkit.build_stage_plan(
            b"*main\n#r0:tai/livearea_search.suprx\n",
            b"old-suprx",
            b"old-rco",
            b"new-suprx",
            b"new-rco",
        )
        for entry in entries:
            if entry.operation in (
                livearea_devkit.OP_REPLACE,
                livearea_devkit.OP_INSTALL,
            ):
                self.assertGreater(entry.new_size, 0)


class OwnershipTests(unittest.TestCase):
    def test_owner_probe_has_no_destructive_process_control(self) -> None:
        script = livearea_devkit.remote_owner_probe_script()
        self.assertIn("/proc", script)
        self.assertNotIn("kill", script)
        self.assertNotIn("pkill", script)

    def test_owned_remote_is_rejected(self) -> None:
        remote = livearea_devkit.Remote("unused")
        remote.owners = lambda: [
            {"pid": 7, "argv": ["bash", "/tooling/devkit/session.sh"]}
        ]
        with self.assertRaises(livearea_devkit.DevkitError):
            remote.require_unowned()


if __name__ == "__main__":
    unittest.main()
