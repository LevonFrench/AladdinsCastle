"""CPU-only checks for apply.py and the ACPsTrace.h parser.

Run from this folder: python -B -m unittest -v test_apply
"""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("ps_apply", HERE / "apply.py")
apply = importlib.util.module_from_spec(spec)
spec.loader.exec_module(apply)

SAMPLE = (
    '#include "VideoCommon/VertexManagerBase.h"\n#include "VideoCommon/VertexShaderManager.h"\n'
    'void VertexManagerBase::Flush()\n{\n      if (m_current_pipeline_object)\n      {\n'
    '        bool hunter_skip = false;\n        bool elements_skip = false;\n      }\n}\n'
    'void VertexManagerBase::OnEndFrame()\n{\n  m_ac_draw_trace.EndFrame();\n}\n')


class ApplyTests(unittest.TestCase):
    def test_patch_once_and_idempotent(self):
        out = apply.patch_text(SAMPLE)
        self.assertIn("bool hunter_skip = ac_ps_skip;", out)
        self.assertIn("ACPsTrace::Get().EndFrame();", out)
        self.assertEqual(out.count("ACPsTrace.h"), 1)
        self.assertEqual(apply.patch_text(out), out)
        # the draw-trace delta's own OnEndFrame edit stays intact
        self.assertIn("m_ac_draw_trace.EndFrame();", out)

    def test_refuses_missing_duplicate_and_half_patched(self):
        with self.assertRaises(ValueError):
            apply.patch_text(SAMPLE.replace("bool hunter_skip = false;", "bool other = false;"))
        with self.assertRaises(ValueError):
            apply.patch_text(SAMPLE + "        bool hunter_skip = false;\n")
        half = apply.patch_text(SAMPLE).replace("ACPsTrace::Get().EndFrame();", "")
        with self.assertRaises(ValueError):
            apply.patch_text(half)

    def test_cli_keeps_crlf_and_needs_local(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "src"
            (bad / "Source/Core/VideoCommon").mkdir(parents=True)
            (bad / "Source/Core/VideoCommon/VertexManagerBase.cpp").write_bytes(SAMPLE.encode())
            with self.assertRaises(SystemExit):
                apply.main([str(bad), "--any-revision"])
            good = Path(tmp) / ".local" / "src"
            (good / "Source/Core/VideoCommon").mkdir(parents=True)
            target = good / "Source/Core/VideoCommon/VertexManagerBase.cpp"
            target.write_bytes(SAMPLE.replace("\n", "\r\n").encode())
            apply.main([str(good), "--any-revision"])
            data = target.read_bytes()
            self.assertIn(b"\r\n", data)
            self.assertNotIn(b"\n", data.replace(b"\r\n", b""))
            self.assertIn(b"ACPsTrace", data)
            self.assertEqual((good / "Source/Core/VideoCommon/ACPsTrace.h").read_bytes(),
                             (HERE / "ACPsTrace.h").read_bytes())
            before = data
            apply.main([str(good), "--any-revision"])
            self.assertEqual(target.read_bytes(), before)


@unittest.skipUnless(shutil.which("cl") or shutil.which("g++") or shutil.which("clang++"),
                     "no C++ compiler on PATH")
class HeaderTests(unittest.TestCase):
    def test_header_compiles_and_parses(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "t.cpp"
            src.write_text(
                '#include "ACPsTrace.h"\n'
                'int main() {\n'
                '  int bad = 0;\n'
                '  auto v = ACPsTrace::ParseHashList("a8d2ed4e, 0x677241d9;zz 123456789", &bad);\n'
                '  if (v.size() != 2 || v[0] != 0xa8d2ed4eu || v[1] != 0x677241d9u || bad != 2) return 1;\n'
                '  auto& t = ACPsTrace::Get();\n'
                '  if (t.Active() || t.ShouldSkip(0xa8d2ed4eu)) return 2;\n'
                '  return 0;\n'
                '}\n')
            exe = Path(tmp) / "t.exe"
            if shutil.which("cl"):
                cmd = ["cl", "/nologo", "/EHsc", "/std:c++17", "/I", str(HERE), str(src),
                       "/Fe:" + str(exe), "/Fo:" + str(Path(tmp) / "t.obj")]
            else:
                cmd = [shutil.which("g++") or shutil.which("clang++"), "-std=c++17", "-Wall",
                       "-Werror", "-I", str(HERE), str(src), "-o", str(exe)]
            subprocess.run(cmd, check=True, cwd=tmp, capture_output=True)
            env = {k: v for k, v in os.environ.items() if not k.startswith("AC_PS_")}
            self.assertEqual(subprocess.run([str(exe)], env=env).returncode, 0)


if __name__ == "__main__":
    unittest.main()
