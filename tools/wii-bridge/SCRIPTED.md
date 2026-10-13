# Scripted DSU input

For a supervised performance attempt, pass a fresh `--session-token` of 32
lowercase hexadecimal characters. The ready line and final receipt include
`producer_pid`, the SHA-256 of the script bytes actually read, and `session_token`.
The supervisor must match all three and the port to its fresh input attachment
and live owned process; `ready: true` alone is not sufficient. Ordinary bounded
DSU runs remain compatible without a token, and `--check` requires no token.

`scripted.py` sends controller packets through the existing loopback server. It does not send OS input, start a game or initialize VR. Use the canonical generated `flat-dolphin/split-right.ini` profile, including when DolphinXR renders a native VR scene: Source 1 selects emulated DSU input independently of the renderer. Slot 0 is the remote; slot 1 is the Nunchuk. The sitting runner accepts `-DsuProfile` and `-DsuPort` and enables background controller input.

The JSON schema has `version: 1`, `seconds` (0.1–1800), and ordered `steps`. Each step has `at` and exactly two `pads`. A pad accepts `buttons` (DSU names), `pointer` and `stick` (right/up, -1..1), `accel` (g), and `gyro` (degrees/second). Omitted fields return to neutral at each step. Hold a state until the next step; always put an explicit release after a button press. The canonical generated profile maps Cross to Wii A, R2 to B, Options to Plus, and slot-1 L2 to Nunchuk Z. This tool does not claim those are each game's selected actions.

```json
{"version":1,"seconds":3,"steps":[
  {"at":0,"pads":[{},{}]},
  {"at":1,"pads":[{"buttons":["Cross"]},{}]},
  {"at":1.2,"pads":[{},{}]}
]}
```

Invoke with `python tools/wii-bridge/scripted.py --script <absolute-script> --receipt <fresh-absolute-path-under-.local>`. Use `--check` to validate without a socket. The service prints a ready record, then starts its timeline when DSU slot 0 subscribes. With no client before `--wait-for-client` (default 30 seconds) it exits 2. Each run is bounded and releases buttons before closing. Exit 0 proves packet service completion only; a game frame/log must prove prompt or mission progress. Receipt output is exclusive-create and includes script hash and packet/subscription counts. Keep game-specific sequences and captures private.

Never run alongside the live bridge on the same port. Start the DSU service before Dolphin and supervise both owned processes. Stop a run at a system prompt; no security dialog automation is provided. Changing states is designed for scripted desktop observations, not performance measurement until the run and capture overhead are controlled.
