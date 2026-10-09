#!/usr/bin/env python3
"""CPU-only immutable-pin and package-provenance regressions."""
import pathlib, re, shutil, subprocess, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
class BuildPolicyTest(unittest.TestCase):
    def test_dependency_pins_are_full_shas(self):
        for path in (ROOT / 'hub/cmake').glob('*.cmake'):
            for tag in re.findall(r'\bGIT_TAG\s+(\S+)', path.read_text(encoding='utf-8')):
                self.assertRegex(tag, r'^[0-9a-f]{40}$', str(path))
    def test_action_pins_and_gpu_free_ci(self):
        source = (ROOT / '.github/workflows/ci.yml').read_text(encoding='utf-8')
        actions = re.findall(r'uses:\s+(\S+)', source)
        self.assertGreaterEqual(len(actions), 5)
        for action in actions: self.assertRegex(action, r'@[0-9a-f]{40}$')
        for setting in ('QT_QPA_PLATFORM: offscreen', 'QT_QUICK_BACKEND: software', 'QT_OPENGL: software'):
            self.assertIn(setting, source)
    def test_package_records_revision(self):
        source = (ROOT / 'hub/packaging/portable-windows.ps1').read_text(encoding='utf-8')
        self.assertIn('-CiRevision $env:GITHUB_SHA', source)
        self.assertIn('Source commit: $buildRevision', source)
    @unittest.skipUnless(shutil.which('pwsh'), 'PowerShell unavailable; Windows CI covers selection')
    def test_revision_selection(self):
        helper = str(ROOT / 'hub/packaging/BuildInfo.ps1').replace("'", "''")
        script = ". '" + helper + "'; "
        script += "if ((Get-PackageBuildRevision -CiRevision ('A'*40) -CheckoutRevision ('b'*40)) -ne ('a'*40)) { exit 1 }; "
        script += "if ((Get-PackageBuildRevision -CheckoutRevision ('b'*40)) -ne ('b'*40)) { exit 2 }; "
        script += "foreach ($invalid in @('', 'main', ('a'*39), ('g'*40))) { $blocked=$false; try { Get-PackageBuildRevision -CiRevision $invalid | Out-Null } catch { $blocked=$true }; if (-not $blocked) { exit 3 } }; exit 0"
        result = subprocess.run(['pwsh', '-NoProfile', '-Command', script], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
if __name__ == '__main__': unittest.main()
