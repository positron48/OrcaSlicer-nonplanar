import copy
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest
from deps_cache_key import identity


class DependencyCacheIdentity(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        for name in ("deps/recipe.cmake", "cmake/modules/tool.cmake", "scripts/linux.d/debian", "build_linux.sh", "src/app.cpp"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("original\n")
        subprocess.run(["git", "add", "."], cwd=self.root, check=True)
        self.environment = {"compiler": "clang", "packages": "p=1", "image": "v1"}

    def key(self, environment=None):
        return identity(self.root, self.environment if environment is None else environment)["key"]

    def test_dependencies_and_build_recipe_invalidate(self):
        original = self.key()
        for name in ("deps/recipe.cmake", "cmake/modules/tool.cmake", "scripts/linux.d/debian", "build_linux.sh"):
            path = self.root / name
            path.write_text("changed\n")
            self.assertNotEqual(original, self.key())
            path.write_text("original\n")
            self.assertEqual(original, self.key())

    def test_app_and_generated_outputs_do_not_invalidate(self):
        original = self.key()
        (self.root / "src/app.cpp").write_text("new app\n")
        output = self.root / "deps/build/OrcaSlicer_dep/usr/local/library.a"
        output.parent.mkdir(parents=True)
        output.write_bytes(b"built output")
        self.assertEqual(original, self.key())

    def test_environment_and_install_path_invalidate(self):
        original = self.key()
        for key in self.environment:
            changed = copy.deepcopy(self.environment)
            changed[key] += "-different"
            self.assertNotEqual(original, self.key(changed))
        # Include the fixed install prefix independently of repository bytes.
        with tempfile.TemporaryDirectory() as other:
            clone = Path(other) / "checkout"
            shutil.copytree(self.root, clone)
            self.assertNotEqual(original, identity(clone, self.environment)["key"])

    def test_new_tracked_input_and_missing_input_invalidate_or_fail(self):
        original = self.key()
        (self.root / "deps/new.cmake").write_text("new dependency\n")
        subprocess.run(["git", "add", "deps/new.cmake"], cwd=self.root, check=True)
        self.assertNotEqual(original, self.key())
        (self.root / "deps/recipe.cmake").unlink()
        with self.assertRaises(FileNotFoundError):
            self.key()

    def test_key_is_independent_of_mapping_order(self):
        self.assertEqual(self.key(), self.key(dict(reversed(list(self.environment.items())))))


if __name__ == "__main__":
    unittest.main()
