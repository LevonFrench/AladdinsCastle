"""CPU-only patch-boundary checks against explicitly supplied pinned sources."""
import argparse
from contextlib import redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from unittest.mock import patch

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    args = parser.parse_args()
    scratch = args.scratch.resolve()
    if '.local' not in scratch.parts:
        raise RuntimeError('Private scratch required')
    scratch.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    spec = importlib.util.spec_from_file_location('native_prepare', here/'prepare.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    manifest = json.loads((here/'source-hashes.json').read_text())
    with tempfile.TemporaryDirectory(prefix='guards-', dir=scratch) as tmp:
        root = Path(tmp)
        fixture = root/'source'
        for name in manifest:
            target = fixture/name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((args.source/name).read_bytes())
        def invoke(output, pin=module.PIN):
            with patch('sys.argv', ['prepare', '--source', str(fixture), '--output', str(output)]), \
                 patch.object(module.subprocess, 'check_output', return_value=pin), \
                 redirect_stdout(io.StringIO()):
                module.main()
        out = root/'good'
        invoke(out)
        assert (out/'LICENSE').exists()
        assert 'acn_view' in (out/'engine/geo_hw.h').read_text()
        assert 'ACN22_SYNTHETIC_SLICE' in (out/'engine/ss22_gl.c').read_text()
        assert 'if (!nb) { acn22_invalidate_frame(); return; }' in (out/'engine/ss22_gl.c').read_text()
        batch = (out/'engine/quad_gl.c').read_text()
        assert 'acn22_native_capture_fan((uint32_t)n, qa_xy, qa_rgba, qa_st); return;' in batch
        assert 'glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_NEAREST)' in batch
        assert 'glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE)' in batch
        assert '!st_buf.pose && !bb_run.pose' in batch
        assert 'int acn22_proof_prime(' in batch
        assert 'glGetTexImage(GL_TEXTURE_1D,0,GL_ALPHA,GL_UNSIGNED_BYTE,actual)' in batch
        assert 'ACN22_NATIVE_PROC("glActiveTexture")' in batch
        assert 'acn22_adapter_quad_shutdown(void)' in batch
        assert '#else\n'+__import__('native_patch').POSE_TEXTURE in batch
        tex = (out/'engine/tex_bake.c').read_text()
        assert '#if ACN22_NATIVE_GPU_PROOF' in tex
        assert 'if (acn22_proof_cache_locked())' in tex
        assert 'out_su,out_sv,out_ou,out_ov,0,1); /* lookup ONLY */' in tex
        assert 'acn22_adapter_cache_shutdown(void)' in tex
        assert 'Exclude legitimate hit ref bit and all C padding' in tex
        sentinel = out/'sentinel'
        sentinel.write_text('keep')
        def refused(output, pin=module.PIN):
            try:
                invoke(output,pin)
            except RuntimeError:
                return
            raise AssertionError('Unsafe patch accepted')
        refused(out)
        assert sentinel.read_text() == 'keep'
        refused(root/'wrong-pin', 'bad-pin')
        assert not (root/'wrong-pin').exists()
        changed = fixture/'engine/geo_hw.c'
        changed.write_text(changed.read_text()+'\n/* deliberate source drift */\n')
        refused(root/'drift')
        assert not (root/'drift').exists()
        outside = Path(tempfile.gettempdir())/'acn22-forbidden-native-overlay'
        assert '.local' not in outside.resolve().parts
        refused(outside)
    print('Patch boundary: fresh output, preserved sentinel, wrong pin, source drift, private path: PASS')

if __name__ == '__main__':
    main()
