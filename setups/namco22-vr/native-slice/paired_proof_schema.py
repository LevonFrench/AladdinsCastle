"""Exact v1 producer key sets and source-synthetic interface example."""
import copy
import json
from pathlib import Path

ROLES=("markers-pose0","markers-pose1","markers-replay0","cz-clipped")
STAMP_KEYS="scene_frame_id lease_id lease_generation native_frame_counter display_id pair_sequence pose_revision anchor_revision scale_revision target_generation view_from_scene_sha256 projection_from_view_sha256 composition_sha256".split()
EYE_KEYS="eye_index array_layer image_rect draw_rect draw_rect_origin rgb_sha256 view_from_scene view_from_scene_sha256 projection_from_view projection_from_view_sha256 eye_from_head eye_from_head_sha256 entry exit diagnostics".split()
READBACK_KEYS="source source_origin export_origin row_reversal_count pair_sequence target_generation counter_qpc clock_domain clock_frequency predicted_display_time_ns geometry_complete".split()
PREFLIGHT_KEYS="native_before_artifact native_before_sha256 native_after_artifact native_after_sha256 cache_before_artifact cache_before_sha256 cache_after_artifact cache_after_sha256 device_artifact device_sha256".split()
SWAP_KEYS="pair_sequence lease_id scene_frame_id target_generation counter_qpc clock_domain clock_frequency image_rgb_sha256 presentation".split()
RELEASE_KEYS="role_id lease_id lease_generation scene_frame_id native_frame_counter last_pair_sequence counter_qpc clock_domain clock_frequency invalidated".split()
PAIR_KEYS="schema evidence_class run_id role_id case_id capture_kind artifact file_sha256 full_rgb_sha256 width height pixel_format row_origin layout presentation_transform mirrored_x mirrored_y lease_kind scene_frame_id lease_id lease_generation scene_snapshot_sha256 snapshot_artifact resource_artifacts fog_artifacts native_frame_counter display_id pair_sequence pose_revision anchor_revision scale_revision composition_sha256 target_generation target_width target_height scene_units_per_metre scale_provisional head_from_anchor head_from_anchor_sha256 anchor_from_scene anchor_from_scene_sha256 oracle_manifest_sha256 eyes preflight readback compositor_correlation paired_capture_candidate synthetic_stereo game_stereo headset_acceptance".split()
RUN_KEYS="schema evidence_class run_id producer_kind scenario_id capture_kind attempt result pairs release_events source_commit upstream_commit overlay_sha256 sdk_manifest_sha256 exe_sha256 sdl_dll_sha256 source_manifest build_receipt_sha256 contract_sha256 oracle_manifest_sha256 exit_code owned_processes_absent forced_cleanup failure private_launch_receipt_sha256 elapsed_ms".split()


def keysets():
    return dict(schema="acn22-producer-interface/v1",role_order=list(ROLES),run=RUN_KEYS,pair=PAIR_KEYS,
      eye=EYE_KEYS,entry_exit=STAMP_KEYS,preflight=PREFLIGHT_KEYS,readback=READBACK_KEYS,swap=SWAP_KEYS,release=RELEASE_KEYS)


