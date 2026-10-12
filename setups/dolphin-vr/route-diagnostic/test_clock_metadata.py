"""Synthetic protocol/clock tests; no app, GPU, game, source checkout or compiler."""
import copy
import json
import unittest
from fractions import Fraction
import clock_metadata as clock

def origin():
    return dict(schema=1, kind="route-origin-and-stop", clock_available=True,
        monotonic_clock="windows-qpc", qpc_unit="ticks", utc_clock="windows-filetime-utc",
        utc_unit="100ns-since-1601", qpc_frequency_hz=10_000_000,
        qpc_before=100_000_000, qpc_after=100_000_020, utc_filetime_100ns=134_000_000_000_000_000,
        elapsed_unit="milliseconds", anchor_elapsed_unit="nanoseconds", logger_elapsed_at_anchor_ns=200_000_000,
        sample_interval_frames=30, row_limit=100000, byte_limit=32*1024*1024, start_seconds=0,
        duration_seconds=600, completed_frame_events=120, complete_rows=40, complete_csv_bytes=24000,
        stop_elapsed_ms=3000, stop_reason="logger-destroyed", csv_close_ok=True, exact_gpu_frame_sync=False)

class ClockMetadataTests(unittest.TestCase):
    def test_roundtrip_and_bounded_host_clock_join(self):
        value=clock.load_route(json.dumps(origin()))
        before,after=clock.route_qpc_seconds(value,1200,same_host_boot=True)
        self.assertLess(before,Fraction(11))
        self.assertGreater(after-before,Fraction(2,1_000_000))
        frame={key:value[key] for key in clock.CLOCK_FIELDS}
        frame["compositor_time_100ns"]=110_000_010
        self.assertTrue(before <= clock.compositor_qpc_seconds(frame) <= after)
        with self.assertRaises(ValueError): clock.route_qpc_seconds(value,1200)

    def test_clock_units_regression_and_unavailable(self):
        for field,replacement in (("qpc_unit","milliseconds"),("qpc_frequency_hz",0),
                ("qpc_before",100_000_021),("clock_available",False),("exact_gpu_frame_sync",True)):
            value=origin();value[field]=replacement
            with self.subTest(field=field),self.assertRaises(ValueError): clock.validate_route(value)

    def test_integer_precision_and_overflow(self):
        value=origin();value.update(qpc_before=clock.I64_MAX-20,qpc_after=clock.I64_MAX)
        decoded=clock.load_route(json.dumps(value))
        self.assertEqual(decoded["qpc_after"],clock.I64_MAX)
        self.assertGreater(clock.route_qpc_seconds(decoded,200,same_host_boot=True)[1],Fraction(clock.I64_MAX,10_000_000))
        for field,bad in (("qpc_after",clock.I64_MAX+1),("utc_filetime_100ns",clock.U64_MAX+1),
                ("completed_frame_events",clock.U64_MAX+1),("complete_rows",True)):
            value=origin();value[field]=bad
            with self.subTest(field=field),self.assertRaises(ValueError): clock.validate_route(value)

    def test_terminal_causes_match_counters(self):
        for reason in clock.STOP_REASONS:
            value=origin();value["stop_reason"]=reason
            if reason=="row-limit": value["complete_rows"]=value["row_limit"]
            if reason=="time-limit": value["stop_elapsed_ms"]=600000
            if reason=="counter-overflow": value["completed_frame_events"]=clock.U64_MAX
            with self.subTest(reason=reason): clock.validate_route(value)
        for reason in ("row-limit","time-limit","counter-overflow","natural-game-exit"):
            value=origin();value["stop_reason"]=reason
            with self.subTest(reason=reason),self.assertRaises(ValueError): clock.validate_route(value)

    def test_bounds_and_private_fields(self):
        for field,bad in (("row_limit",100001),("byte_limit",32*1024*1024+1),
                ("complete_csv_bytes",32*1024*1024+1),("duration_seconds",float("inf")),
                ("sample_interval_frames",0),("private_path","synthetic-only")):
            value=origin();value[field]=bad
            with self.subTest(field=field),self.assertRaises(ValueError): clock.validate_route(value)
        with self.assertRaises(ValueError): clock.load_route(" "*4097)
        with self.assertRaises(ValueError): clock.load_route('{"schema":1,"schema":1}')
        with self.assertRaises(ValueError): clock.route_qpc_seconds(origin(),3001,same_host_boot=True)

    def test_capture_semantics_and_order(self):
        value=origin();frame={key:value[key] for key in clock.CLOCK_FIELDS}
        frame["compositor_time_100ns"]=110_000_001
        second=copy.deepcopy(frame);second["compositor_time_100ns"]+=10_000_000
        capture=dict(schema=1,compositor_unit="100ns-qpc",exact_gpu_frame_sync=False,frames=[frame,second])
        clock.validate_capture(capture)
        self.assertEqual(clock.compositor_qpc_seconds(second)-clock.compositor_qpc_seconds(frame),1)
        second["compositor_time_100ns"]=frame["compositor_time_100ns"]-1
        with self.assertRaises(ValueError): clock.validate_capture(capture)

    def test_csv_rounding_preimages_and_decimal_carries(self):
        pairs=(("239220.175", "239220.17451"), ("239220.175", "239220.17549"),
               ("100000", "99999.99996"), ("100000", "100000.0004"),
               ("1e+06", "999999.9996"), ("1e-06", "0.0000009999999996"))
        for token, actual in pairs:
            with self.subTest(token=token,actual=actual):
                low,high=clock.elapsed_ms_interval(token)
                self.assertLessEqual(low,Fraction(actual));self.assertGreaterEqual(high,Fraction(actual))
        self.assertEqual(clock.elapsed_ms_interval("0"),(0,0))
        for token in ("NA","nan","-1","1e999999","1e-999",True):
            with self.subTest(token=token),self.assertRaises(ValueError): clock.elapsed_ms_interval(token)

    def test_rounded_last_row_and_qpc_event_inclusion(self):
        value=origin();value["stop_elapsed_ms"]=239220.1749
        low,high=clock.route_qpc_seconds(value,"239220.175",same_host_boot=True)
        actual=Fraction(value["qpc_before"],value["qpc_frequency_hz"])+Fraction("239220.17451")/1000-Fraction(200_000_001,1_000_000_000)
        self.assertLessEqual(low,actual);self.assertGreaterEqual(high,actual)
        with self.assertRaises(ValueError): clock.route_qpc_seconds(value,"239220.18",same_host_boot=True)

    def test_near_duration_regression_with_zero_width_anchor(self):
        value=origin();value.update(qpc_after=value["qpc_before"],logger_elapsed_at_anchor_ns=0,stop_elapsed_ms=600000)
        actual=Fraction(value["qpc_before"],value["qpc_frequency_hz"])+Fraction("599999.51234")/1000
        old_nominal=Fraction(value["qpc_before"],value["qpc_frequency_hz"])+Fraction("599999.512")/1000
        self.assertEqual(actual-old_nominal,Fraction(340,1_000_000_000))
        low,high=clock.route_qpc_seconds(value,"599999.512",same_host_boot=True)
        self.assertLessEqual(low,actual);self.assertGreaterEqual(high,actual)

if __name__=="__main__": unittest.main()
