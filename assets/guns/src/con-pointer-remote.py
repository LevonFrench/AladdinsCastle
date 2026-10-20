"""Original con-pointer-remote; GPL-3.0-only source, CC0 generated asset.

A Wii Remote-like pointer controller at true size (148 x 36 x 31 mm): rounded bar, IR window
at the pointing end, power button, D-pad, A, B trigger underneath, minus / home / plus, a
speaker grille, 1 and 2, four player lights and a wrist-strap loop at the back. Our own
dimensions and layout from the controller's published size; no logo, lettering or markings.
Grip anchor: where the hand holds it, a little behind the middle (as the DolphinXR stand-in).
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=148, width_mm=36, depth_mm=31)

FRONT, BACK, W, H = -92.0, 56.0, 18.1, 15.4      # mm; grip anchor at the origin, -Z points
TOP = H


def make_design():
    from common import Design
    d = Design(PARAMETERS['length_mm'])
    # body: a squarish superellipse bar, rounded into both ends
    d.zloft('shell', [(FRONT, W - 2.6, H - 2.4, 0), (FRONT + .5, W - 1.1, H - 1.0, 0), (FRONT + 2.0, W, H, 0),
                      (BACK - 2.0, W, H, 0), (BACK - .5, W - 1.1, H - 1.0, 0), (BACK, W - 2.6, H - 2.4, 0)], power=3.2)
    # infrared window: dark, filling most of the pointing end
    d.zloft('ir_window', [(FRONT - .4, W - 4.2, H - 4.4, 0), (FRONT + 1.2, W - 4.2, H - 4.4, 0)], 'dark', power=3.6)
    # power button, front left of the top face
    d.cylinder('power_button', (-9.5, TOP + .3, FRONT + 9), 2.6, 1.6, 'dark', axis='y')
    # D-pad: a cross on a round dark well
    d.node('button_dpad', (0, TOP + 1.2, FRONT + 25))
    d.cylinder('dpad_well', (0, TOP - .1, FRONT + 25), 11.2, 1.0, 'dark', axis='y')
    d.box('dpad_cross_vertical', (0, TOP + 1.0, FRONT + 25), (6.6, 2.4, 19.6), 'accent', 'button_dpad', .8)
    d.box('dpad_cross_horizontal', (0, TOP + 1.0, FRONT + 25), (19.6, 2.4, 6.6), 'accent', 'button_dpad', .8)
    # A: the big round button
    d.node('button_a', (0, TOP + 1.4, FRONT + 49))
    d.cylinder('a_well', (0, TOP - .1, FRONT + 49), 9.4, 1.0, 'dark', axis='y')
    d.cylinder('button_a_cap', (0, TOP + 1.3, FRONT + 49), 8.0, 2.8, 'accent', 'button_a', axis='y')
    # B trigger underneath, under the A button, curving down for the index finger
    d.node('button_b', (0, -H, FRONT + 46))
    d.outline('button_b_cap', [(FRONT + 34, -H + .8), (FRONT + 58, -H + .8), (FRONT + 56, -H - 1.8), (FRONT + 51, -H - 3.6),
                               (FRONT + 44, -H - 4.4), (FRONT + 38, -H - 3.8), (FRONT + 35, -H - 2.0)],
              15, 'accent', 'button_b', bevel=1.5)
    # minus, home, plus
    for bid, x, r in (('minus', -12.5, 3.0), ('home', 0.0, 4.4), ('plus', 12.5, 3.0)):
        d.node('button_' + bid, (x, TOP + .8, FRONT + 82))
        d.cylinder('button_' + bid + '_cap', (x, TOP + .7, FRONT + 82), r, 1.6, 'accent', 'button_' + bid, axis='y')
    # speaker grille: dark holes in five rows
    for row in range(5):
        for col in range(5):
            d.box('speaker_hole_%d_%d' % (row, col), (-6 + col * 3, TOP - .05, FRONT + 92 + row * 2.6), (1.5, .5, 1.5), 'dark', bevel=.4)
    # 1 and 2
    for bid, z in (('one', FRONT + 113), ('two', FRONT + 126)):
        d.node('button_' + bid, (0, TOP + .8, z))
        d.cylinder('button_' + bid + '_cap', (0, TOP + .7, z), 4.6, 1.6, 'accent', 'button_' + bid, axis='y')
    # four player lights near the back
    for i in range(4):
        d.box('player_light_%d' % i, (-10.5 + i * 7, TOP - .05, BACK - 8), (2.4, .6, 1.4), 'glass', bevel=.2)
    # expansion port on the back face, and the wrist-strap loop under it
    d.box('expansion_port', (0, -2.5, BACK - .1), (20, 6, 1.2), 'dark', bevel=.5)
    d.ring('strap_loop', [(BACK - 3, -8.5), (BACK + 5, -8.5), (BACK + 10, -10.5), (BACK + 11.5, -14.5), (BACK + 10, -18.5),
                          (BACK + 5, -20.5), (BACK - 3, -20.5)],
           [(BACK - 1, -11.0), (BACK + 5, -11.0), (BACK + 8.2, -12.3), (BACK + 9.2, -14.5), (BACK + 8.2, -16.7),
            (BACK + 5, -18.0), (BACK - 1, -18.0)], 3.2, 'dark')
    return d


if __name__ == '__main__':
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from common import run
    run('con-pointer-remote', PARAMETERS)
