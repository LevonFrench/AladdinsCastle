# R3 DSU server-list correction

Fresh settings now end each DSU server entry with a semicolon. DolphinXR at pinned source 85c4266 reconstructs `name:address:port;` in `DualShockUDPClient.cpp::ConfigChanged`, compares that string with the stored setting and restarts the backend when they differ. The generator, PowerShell sitting helper and fallback profile setup had omitted the terminator. The correction preserves the product name, loopback address and chosen port.

A desktop run reached animated setup instructions but missed its five-second clean-close deadline. Its log showed graphics teardown followed by repeated DSU restarts. That observation motivated this source-grounded correction; whether it resolves runtime shutdown still requires the one diagnosed retry. No timeout was enlarged and no forced cleanup is counted as success.

The PowerShell regression failed before the correction, then all 59 checks passed in Windows PowerShell 5.1. Viewer regressions also failed first and then passed, including exact old-manifest admission and endpoint-tampering refusal. Guns' 31 source checks passed. Only the generated DSU server file and its manifest digest changed; native/button profiles and image bytes are unchanged. The confirmation index refresh records the changed generator source digest.

Previously prepared profiles are preserved. Viewer admits only their exact historical templates, without rewriting them; fresh preparation supplies the fix. Existing run receipts stay frozen. Headset, mission, input-response and performance acceptance remain separate.
