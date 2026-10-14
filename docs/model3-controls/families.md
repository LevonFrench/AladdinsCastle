# Model 3 cabinet families

All 17 catalog titles, grouped from Supermodel input flags; cabinet topology follows the public catalog and existing controls notes. Exact game captions not established by input names remain questions.

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
| Star Wars Trilogy Arcade (`swtrilgy`) | Flight stick | AnalogJoyX/Y, trigger and Event; common, analog_joystick |
| Virtua Fighter 3 (`vf3`) | Arcade stick + four buttons | Punch, Kick, Guard, Escape; Start during a fight selects first-person view; common, joystick1, joystick2, fighting |
| Fighting Vipers 2 (`fvipers2`) | Arcade stick + three buttons | Punch, Kick, Guard; common, joystick1, joystick2, fighting |
| Spikeout (`spikeout`) | Arcade stick + four buttons | Beat, Charge, Jump, Shift; one player per linked cabinet; common, joystick1, spikeout |
| Virtua Striker 2 (`vs2`) | Arcade stick + three buttons | ShortPass, LongPass, Shoot; common, joystick1, joystick2, soccer |

The five owner-requested flight-stick and arcade-stick titles were added on 2026-10-13. Magical Truck’s existing catalog wheel label conflicts with the emulator’s explicit pump-lever inputs; this work uses pump levers without editing that catalog label. Its GLB is one shared two-player assembly, not one complete copy per player.

Sources: `_refs/Supermodel/Src/Inputs/Inputs.cpp` and `Config/Games.xml`, and the catalog’s `games/*/game.toml`. Games.xml is read only for public identity/input types; no ROM names or contents are copied.

D55/D60 apply per-game choices. D65 is not present in the checkout’s workshop table; the owner’s current instruction supplies its rule: offer each body-control choice before launch as an illustrated card with one plain sentence.
