# VPX write isolation: launch stays unavailable

Read-only audit of official tag `v10.8.1-5436-af26b2d93`, commit
`af26b2d9392b4c9ec3e3edff4bbd69562085a44c`. No VPX execution, build,
download, collection access or registry inspection occurred. Supported options
isolate some outputs; they do not confine every write. The Hub's diagnostic
plans remain `launchAllowed=false` in desktop and VR modes, including tables
classified VR-ready and needing no ROM.

| Sink | Supported option or restriction | Unresolved condition |
|---|---|---|
| Main/table settings | Owned `-Ini` and existing `-TableIni` select their settings files. | They do not relocate preferences, table data or component outputs. |
| Preferences/log | The actual parser supports `-PrefPath`, despite its omission from the text command-line document. Later logs use the selected preferences path. | `VPApp` constructs `FileLocator` before parsing arguments. Default preference creation/migration can already happen. |
| Texture cache | Effective `[Player] CacheMode=0` suppresses the texture-cache path. | Other table data is unaffected; changing the working directory does not change table-relative resolution. |
| Scores/user data | `SaveValue` and `UserDirectory` use FileLocator's User path. | Normal layout creates `user` beside the table; even `LoadValue` opens structured storage read/write. AppOnly uses the executable directory and is chosen by actual INI placement. No independent owned User-root switch is proved. |
| Tables/scripts/autosave/capture | Direct `-Play` avoids the Win32 editor save/autosave path; capture and export commands are excluded. | Exit scripts still run. Attract capture explicitly writes table-parent/Capture. External script/COM writes remain unconfined. |
| Plugins/ROM tools | Per-plugin `Enable=false` skips loading that plugin. `PinMAMEPath` supplies a component root. | Disabling required components may break a table. PinMAME's root combines ROM and nvram locations; an independent read-only ROM/writable persistence split is unproved. Other COM/plugin outputs require separate evidence. |
| Registry/persistence | Direct Play avoids the editor docking-registry save. | Script object-safety checks are not file/registry isolation and can request manual approval. No ordinary launch option proves confinement of external components. |

Primary source citations, all at the immutable audited commit:

- [Construction before command parsing](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/main.cpp#L194), [default preference setup/migration](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/FileLocator.cpp#L35), [PrefPath handling](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/AppCommands.cpp#L577), [log destination](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/utils/Logger.cpp#L167).
- [TableIni selection](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/AppCommands.cpp#L43), [settings write/remove](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/LayeredINIPropertyStore.cpp#L208), [table/AppOnly layouts](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/FileLocator.cpp#L267), [UserDirectory](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/ScriptGlobalTable.cpp#L370), [SaveValue](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/ScriptGlobalTable.cpp#L505), [LoadValue](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/ScriptGlobalTable.cpp#L581).
- [CacheMode definition](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/Settings_properties.inl#L198), [exit scripts/settings/cache](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/player.cpp#L867), [capture output](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/player.cpp#L1791), [external script save](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/parts/pintable.cpp#L1378).
- [Plugin enable admission](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/player.cpp#L160), [PinMAME common root](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/plugins/pinmame/PinMAMEPlugin.cpp#L213), [script object policy](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/core/ScriptInterpreter.cpp#L493), [editor registry save](https://github.com/vpinball/vpinball/blob/af26b2d9392b4c9ec3e3edff4bbd69562085a44c/src/ui/win/WinEditor.cpp#L1420).

No bounded live-launch plan is supplied: the startup preferences, table user
storage and external script/COM/component sinks are still unresolved. No
collection copy, ACL/registry change, installation edit or engine patch is
proposed. Future launch admission needs evidence for every writable sink.
The read-only scan and existing now-playing behavior are unchanged.

`launchGuidance()` supplies one reason, three stable blocker IDs and plain help
to both diagnostic JSON and the detail page. It checks no additional files and
performs no settings or process action. Choosing VPX, marking VR-ready or
changing the requested display mode cannot remove those blockers.
