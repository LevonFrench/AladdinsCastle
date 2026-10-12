# R4 native button focus

The private experiment launcher enabled Dolphin background input, but the public
sitting setup applied it only to DSU. Default and Eager native preparations now
set Dolphin.ini `[Input] BackgroundInput=True`; scripted DSU retains that setting.
Desktop keyboard/mouse preparation explicitly sets False, including when reusing
a managed directory that previously selected a controller route.

Pinned DolphinXR 85c4266 defines MAIN_INPUT_BACKGROUND_INPUT as False in
MainSettings.cpp and consumes it in VideoInterface.cpp's input gate and
RenderWidget.cpp's focus policy. This is the build's native setting; no new input
instrumentation or OS keyboard/mouse control is introduced.

All 63 Windows PowerShell 5.1.26100.9549 synthetic checks passed, including both
native heartbeat modes, native-to-desktop focus restoration and desktop DSU.
No app/game/VR run occurred. Controller button response after an actual focus
change remains unverified while GPU use is held. Hub recipe defaults are a
separate Hub-owned change; existing user tuning remains theirs.
