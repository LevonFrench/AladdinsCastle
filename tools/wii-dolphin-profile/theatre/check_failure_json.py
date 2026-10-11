"""Run a pure CPU fixture and independently parse its private-free JSON output."""
import json
import subprocess
import sys

result = subprocess.run([sys.argv[1]], check=True, capture_output=True, text=True, timeout=10)
data = json.loads(result.stdout)
assert data["schema"] == "ac-wgc-failure-diagnostic/1"
assert data["accepted"] is False
assert data["stage"] == "compare-change" and data["deadline_kind"] == "comparison"
assert data["bounds"] == {"startup_seconds": 40, "comparison_seconds": 15}
assert data["error"] == 'failure "quoted"\n\\end'
assert data["counts"]["raw_frames"] == 205 and data["counts"]["spatial_rejections"] == 203
assert data["counts"]["spatial_candidates"] == 2
assert data["compositor_time_ms"] == {"first": 0, "last": 1399, "min": 0, "max": 1399}
assert [sample["file"] for sample in data["samples"]] == ["first-candidate.png", "latest-raw.png"]
assert all(sample["accepted"] is False for sample in data["samples"])
assert data["selection"]["pid"] == "12" and data["selection"]["hwnd"] == "99"
print("CPU failure diagnostic counters, gate boundaries, retention and JSON passed; no capture.")
