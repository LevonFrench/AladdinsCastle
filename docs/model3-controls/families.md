# Model 3 cabinet families

All 12 catalog titles, grouped from Supermodel input flags; cabinet topology follows the public catalog and existing controls notes. Exact game captions not established by input names remain questions.

| Game | Family | Differences / Supermodel input types |
|---|---|---|
| Scud Race (`scud`) | Four-speed racer | H4 + four view buttons; common, vehicle, shift4, vr4 |
| Daytona USA 2: Battle on the Edge (`daytona2`) | Four-speed racer | H4 + four view buttons; common, vehicle, shift4, vr4 |
| Le Mans 24 (`lemans24`) | Sequential racer | Sequential + four view buttons; common, vehicle, vr4, shiftupdown |
| Dirt Devils (`dirtdvls`) | Sequential racer | Sequential + one view button; no handbrake port; common, vehicle, shiftupdown, viewchange |
| Emergency Call Ambulance (`eca`) | Sequential racer | Sequential + one view button; common, vehicle, shiftupdown, viewchange |
| Sega Rally 2 (`srally2`) | Rally | H4 + handbrake + one view button; common, vehicle, shift4, handbrake, viewchange |
| Harley-Davidson and L.A. Riders (`harley`) | Motorcycle | Steering + throttle/front brake/rear brake + music + view; common, vehicle, shiftupdown, harley, viewchange |
| The Lost World: Jurassic Park (`lostwsga`) | Pistol | Two analog-aim slots; two trigger switches per slot; common, analog_gun1, analog_gun2 |
| L.A. Machineguns (`lamachin`) | Mounted gun | Two analog-aim slots; two trigger switches per slot; common, analog_gun1, analog_gun2 |
| The Ocean Hunter (`oceanhun`) | Mounted gun | Two analog-aim slots; two trigger switches per slot; common, analog_gun1, analog_gun2 |
| Ski Champ (`skichamp`) | Ski platform | Swing/edge axes, pole switches, three selection buttons; common, ski |
| Magical Truck Adventure (`magtruck`) | Handcar pump | Two vertical pump axes and two pedal switches; no wheel port; common, magtruck |

No flight-stick or conventional stick-and-buttons Model 3 title is present in this catalog. No unlisted game or substitute cabinet is added. Magical Truck’s existing catalog wheel label conflicts with the emulator’s explicit pump-lever inputs; this work uses pump levers without editing the catalog.

Sources: `_refs/Supermodel/Src/Inputs/Inputs.cpp` and `Config/Games.xml`, and the catalog’s `games/*/game.toml`. Games.xml is read only for public identity/input types; no ROM names or contents are copied.

D55/D60 apply per-game choices. D65 is not present in the checkout’s workshop table; the owner’s current instruction supplies its rule: offer each body-control choice before launch as an illustrated card with one plain sentence.
