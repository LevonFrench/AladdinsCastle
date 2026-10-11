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
