// license:GPLv3+
// SPDX-License-Identifier: GPL-3.0-or-later
// AladdinsCastle pinball table adjust (universal controls U3p): the table follows the hands only while the
// "Align VR view using controllers" binding (both stick clicks) is held, and stays where it is on release.
//
// VPX's own action toggles "follow the hands" on each press instead, so after a both-clicks chord the table kept
// following the hands until the next chord. apply_acvr.py routes that action through this rule.
#pragma once

namespace acvr {

// The new "table follows the hands" state after the binding changed from was_pressed to is_pressed.
// No change of the binding: keep the current state (it may have been switched off elsewhere, e.g. VPX's head recentre).
inline bool table_follows_hands(bool was_pressed, bool is_pressed, bool following)
{
   return was_pressed == is_pressed ? following : is_pressed;
}

}
