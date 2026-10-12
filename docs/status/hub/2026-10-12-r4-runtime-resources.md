# Hub runtime resources

Multi-config builds placed the dashboard thumbnail and manifest above the actual
executable directory. Hub looks beside its executable, so that layout prevented
thumbnail loading. A post-build step now puts both resources beside the actual
target, preserving the single-config layout and portable manifest paths.

The failing-first regression executes the project's actual resource commands
with small native targets: Ninja Release passed; Ninja Multi-Config Release and
Debug both failed on missing adjacent assets. All three pass after the fix,
including byte-exact thumbnail and manifest identity/relative-path checks.
The registered full-project CTest also passed. Initial local fixture runs hit
the known MSVC PDB-manager mismatch before placement assertions; those logs are
preserved separately. The tiny fixture uses embedded debug info, with no change
to production compiler settings.

Review found the extra test initially required Ninja for all testing builds.
A failing-first no-Ninja discovery configuration reproduces that regression;
conditional registration now keeps ordinary configuration working with an
explicit skip message. Existing three resource layouts remain covered when
Ninja is available. This adds no required tool to ordinary Hub configuration.

A bounded built-resource-only correction also put the existing reviewed assets
beside the existing diagnostic executable; binary bytes are unchanged. No runtime,
registration or Steam write followed. This closes a concrete startup prerequisite,
not the remaining no-frame/mirror error or upright/headset acceptance. A later
upright retry requires a fresh serialized graphics booking. Pinball initialization
and normal-close failures remain incomplete; CPU activity is not radiance progress.
