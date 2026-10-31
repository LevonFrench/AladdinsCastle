// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
// The universal controls (docs/controls.md, U1-U4): stick clicks only, the same in every game and host.
// Every game card, controls hint and help text takes its Start / Select / recentre / quit sentence from here,
// so the wording cannot drift between systems. tests/universal_controls_text.py checks that the docs and the
// website carry the same sentence and that no Hub text still says the retired left Y or grip rules.
namespace ac::universal {
// U1 Start, U2 Select or Coin, U3 recentre, U4 quit and save.
inline QString controls(){
 return QStringLiteral("Left stick click = Start. Right stick click = Select or Coin. "
                       "Click both sticks together = put the screen in front of you again. "
                       "Hold both stick clicks for 3 seconds = quit and save.");
}
// Games never use the left Menu or the right Oculus button; the left Menu is how you get back to the Hub.
inline QString menuNote(){
 return QStringLiteral("The left Menu button opens the SteamVR dashboard, where the Hub panel is; games never use it or the right Oculus button.");
}
// Pinball (U1, U2, U3p, U4). The patched AladdinsCastle VPX (setups/vpx-vr; the Hub tells it by its "-acvr" product
// version, pinball/Build.h) has U4 and a hold-only table adjust. Standard VPX cannot time a hold, toggles the table
// adjust on each double click, and the Hub cannot read the controllers while VPX has them, so there the way out is
// the SteamVR dashboard.
inline QString pinballControls(bool patched=false){
 if(patched)
  return QStringLiteral("Left stick click = Start. Right stick click = Coin. A = the plunger button. "
                        "Click both sticks together = align the table to your hands: keep them held and move your hands apart or together to scale it, "
                        "turn the pair to turn it, move both to move it, and let go to keep it. "
                        "Hold both stick clicks for 3 seconds with your hands still = quit and save.");
 return QStringLiteral("Left stick click = Start. Right stick click = Coin. A = the plunger button. "
                       "Click both sticks together = the table follows your hands: move them apart or together to scale it, "
                       "turn the pair to turn it, move both to move it, and click both sticks again to keep it there. "
                       "Holding both stick clicks quits only in the patched AladdinsCastle build of Visual Pinball. "
                       "In this standard Visual Pinball, press the left Menu button, open the Hub panel and choose Exit game.");
}
// A game's own controls text followed by U1-U4. Safe to call twice on the same text.
inline QString with(const QString &own){
 const auto sentence=controls();
 if(own.contains(sentence))return own;
 return own.isEmpty()?sentence:own.trimmed()+QLatin1Char(' ')+sentence;
}
// The same for the now-playing hint: U1-U4, then how to get back to the Hub.
inline QString withHint(const QString &own){
 const auto note=menuNote();
 const auto text=with(own);
 return text.contains(note)?text:text+QLatin1Char(' ')+note;
}
}
