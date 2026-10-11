"""Validate bounded private clock records; correlate host clocks, never GPU frames."""
import json
import math
from fractions import Fraction

I64_MIN, I64_MAX, U64_MAX = -(1 << 63), (1 << 63) - 1, (1 << 64) - 1
STOP_REASONS = {"logger-destroyed", "time-limit", "row-limit", "byte-limit",
                "counter-overflow", "header-write-error", "row-write-error", "flush-error"}
CLOCK_FIELDS = {"clock_available", "monotonic_clock", "qpc_unit", "utc_clock", "utc_unit",
                "qpc_frequency_hz", "qpc_before", "qpc_after", "utc_filetime_100ns"}
ROUTE_FIELDS = CLOCK_FIELDS | {"schema", "kind", "elapsed_unit", "anchor_elapsed_unit",
    "logger_elapsed_at_anchor_ns", "sample_interval_frames", "row_limit", "byte_limit",
    "start_seconds", "duration_seconds", "completed_frame_events", "complete_rows",
    "complete_csv_bytes", "stop_elapsed_ms", "stop_reason", "csv_close_ok", "exact_gpu_frame_sync"}

def integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError("Integer outside declared clock/counter range")
    return value

def number(value, low, high):
    if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError("Invalid finite timing value")
    return Fraction(str(value))

def load_route(text):
    if len(text.encode("utf-8")) > 4096:
        raise ValueError("Metadata exceeds single-record byte bound")
    def unique(pairs):
        result = {}
        for key, item in pairs:
            if key in result: raise ValueError("Duplicate metadata field")
            result[key] = item
        return result
    value = json.loads(text, object_pairs_hook=unique)
    validate_route(value)
    return value

def validate_clock(value):
    if value["clock_available"] is not True:
        raise ValueError("Clock correlation unavailable")
    if (value["monotonic_clock"], value["qpc_unit"], value["utc_clock"], value["utc_unit"]) != (
            "windows-qpc", "ticks", "windows-filetime-utc", "100ns-since-1601"):
        raise ValueError("Unsupported clock domain or units")
    frequency = integer(value["qpc_frequency_hz"], 1, I64_MAX)
    before, after = (integer(value[key], I64_MIN, I64_MAX) for key in ("qpc_before", "qpc_after"))
    integer(value["utc_filetime_100ns"], 0, U64_MAX)
    if after < before:
        raise ValueError("Regressing clock bracket")
    return frequency, before, after

def validate_route(value):
    if not isinstance(value, dict) or set(value) != ROUTE_FIELDS:
        raise ValueError("Unexpected metadata fields")
    if type(value["schema"]) is not int or value["schema"] != 1 or value["kind"] != "route-origin-and-stop":
        raise ValueError("Unsupported origin record")
    validate_clock(value)
    if value["elapsed_unit"] != "milliseconds" or value["anchor_elapsed_unit"] != "nanoseconds":
        raise ValueError("Elapsed units mismatch")
    integer(value["logger_elapsed_at_anchor_ns"], 0, I64_MAX)
    integer(value["sample_interval_frames"], 1, 600)
    row_limit = integer(value["row_limit"], 1, 100000)
    integer(value["byte_limit"], 0, 32 * 1024 * 1024)
    rows = integer(value["complete_rows"], 0, row_limit)
    integer(value["complete_csv_bytes"], 0, value["byte_limit"])
    frames = integer(value["completed_frame_events"], 0, U64_MAX)
    delay = number(value["start_seconds"], 0, 600)
    duration = number(value["duration_seconds"], 0, 600)
    stop = number(value["stop_elapsed_ms"], 0, 1e15)
    if duration == 0 or value["stop_reason"] not in STOP_REASONS:
        raise ValueError("Invalid stop declaration")
    if type(value["csv_close_ok"]) is not bool or value["exact_gpu_frame_sync"] is not False:
        raise ValueError("Invalid evidence claim")
    if value["stop_reason"] == "row-limit" and rows != row_limit:
        raise ValueError("Row limit not reached")
    if value["stop_reason"] == "time-limit" and stop / 1000 < delay + duration:
        raise ValueError("Time limit not reached")
    if value["stop_reason"] == "counter-overflow" and frames != U64_MAX:
        raise ValueError("Counter overflow not reached")

def validate_capture(value):
    if not isinstance(value, dict) or set(value) != {"schema", "compositor_unit", "exact_gpu_frame_sync", "frames"} or (
            type(value["schema"]) is not int or value["schema"] != 1 or value["compositor_unit"] != "100ns-qpc" or
            value["exact_gpu_frame_sync"] is not False or type(value["frames"]) is not list or len(value["frames"]) != 2):
        raise ValueError("Unsupported capture clock record")
    for frame in value["frames"]:
        if not isinstance(frame, dict) or set(frame) != CLOCK_FIELDS | {"compositor_time_100ns"}:
            raise ValueError("Unexpected capture clock fields")
        validate_clock(frame)
        integer(frame["compositor_time_100ns"], I64_MIN, I64_MAX)
    if value["frames"][1]["compositor_time_100ns"] < value["frames"][0]["compositor_time_100ns"]:
        raise ValueError("Regressing compositor timestamp")

def route_qpc_seconds(value, elapsed_ms, *, same_host_boot=False):
    """QPC-domain interval for a CSV row, not a capture-frame attribution."""
    validate_route(value)
    if not same_host_boot:
        raise ValueError("Same host boot provenance must be supplied independently")
    frequency, before, after = validate_clock(value)
    elapsed = number(elapsed_ms, 0, 1e15)
    if elapsed > number(value["stop_elapsed_ms"], 0, 1e15):
        raise ValueError("Row beyond recorded stop")
    delta = elapsed / 1000 - Fraction(value["logger_elapsed_at_anchor_ns"], 1_000_000_000)
    return Fraction(before, frequency) + delta, Fraction(after, frequency) + delta

def compositor_qpc_seconds(frame):
    validate_clock(frame)
    return Fraction(integer(frame["compositor_time_100ns"], I64_MIN, I64_MAX), 10_000_000)
