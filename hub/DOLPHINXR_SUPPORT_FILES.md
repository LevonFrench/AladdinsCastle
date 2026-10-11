# Pinned DolphinXR archive support files

The content guard retains its default denial of game-content extensions, known
game/BIOS filenames and denied hashes. Archive inspection has one curated
exception for the nine `Sys/` `.bin` support resources in DolphinXR v0.4.
`data/content-guard.toml` pins each exact archive SHA-256, literal entry path,
member SHA-256 and byte count. Each member matches the committed `Data/Sys/`
resource at source revision `85c4266b29eb308d173fefa0f21f5cb77df3516a`; primary
source links are beside the fingerprints. No upstream payload is stored here.

The exception applies only when Archive computes the actual archive and member
hashes. Source-file, named game/BIOS, archive/member denied-hash, link, traversal,
case-collision, encryption, expansion and corruption checks remain authoritative.
Every entry must pass inspection before staging exists. Extract rechecks the
archive/member proof; copy and ordinary content-guard calls retain `.bin` denial.
Unpinned archives, altered members or paths, malformed/ambiguous pins and byte
count changes refuse. The exception is neither a global extension relaxation
nor an install-recipe switch.

Synthetic regressions exercise exact admission and those refusal boundaries.
An explicit local pinned-release preflight also passes the same Archive API; it
never downloads, installs or launches a runtime. Real recipe install/boot and
headset acceptance remain separate checks owned by the lead.