def example():
    # Shape example, deliberately impossible to admit as a native run.
    import paired_proof_protocol as p
    o=p.oracle();h="0"*64;run={k:None for k in RUN_KEYS}
    run.update(schema="acn22-paired-proof/v1",evidence_class="source-synthetic-example",run_id="0"*32,
      producer_kind="system22-native-synthetic",scenario_id="native-batch-paired-v1",capture_kind="producer-gl-backbuffer",
      attempt=1,result="example",pairs=[],release_events=[],source_manifest={},exit_code=None,
      owned_processes_absent=False,forced_cleanup=False,failure="not runtime evidence",elapsed_ms=0)
    records=[];swaps=[];releases=[]
    for seq,role in enumerate(ROLES,1):
        lease=1 if seq<4 else 2;case=0 if seq<4 else 1;phase=1 if seq==2 else 0
        spec=o["markers"]["poses"][phase] if case==0 else o["cz"];es=spec["eyes"] if case==0 else spec["samples"]
        pair={k:None for k in PAIR_KEYS};pair.update(schema=run["schema"],evidence_class=run["evidence_class"],run_id=run["run_id"],role_id=role,
          case_id="markers" if case==0 else "clipped-cz",capture_kind=run["capture_kind"],artifact=f"pair_{seq}.ppm",
          width=1280,height=480,pixel_format="RGB8",row_origin="top-left",layout="sbs-left-right",
          presentation_transform="identity",mirrored_x=False,mirrored_y=False,lease_kind="fixture-immutable-snapshot",
          scene_frame_id=str(lease),lease_id=str(lease),lease_generation=str(lease),native_frame_counter=str(lease),display_id=str(seq),
          pair_sequence=str(seq),pose_revision=str(seq),anchor_revision="0",scale_revision="0",target_generation="1",target_width=1280,target_height=480,
          scene_units_per_metre=1000.0,scale_provisional=True,snapshot_artifact=f"scene_{lease}.snapshot.bin",resource_artifacts=[],fog_artifacts=[],
          head_from_anchor=list(map(p.frozen.f32hex,spec["head_f32"])),anchor_from_scene=list(map(p.frozen.f32hex,o["anchor_f32"])),eyes=[],
          compositor_correlation="unverified",paired_capture_candidate=False,synthetic_stereo="unverified",game_stereo="unverified",headset_acceptance="unverified")
        for key in PAIR_KEYS:
            if key.endswith("sha256"):pair[key]=h
        for ix,e in enumerate(es):
            stamp={k:"1" for k in STAMP_KEYS};stamp.update(scene_frame_id=str(lease),lease_id=str(lease),lease_generation=str(lease),native_frame_counter=str(lease),
              display_id=str(seq),pair_sequence=str(seq),pose_revision=str(seq),anchor_revision="0",scale_revision="0",view_from_scene_sha256=h,projection_from_view_sha256=h,composition_sha256=h)
            eye={k:None for k in EYE_KEYS};eye.update(eye_index=ix,array_layer=0,image_rect=o["rectangles"][ix],draw_rect=o["rectangles"][ix],draw_rect_origin="lower-left",rgb_sha256=h,
              view_from_scene=list(map(p.frozen.f32hex,e["view_f32"])),view_from_scene_sha256=h,projection_from_view=list(map(p.frozen.f32hex,spec["projection_f32"])),projection_from_view_sha256=h,
              eye_from_head=list(map(p.frozen.f32hex,e["eye_from_head_f32"])),eye_from_head_sha256=h,entry=stamp,exit=copy.deepcopy(stamp),diagnostics={"source_example":True})
            pair["eyes"].append(eye)
        pair["readback"]=dict(source="GL_BACK",source_origin="lower-left",export_origin="top-left",row_reversal_count=1,pair_sequence=str(seq),target_generation="1",
          counter_qpc="0",clock_domain="producer-qpc",clock_frequency="1",predicted_display_time_ns=None,geometry_complete=False)
        pair["preflight"]={key:h if key.endswith("sha256") else f"example-{key}.json" for key in PREFLIGHT_KEYS}
        records.append(pair);run["pairs"].append(dict(artifact=f"pair_{seq}.receipt.json",sha256=h,swap_artifact=f"pair_{seq}.swap.json",swap_sha256=h))
        swaps.append(dict(pair_sequence=str(seq),lease_id=str(lease),scene_frame_id=str(lease),target_generation="1",counter_qpc="0",clock_domain="producer-qpc",clock_frequency="1",image_rgb_sha256=h,presentation="swap-requested"))
    for lease,seq,role in ((1,3,"release-markers"),(2,4,"release-cz")):
        releases.append(dict(role_id=role,lease_id=str(lease),lease_generation=str(lease),scene_frame_id=str(lease),native_frame_counter=str(lease),last_pair_sequence=str(seq),counter_qpc="0",clock_domain="producer-qpc",clock_frequency="1",invalidated=True))
        run["release_events"].append(dict(artifact=f"release_{lease}.json",sha256=h))
    return dict(evidence_class=run["evidence_class"],images_created=False,run=run,pairs=records,swaps=swaps,releases=releases)


def check_example(value):
    assert set(value["run"])==set(RUN_KEYS)
    for pair in value["pairs"]:
        assert set(pair)==set(PAIR_KEYS) and set(pair["readback"])==set(READBACK_KEYS) and set(pair["preflight"])==set(PREFLIGHT_KEYS)
        for e in pair["eyes"]:
            assert set(e)==set(EYE_KEYS) and set(e["entry"])==set(STAMP_KEYS) and set(e["exit"])==set(STAMP_KEYS)
    for swap in value["swaps"]:assert set(swap)==set(SWAP_KEYS)
    for release in value["releases"]:assert set(release)==set(RELEASE_KEYS)


if __name__=="__main__":
    value=example();check_example(value);base=Path(__file__).parent
    for name,data in (("producer-interface.json",keysets()),("producer-source-example.json",value)):
        (base/name).write_bytes((json.dumps(data,sort_keys=True,indent=2)+"\n").encode("ascii"))
