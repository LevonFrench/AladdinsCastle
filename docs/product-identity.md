# Product identity

The canonical identity is `data/product.json`, relative to the package root.
It is build input and is also shipped with the portable data. Read it rather
than spelling a product name in code, tests, scripts or templates. The current
values preserve existing application, storage and integration identities.

Schema 1 fields:

| Field | Consumer |
| --- | --- |
| `display_name` | Window titles, visible labels, help and OpenXR application name |
| `short_id` | Machine-readable product identifier |
| `program_name` | Hub executable basename, without a platform extension |
| `overlay_key` | SteamVR application manifest and dashboard registration |
| `steam_shortcut_name` | Product-owned Steam shortcut name and ownership tag prefix |
| `user_data_folder` | Product directory beneath the platform's user-data root |
| `dsu_server_name` | DSU endpoint label and generated controller device paths |

Build systems may generate headers, QML constants and application manifests
from this file. Generated files must not become a second editable definition.
Python and PowerShell consumers may parse this small JSON file directly. Tests
read the same definition; they test relationships instead of duplicating names.
The name is not an authority for admitting a process or changing a file: retain
executable hashes, ownership markers, path guards and process identity checks.

Changing a storage folder, device name, overlay key or Steam ownership tag is
a migration decision. A future release must explicitly handle old identities;
editing this file must not silently move data, register an overlay or rewrite
Steam files. Historical reports and repository URLs remain historical facts.
