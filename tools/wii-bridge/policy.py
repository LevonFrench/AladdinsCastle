# SPDX-License-Identifier: GPL-3.0-only
"""Strict consumer of the generated scripted-DSU Wii action descriptor."""
import hashlib
import json
import re
from pathlib import Path


BUTTON_BITS = {
    'Share': 0, 'L3': 1, 'R3': 2, 'Options': 3,
    'Pad N': 4, 'Pad E': 5, 'Pad S': 6, 'Pad W': 7,
    'L2': 8, 'R2': 9, 'L1': 10, 'R1': 11,
    'Triangle': 12, 'Circle': 13, 'Cross': 14, 'Square': 15,
}
TOKENS = {'trigger', 'grip', 'primary', 'secondary', 'click',
          'up', 'down', 'left', 'right'}
ACTION_IDS = {'fire', 'interact', 'dpad-up', 'dpad-down', 'dpad-left', 'dpad-right',
              'menu', 'minus', 'one', 'two', 'home', 'z', 'c', 'stick-up',
              'stick-down', 'stick-left', 'stick-right', 'remote-shake',
              'remote-swing', 'stick-shake', 'stick-swing'}
ID = re.compile(r'[a-z0-9]+(?:-[a-z0-9]+)*\Z')


def _expression(source):
    # The generator emits conjunctions of physical tokens and optional negation.
    if not isinstance(source, str):
        raise ValueError('unsupported action expression')
    parts = source.split(' & ')
    if not parts or len(parts) > 4:
        raise ValueError('unsupported action expression')
    literals = []
    for part in parts:
        negated = part.startswith('!')
        token = part[1:] if negated else part
        if token not in TOKENS or token in (x[0] for x in literals):
            raise ValueError('unsupported action expression')
        literals.append((token, negated))
    return tuple(literals)


