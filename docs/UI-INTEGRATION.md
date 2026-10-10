# QML UI integration (M1 lane D)

The production front end is `hub/qml/HubRoot.qml`. `DesktopShell.qml` owns the normal window; lane B can render the same HubRoot with `vrOverlayMode: true`. The C++ Theme singleton exposes every nested theme token with `Theme.get(path)` and shared colour calculations; QML never parses TOML. The `hubui` static library shares the exact same compiled QML between the desktop executable and Qt Test.

## Service connection contract

`ac::UiController(GameListModel*, FilterSortModel*, UiSettings*)` is a GUI-thread object. QML context names are `gameModel`, `gameFilter`, `uiController`, `uiSettings` and `catalogGameCount`. The models remain lane C's authority.

UI requests are value signals:

- `scanRequested(QStringList roots)` and `cancelScanRequested()`.
- `installRequested(QString gameId, QString variantId)`, `cancelInstallRequested()`, `retryInstallRequested(bool fromStart, QString handover)`, `promptAnswered(bool proceed)` and `skipStepRequested()`.
- `playRequested(QString gameId, QString variantId)`.
- `uninstallPreviewRequested(QString gameId, QString variantId)`: the service must return a manifest-based preview and own the final confirmation/removal. The UI cannot delete anything.
- `locationRequested(QString kind)`: media, tool, runtime, log, install, downloads or steam-shortcut. The integration layer supplies the picker or local folder action.
- `writeConfigRequested(QString gameId, QVariantMap settings)`: emitted only after a portable profile save; reapply config, without reinstalling.

Service callbacks are public slots: `scanStarted()`, `scanProgress(QVariantMap)`, `scanFinished(bool success)`, `installEvent(QVariantMap)`, `installFinished(bool success, QString message)`, `launchStarted(QString gameId)`, `launchFinished(QString gameId, QString error)` and `applyRuntimeStates(QVector<RuntimeState>)`.

Install events use `kind` = step / ok / warn / fail / work / detail / prompt / done and `text`. Step/failure events also accept `step` and `total`; recovery accepts `canSkip` and `downloadUrl`. The UI formats the same markers for its console and Copy log. Jobs stop between steps via service cancellation. Only service-provided complete runtime snapshots may prove an install; a done event or success callback never marks a game installed. Preserve the other services' snapshot fields when composing updates. Queued C model updates retain their 80 ms coalescing.

The UI initially requests services without simulating completion. Tests alone supply synthetic install events and synthetic roots. Authored VR/Quest variants remain inspectable, with clearly disabled M1 actions; the default selection is a generated flat emulator route when one exists.

## Art and privacy

Lane F registers `engine.addImageProvider("art", ...)`, then calls `uiController.setArtProviderReady(true)`. Forward ScanController art revisions through `uiController.setArtRevision(int)`; GameArt appends `?v=<revision>` only to art-provider URLs, preserving reuse within a revision while invalidating generated-fallback cache entries after a scan. A synthetic owned-art regression checks the effective URL changes. The roles already supply `image://art/<game-id>/<kind>`. Before provider readiness, the cards/hero/banner draw original abstract fallback art inline. Failed or missing provider images retain that fallback. HTTP/HTTPS image URLs are ignored. README inline images are removed; reference/shortcut image syntax is converted to explicit links, and HTML image tags are removed before rendering. Synthetic regressions cover every form. Link clicks accept only HTTP/HTTPS; they never auto-open. No game art, owner folders, game media or library scan results are compiled or committed.

UiSettings writes atomically to the portable executable's `user/hub-settings.toml`, with `[ui]` and `[game.<id>]` tables. Unknown stored keys survive. UI roots/settings values and test captures remain private local state.

## Component map and practical resolutions

Implemented shell/header/search/sort/help, live facets and hardware tree, S/M/L, featured banner, virtualized section rows, portrait library, Explore buckets, recent games, detail/variants/requirements/actions/settings/controls/README/similar/components/uninstall preview request, install console/recovery, keyboard navigation/focus and overlay target sizing. No engine, scanner or installer is duplicated in the UI.

The sectioned list uses a virtualized ListView of rows; the portrait library uses GridView. QML row projections rebuild when proxy results or runtime values change, while the source model is never reset. This is the simpler M1 alternative to an additional persistent-index GridRowModel. Deferred hover/focus previews use 600 ms and 1.4 scale, suppress while scrolling and for 120 ms after movement; the preview lives above content. Every action is available by click and keyboard without hovering.

The reference's random banner-effect pool, Classic style, gems, PC Power and trailers remain excluded by component spec 04. Decoration is deliberately restrained. The permitted cheaper heading fallback replaces the mottled per-title gradient mask; cheaper glow is the default, with lazy effects and preview-only card layers. Hide controls are always available instead of hover-only close controls. README content uses Qt's Markdown renderer, with hand-run How to use sections hidden to avoid conflicting with service actions. Unknown/missing setup documentation renders a useful fallback. Native file/confirm dialogs and runtime/Steam/update operations are owned by their service lanes and do not happen automatically here.

## Verification

`hub-ui` is a CI CTest target. It loads every shipped QML component, exercises each facet family, renders all 413 catalog detail projections, tests search/focus/overlay hit sizing, portable settings, console grammar, retry signals and the separation between service success and installed proof. Headed S2 timing/capture is opt-in through `AC_UI_BENCH_OUTPUT` and Qt Test's `selfBenchmark` function; it uses QQuickWindow frameSwapped/grabWindow, not desktop automation. Receipts and exact local build commands live only in the gitignored lane-local folder. See `docs/spikes/s2-grid.md` for measured performance and its limits.
