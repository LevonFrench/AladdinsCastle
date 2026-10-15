// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace ac {
class HubServices;
class UiController;
// Operator diagnostic: start one game through the Hub's own launch code, keep it
// running for a while, end it through the Hub's Exit (then Stop if it does not
// end), and write what happened to a JSON file. Never starts SteamVR.
struct LaunchProofOptions {
    QString output;      // JSON written here when the proof ends.
    QString gameId;      // "model3-scud", "dreamcast-<id>", "model2-vcop2", "system22-timecris", "ps2-<id>".
    QString view;        // View to start, without changing the user's remembered choice.
    QString childEnv;    // "NAME=value;NAME=" — set, or remove when empty, for this run only.
    QString childArgs;   // Extra arguments, separated by ';', added after the Hub's own.
    int holdSeconds = 20;
    int exitWaitSeconds = 15;
};
LaunchProofOptions launchProofOptions(); // From AC_LAUNCH_PROOF* variables; output empty when not asked for.
void runLaunchProof(const LaunchProofOptions &options, HubServices &services, UiController &ui);
}