class CanonicalPolicy:
    def __init__(self, data, source_hash):
        if (not isinstance(data, dict) or not isinstance(data.get('players'), list) or
                len(data['players']) != 1 or not isinstance(data['players'][0], dict) or
                not isinstance(data['players'][0].get('actions'), list) or
                not all(key in data['players'][0] for key in ('remote_hand', 'stick_hand')) or
                not all(isinstance(data.get(key), dict) for key in ('motion', 'reload', 'server'))):
            raise ValueError('invalid scripted DSU players')
        self.data = data
        self.source_hash = source_hash
        self.primary_hand = data['primary_hand']
        self.remote_hand = data['players'][0]['remote_hand']
        self.stick_hand = data['players'][0]['stick_hand']
        self.actions = []
        self.fire_masks = [0, 0]
        if (data.get('schema') != 'scripted-dsu/1' or data.get('version') != 'shell-stick/1' or
                data.get('route') != 'flat-dolphin' or data.get('mode') not in ('split', 'docked') or
                data.get('device_slots') != {'remote': 0, 'nunchuk': 1} or
                data.get('recenter_chord') != ['right.click', 'left.click'] or
                data.get('recenter_suppresses_game_buttons') is not True or
                data.get('motion', {}).get('physical') is not True or
                data.get('reload', {}).get('enabled') is not False):
            raise ValueError('unsupported scripted DSU contract')
        producer = data.get('producer_policy')
        if not isinstance(producer, dict) or producer.get('schema') != 'canonical-dsu/1':
            raise ValueError('missing canonical producer policy')
        tokens = producer.get('expression_tokens')
        if not isinstance(tokens, list) or any(not isinstance(token, str) for token in tokens):
            raise ValueError('invalid canonical expression tokens')
        recenter = producer.get('recenter', {})
        producers = producer.get('producers', {})
        if not isinstance(recenter, dict) or not isinstance(producers, dict):
            raise ValueError('invalid canonical producer capability')
        scripted = producers.get('scripted_dsu', {})
        legacy = producers.get('legacy_steamvr', {})
        live = producers.get('canonical_live', {})
        if not all(isinstance(row, dict) for row in (scripted, legacy, live)):
            raise ValueError('invalid canonical producer capability')
        if (set(tokens) != TOKENS or len(tokens) != len(TOKENS) or
                producer.get('slots') != {'remote': 0, 'nunchuk': 1} or
                producer.get('unsupported_actions') != ['home'] or
                producer.get('offscreen_hide_supported') is not False or
                recenter.get('chord') != data['recenter_chord'] or
                recenter.get('suppress_game_buttons') is not True or
                recenter.get('shared_theatre_calibration') is not True or
                scripted.get('supported') is not True or scripted.get('tracked_recenter') is not False or
                legacy.get('supported') is not False or not isinstance(legacy.get('reason'), str) or
                not legacy['reason'] or data.get('recenter_supported') is not False):
            raise ValueError('unsupported canonical producer capability')
        if (self.primary_hand not in ('right', 'left') or self.remote_hand != self.primary_hand or
                self.stick_hand != ('left' if self.primary_hand == 'right' else 'right') or
                data['players'][0].get('player') != 0):
            raise ValueError('scripted DSU hand/role mismatch')
        if data.get('server', {}).get('host') != '127.0.0.1' or not isinstance(data['server'].get('port'), int):
            raise ValueError('unsupported scripted DSU server')
        seen = set()
        for item in data['players'][0]['actions']:
            if (not isinstance(item, dict) or
                    not all(key in item for key in ('id', 'role', 'wire', 'expression', 'hand', 'slot', 'script')) or
                    not isinstance(item['script'], dict) or
                    not all(isinstance(item[key], str) for key in ('id', 'role', 'wire', 'expression', 'hand')) or
                    type(item['slot']) is not int):
                raise ValueError('invalid scripted DSU action')
            action_id = item['id']
            if action_id not in ACTION_IDS or action_id in seen:
                raise ValueError('unknown or duplicate scripted DSU action')
            seen.add(action_id)
            role = item['role']
            slot = 0 if role == 'remote' else 1 if role == 'nunchuk' else -1
            hand = self.remote_hand if slot == 0 else self.stick_hand
            if slot < 0 or item['slot'] != slot or item['hand'] != hand:
                raise ValueError('scripted DSU role/slot/hand mismatch')
            terms = _expression(item['expression'])
            script = item['script']
            if action_id == 'home':
                if item.get('supported') is not False or script.get('supported') is not False or item.get('wire') != 'PS':
                    raise ValueError('Home must remain explicitly unsupported')
                continue
            if item.get('supported') is not True or script.get('supported') is not True:
                raise ValueError('scripted DSU action unsupported')
            if script.get('field') == 'buttons':
                if script.get('button') != item['wire'] or item['wire'] not in BUTTON_BITS:
                    raise ValueError('scripted DSU button mismatch')
                output = ('button', BUTTON_BITS[item['wire']], 0)
            elif script.get('field') == 'stick':
                if action_id not in ('stick-up', 'stick-down', 'stick-left', 'stick-right'):
                    raise ValueError('unsupported scripted DSU stick action')
                axis, value = script.get('axis'), script.get('value')
                if (axis not in (0, 1) or value not in (-1, 1) or isinstance(axis, bool) or
                        isinstance(value, bool) or
                        (axis, value) != {'stick-up': (1, 1), 'stick-down': (1, -1),
                                          'stick-left': (0, -1), 'stick-right': (0, 1)}[action_id]):
                    raise ValueError('scripted DSU stick axis mismatch')
                output = ('stick', axis, value)
            else:
                raise ValueError('unsupported scripted DSU field')
            if action_id == 'fire':
                if output[0] != 'button':
                    raise ValueError('fire action needs a button field')
                self.fire_masks[slot] = 1 << output[1]
            self.actions.append((hand, slot, terms, output))
        if seen != ACTION_IDS:
            raise ValueError('incomplete scripted DSU actions')
        if (live.get('supported') is not True or live.get('state') != 'source-tested' or
                data.get('live_bridge_compatible') is not True):
            raise ValueError('canonical live route not approved by producer')

    @classmethod
    def load(cls, root, control_set, primary_hand):
        if not isinstance(control_set, str) or not ID.fullmatch(control_set) or primary_hand not in ('right', 'left'):
            raise ValueError('invalid control set or primary hand')
        base = Path(root) / 'data/controls/wii/generated'
        manifest = json.loads((base / 'generated-files.json').read_text(encoding='utf-8'))
        if (not isinstance(manifest, dict) or manifest.get('version') != 'shell-stick/1' or
                manifest.get('source') != 'data/controls/wii/shell-and-stick.toml' or
                not isinstance(manifest.get('files'), dict)):
            raise ValueError('invalid generated controls manifest')
        rel = f'games/{control_set}/scripted-dsu-{primary_hand}'
        selected = {}
        for suffix in ('.json', '.ini'):
            expected = manifest['files'].get(rel + suffix)
            path = base / (rel + suffix)
            if not isinstance(expected, str) or not re.fullmatch(r'[0-9a-f]{64}', expected):
                raise ValueError('scripted DSU generated hash missing')
            selected[suffix] = path.read_bytes()
            if hashlib.sha256(selected[suffix]).hexdigest() != expected:
                raise ValueError('scripted DSU generated hash mismatch')
        data = json.loads(selected['.json'].decode('utf-8'))
        if not isinstance(data, dict) or data.get('game_id') != control_set or data.get('primary_hand') != primary_hand:
            raise ValueError('scripted DSU game/hand identity mismatch')
        policy = cls(data, manifest['files'][rel + '.json'])
        product = json.loads((Path(root) / 'data/product.json').read_text(encoding='utf-8'))
        if (not isinstance(product, dict) or product.get('schema') != 1 or
                policy.data['server'].get('name') != product.get('dsu_server_name') or
                policy.data['server'].get('port') != 26760):
            raise ValueError('scripted DSU product/server identity mismatch')
        return policy

    def evaluate(self, hands):
        """Return role-slot button masks, continuous stick, tracking, chord."""
        by_hand = {'left': hands[0], 'right': hands[1]}
        chord = all(bool(by_hand[h]['valid']) and bool(by_hand[h]['buttons'][5]) for h in ('left', 'right'))
        masks, sticks = [0, 0], [[0., 0.], [0., 0.]]
        tracked = [bool(by_hand[self.remote_hand]['valid']), bool(by_hand[self.stick_hand]['valid'])]
        if chord:
            return masks, sticks, tracked, True
        for hand, slot, terms, output in self.actions:
            sample = by_hand[hand]
            if not sample['valid']:
                continue
            buttons = sample['buttons']
            x, y = sample['stick']
            state = dict(zip(('trigger', 'grip', 'primary', 'secondary', 'menu', 'click'), buttons))
            # D-pad needs a deliberate deflection; accessory analog uses its
            # measured magnitude after the same bounded input threshold.
            state.update(up=y >= .5, down=y <= -.5, left=x <= -.5, right=x >= .5)
            if not all(bool(state[token]) != negated for token, negated in terms):
                continue
            kind, index, value = output
            if kind == 'button':
                masks[slot] |= 1 << index
            else:
                sticks[slot][index] = value * abs(sample['stick'][index])
        return masks, sticks, tracked, False
