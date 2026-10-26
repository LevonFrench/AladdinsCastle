"""Chord resolver regressions (CPU only): a small model of Dolphin's expression functions.

The model follows the pinned fork's FunctionExpression.cpp for hold(), pulse() and the boolean
operators (AND = min, OR = max, NOT = 1 - x, no short circuit, threshold 0.5), and runs the exact
expressions the profiles carry against scripted click timelines at the 5 ms hotkey poll rate.
"""
import re
import unittest

import universal_controls as uc

STEP = 5  # ms, the HotkeyScheduler's sleep
THRESHOLD = 0.5


def tokenize(text):
    return re.findall(r'`[^`]*`|[A-Za-z_]+|[0-9.]+|[&|!(),]', text)


class Parser:
    def __init__(self, text):
        self.t = tokenize(text)
        self.i = 0

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def take(self):
        self.i += 1
        return self.t[self.i - 1]

    def parse(self):
        node = self.or_()
        assert self.peek() is None, self.t[self.i:]
        return node

    def or_(self):
        node = self.and_()
        while self.peek() == '|':
            self.take()
            node = ('or', node, self.and_())
        return node

    def and_(self):
        node = self.unary()
        while self.peek() == '&':
            self.take()
            node = ('and', node, self.unary())
        return node

    def unary(self):
        tok = self.take()
        if tok == '!':
            return ('not', self.unary())
        if tok == '(':
            node = self.or_()
            assert self.take() == ')'
            return node
        if tok.startswith('`'):
            return ('in', tok.strip('`').split(':')[-1])
        if re.fullmatch(r'[0-9.]+', tok):
            return ('num', float(tok))
        assert self.take() == '('
        args = [self.or_()]
        while self.peek() == ',':
            self.take()
            args.append(self.or_())
        assert self.take() == ')'
        return ('fn', tok, args, {})  # last item is this node's state


def evaluate(node, now, inputs):
    kind = node[0]
    if kind == 'num':
        return node[1]
    if kind == 'in':
        return 1.0 if inputs.get(node[1]) else 0.0
    if kind == 'not':
        return 1.0 - evaluate(node[1], now, inputs)
    if kind in ('and', 'or'):
        a, b = evaluate(node[1], now, inputs), evaluate(node[2], now, inputs)
        return min(a, b) if kind == 'and' else max(a, b)
    _, name, args, st = node
    values = [evaluate(a, now, inputs) for a in args]
    if name == 'hold':
        if values[0] < THRESHOLD:
            st['on'], st['start'] = False, now
        elif not st.get('on') and now - st.get('start', now) >= values[1] * 1000:
            st['on'] = True
        return 1.0 if st.get('on') else 0.0
    if name == 'pulse':
        if values[0] < THRESHOLD:
            st['released'] = True
        elif st.get('released', False):
            st['released'] = False
            end = values[1] * 1000
            if st.get('on'):
                st['end'] += end
            else:
                st['on'], st['end'] = True, now + end
        if st.get('on') and now >= st['end']:
            st['on'] = False
        return 1.0 if st.get('on') else 0.0
    raise AssertionError(name)


LC = 'Left Button Thumbstick'
RC = 'Right Button Thumbstick'
EDGE_FREE = 'Hotkey'


def run(expression, events, length, edge=False):
    """events: [(ms, input, down)]. Returns the list of (start_ms, end_ms) active spans, or the
    press-edge times when edge is set (what a Dolphin hotkey acts on)."""
    tree = Parser(expression.replace('@(Shift+R) | ', '')).parse()
    inputs, spans, begin, previous = {}, [], None, 0.0
    events = sorted(events)
    for now in range(0, length, STEP):
        while events and events[0][0] <= now:
            _, name, down = events.pop(0)
            inputs[name] = down
        value = evaluate(tree, now, inputs)
        if value >= THRESHOLD and begin is None:
            begin = now
        if value < THRESHOLD and begin is not None:
            spans.append((begin, now))
            begin = None
    if begin is not None:
        spans.append((begin, length))
    return [s[0] for s in spans] if edge else spans


def click(side, down_ms, up_ms):
    name = LC if side == 'L' else RC
    return [(down_ms, name, True), (up_ms, name, False)]


