from pathlib import Path
import tempfile
import unittest
import zipfile

from package_release import package_release


class ReleasePackageTests(unittest.TestCase):
    def test_only_installable_files_are_packaged(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            dist = root / "dist"
            plugin = dist / "livearea_search"
            plugin.mkdir(parents=True)
            (plugin / "livearea_search.suprx").write_bytes(b"plugin")
            (plugin / "livearea_search_plugin.rco").write_bytes(b"resource")
            instructions = (Path(__file__).resolve().parents[1] / "deploy/install.txt").read_bytes()
            (dist / "install.txt").write_bytes(instructions)
            (dist / "SHA256SUMS").write_bytes(b"stale checksum")
            (plugin / "startup.log").write_bytes(b"stale diagnostic")
            output = root / "livearea_search.zip"
            package_release(dist, output)
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.namelist(), [
                    "livearea_search/",
                    "livearea_search/livearea_search.suprx",
                    "livearea_search/livearea_search_plugin.rco",
                    "install.txt",
                ])
                self.assertIsNone(archive.testzip())
                self.assertEqual(archive.read("install.txt"), instructions)
                self.assertEqual(archive.read("livearea_search/livearea_search.suprx"), b"plugin")
                self.assertEqual(archive.read("livearea_search/livearea_search_plugin.rco"), b"resource")
            # Fixed ZIP metadata also makes repeated packaging reproducible.
            original = output.read_bytes()
            package_release(dist, output)
            self.assertEqual(output.read_bytes(), original)

    def test_incomplete_dist_preserves_existing_package(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "release.zip"
            output.write_bytes(b"previous package")
            with self.assertRaises(FileNotFoundError):
                package_release(root / "missing-dist", output)
            self.assertEqual(output.read_bytes(), b"previous package")

    def test_install_instructions_have_unindented_paths_and_config(self) -> None:
        instructions = (Path(__file__).resolve().parents[1] / "deploy/install.txt").read_text()
        self.assertTrue(all(not line or not line[0].isspace() for line in instructions.splitlines()))
        self.assertIn("ur0:/tai/livearea_search/livearea_search.suprx\n", instructions)
        self.assertIn("ur0:/tai/livearea_search/livearea_search_plugin.rco\n", instructions)
        self.assertIn("*main\nur0:tai/livearea_search/livearea_search.suprx\n", instructions)


if __name__ == "__main__":
    unittest.main()