class ChordResolver(unittest.TestCase):
    def test_lone_left_click_is_start_for_as_long_as_it_is_held(self):
        spans = run(uc.START, click('L', 100, 900), 3000)
        self.assertEqual(len(spans), 1)
        self.assertTrue(240 <= spans[0][0] <= 270)  # 150 ms wait after the press
        self.assertTrue(900 <= spans[0][1] <= 1010)
        self.assertEqual(run(uc.SELECT, click('L', 100, 900), 3000), [])

    def test_short_tap_still_sends_a_press(self):
        spans = run(uc.START, click('L', 100, 180), 3000)
        self.assertEqual(len(spans), 1)
        self.assertTrue(180 <= spans[0][0] <= 190)

    def test_lone_right_click_is_select_only(self):
        self.assertEqual(len(run(uc.SELECT, click('R', 100, 900), 3000)), 1)
        self.assertEqual(run(uc.START, click('R', 100, 900), 3000), [])

    def test_chord_sends_no_start_or_select_in_any_release_order(self):
        for first, second in (('L', 'R'), ('R', 'L')):
            for gap in (0, 40, 120):
                for order in (0, 1):
                    for hold in (300, 900, 1500, 2500):
                        a = click(first, 100, 100 + hold + (30 if order else 0))
                        b = click(second, 100 + gap, 100 + gap + hold + (0 if order else 30))
                        for expr in (uc.START, uc.SELECT):
                            self.assertEqual(run(expr, a + b, 8000), [], (first, gap, order, hold))

    def test_recentre_fires_once_on_release_of_a_quick_chord(self):
        for gap in (0, 100, 140):
            edges = run(uc.RECENTRE, click('L', 100, 700) + click('R', 100 + gap, 720), 3000, edge=True)
            self.assertEqual(len(edges), 1, gap)
            self.assertTrue(700 <= edges[0] <= 740)

    def test_recentre_ignores_single_clicks_late_second_click_and_long_holds(self):
        self.assertEqual(run(uc.RECENTRE, click('L', 100, 600), 3000), [])
        self.assertEqual(run(uc.RECENTRE, click('R', 100, 600), 3000), [])
        # second click 200 ms (past the 150 ms window) or 400 ms after the first is not a chord for recentring
        self.assertEqual(run(uc.RECENTRE, click('L', 100, 900) + click('R', 300, 900), 3000), [])
        self.assertEqual(run(uc.RECENTRE, click('L', 100, 900) + click('R', 500, 900), 3000), [])
        # held past 1 s: the release does nothing
        self.assertEqual(run(uc.RECENTRE, click('L', 100, 1600) + click('R', 130, 1600), 4000), [])

    def test_quit_needs_both_clicks_for_three_seconds(self):
        both = lambda up: click('L', 100, up) + click('R', 130, up)
        self.assertEqual(len(run(uc.QUIT, both(3500), 5000, edge=True)), 1)
        self.assertEqual(run(uc.QUIT, both(2900), 5000), [])
        self.assertEqual(run(uc.QUIT, click('L', 100, 5000), 6000), [])

    def test_start_returns_after_the_block_window(self):
        events = click('L', 100, 500) + click('R', 120, 520) + click('L', 3000, 3600)
        self.assertEqual(len(run(uc.START, events, 6000)), 1)
        # long hold (over 1 s) then release blocks Start for 3.5 s
        events = click('L', 100, 2000) + click('R', 120, 1990) + click('L', 3000, 3600)
        self.assertEqual(run(uc.START, events, 6000), [])


class ProfileLayer(unittest.TestCase):
    def test_every_profile_carries_the_universal_layer(self):
        self.assertEqual(uc.main([]), 0)

    def test_no_profile_binds_a_menu_button_or_the_old_left_y_duties(self):
        for path in uc.wii_profile_files() + uc.gc_profile_files():
            text = path.read_text(encoding='utf-8')
            self.assertNotIn('Button Menu', text, path)
            self.assertNotIn('Button Thumbstick` & `Left Thumbstick', text, path)
            self.assertNotRegex(text, r'Buttons/-\s*=\s*pulse\(!`Left Button Y`', path)

    def test_remote_two_is_left_y_tap_where_it_was_the_right_click(self):
        for path in uc.wii_profile_files():
            text = path.read_text(encoding='utf-8')
            self.assertNotIn('Buttons/2 = `Right Button Thumbstick`', text, path)

    def test_gamecube_z_does_not_cost_x_or_y(self):
        for path in uc.gc_profile_files():
            text = path.read_text(encoding='utf-8')
            self.assertIn(f'Buttons/Z = {uc.GC_Z}', text)
            self.assertIn(f'Buttons/X = {uc.GC_X}', text)
            self.assertIn(f'Buttons/Y = {uc.GC_Y}', text)
            self.assertIn(f'Buttons/Start = {uc.START}', text)
            self.assertNotIn('Buttons/Select', text)

    def test_hotkey_template_binds_recentre_and_exit_on_the_openxr_device(self):
        text = (uc.HERE.parent / 'templates/Hotkeys.ini').read_text(encoding='utf-8')
        self.assertIn('General/Exit = hold(', text)
        self.assertIn('VR/Reset VR Position = ', text)
        self.assertNotIn('Button Menu', text)
        self.assertNotIn('Raw Accel', text)
        for name in re.findall(r'`([^`]*)`', text):
            self.assertTrue(name.startswith(uc.DEVICE + ':'), name)


    def test_installers_seed_the_same_hotkeys_and_confirm_stop(self):
        for path in sorted((uc.ROOT / 'games').glob('wii-*/install.toml')):
            text = path.read_text(encoding='utf-8')
            if 'Hotkeys.ini' not in text:
                continue
            for key, value in uc.HOTKEYS.items():
                self.assertIn(f'key = "{key}", value = "{value}"', text, path)
            self.assertIn('key = "ConfirmStop", value = "False"', text, path)


if __name__ == '__main__':
    unittest.main()
