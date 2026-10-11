"""Reviewed-run controller. Import/tests never start a native process or GL."""
import argparse
import json
from pathlib import Path
import queue
import secrets
import subprocess
import threading
import time

import paired_proof_protocol as p
import paired_proof_schema as schema

PIN="a9f1a0a1f9de62387a7138006fa36933fd477123"


def no_links(path):
    for part in [path,*path.parents]:
        if part.exists() and (part.is_symlink() or getattr(part,"is_junction",lambda:False)()):
            raise ValueError("linked private output/input forbidden")


def put(path,data):
    with path.open("xb") as f:f.write(data)


def json_bytes(value):
    return (json.dumps(value,sort_keys=True,indent=2,allow_nan=False)+"\n").encode("utf-8")


def read_json(root,name):
    path=root/name;no_links(path)
    if path.stat().st_size>1024*1024:raise ValueError("metadata bound")
    return p.strict_json(path.read_text())


def read(root,name,size=None):
    path=root/name;no_links(path)
    limit=size if size is not None else 1024*1024
    if path.stat().st_size>limit:raise ValueError("artifact size bound: "+name)
    data=path.read_bytes()
    if size is not None and len(data)!=size:raise ValueError("artifact length: "+name)
    return data


def pair_record(root,run_id,meta,composition_sha,snapshot_name,rgb,diagnostics):
    seq=meta["sequence"];image=f"pair_{seq}.ppm"
    ppm=b"P6\n1280 480\n255\n"+rgb
    eye_records=[]
    for i,eye in enumerate(meta["eyes"]):
        def matrix_sha(words):return p.sha(b"".join(int(x,16).to_bytes(4,"little") for x in words))
        def stamp(raw):
            if raw["lease"]!=meta["lease"] or raw["frame"]!=meta["frame"] or raw["native_frame_counter"]!=meta["native_frame_counter"] or raw["sequence"]!=seq or raw["target_generation"]!=meta["target_generation"]:
                raise ValueError("actual entry/exit identity drift")
            if raw["view_f32"]!=eye["view_f32"] or raw["projection_f32"]!=meta["projection_f32"]:
                raise ValueError("actual entry/exit matrix drift")
            return dict(scene_frame_id=str(raw["frame"]),lease_id=str(raw["lease"]),lease_generation=str(raw["lease"]),
              native_frame_counter=str(raw["native_frame_counter"]),display_id=str(raw["sequence"]),pair_sequence=str(raw["sequence"]),
              pose_revision=str(raw["sequence"]),anchor_revision="0",scale_revision="0",target_generation=str(raw["target_generation"]),
              view_from_scene_sha256=matrix_sha(raw["view_f32"]),projection_from_view_sha256=matrix_sha(raw["projection_f32"]),composition_sha256=composition_sha)
        eye_records.append(dict(eye_index=i,array_layer=0,image_rect=p.oracle()["rectangles"][i],
          draw_rect=eye["rect"],draw_rect_origin="lower-left",rgb_sha256=p.sha(p.eye_bytes(rgb,i)),
          view_from_scene=list(map(p.frozen.f32hex,eye["view_f32"])),
          view_from_scene_sha256=matrix_sha(eye["view_f32"]),
          projection_from_view=list(map(p.frozen.f32hex,meta["projection_f32"])),
          projection_from_view_sha256=matrix_sha(meta["projection_f32"]),
          eye_from_head=list(map(p.frozen.f32hex,eye["eye_from_head_f32"])),
          eye_from_head_sha256=matrix_sha(eye["eye_from_head_f32"]),entry=stamp(eye["entry_state"]),exit=stamp(eye["exit_state"]),diagnostics=diagnostics[i]))
    record=dict(schema="acn22-paired-proof/v1",evidence_class="producer-native-run",run_id=run_id,role_id=schema.ROLES[seq-1],case_id="markers" if meta["case"]==0 else "clipped-cz",
      capture_kind="producer-gl-backbuffer",artifact=image,file_sha256=p.sha(ppm),full_rgb_sha256=p.sha(rgb),
      width=1280,height=480,pixel_format="RGB8",row_origin="top-left",layout="sbs-left-right",
      presentation_transform="identity",mirrored_x=False,mirrored_y=False,
      lease_kind="fixture-immutable-snapshot",scene_frame_id=str(meta["frame"]),lease_id=str(meta["lease"]),
      lease_generation=str(meta["lease"]),scene_snapshot_sha256=p.sha(read(root,snapshot_name)),snapshot_artifact=snapshot_name,
      native_frame_counter=str(meta["native_frame_counter"]),display_id=str(seq),pair_sequence=str(seq),
      pose_revision=str(seq),anchor_revision="0",scale_revision="0",composition_sha256=composition_sha,
      target_generation="1",target_width=1280,target_height=480,scene_units_per_metre=1000.0,scale_provisional=True,
      head_from_anchor=list(map(p.frozen.f32hex,meta["head_f32"])),anchor_from_scene=list(map(p.frozen.f32hex,meta["anchor_f32"])),
      head_from_anchor_sha256=matrix_sha(meta["head_f32"]),anchor_from_scene_sha256=matrix_sha(meta["anchor_f32"]),
      oracle_manifest_sha256=p.ORACLE_SHA,eyes=eye_records,
      resource_artifacts=[],fog_artifacts=[],
      preflight={key:p.sha(read(root,f"pair_{seq}."+suffix)) if key.endswith("sha256") else f"pair_{seq}."+suffix
        for key,suffix in (("native_before_artifact","before.json"),("native_before_sha256","before.json"),
          ("native_after_artifact","after.json"),("native_after_sha256","after.json"),
          ("cache_before_artifact","cache_before.json"),("cache_before_sha256","cache_before.json"),
          ("cache_after_artifact","cache_after.json"),("cache_after_sha256","cache_after.json"))} |
        dict(device_artifact="device.json",device_sha256=p.sha(read(root,"device.json"))),
      readback=dict(source="GL_BACK",source_origin="lower-left",export_origin="top-left",row_reversal_count=1,pair_sequence=str(seq),target_generation="1",
        counter_qpc=str(meta["readback_qpc"]),clock_domain="producer-qpc",clock_frequency=str(meta["qpc_frequency"]),predicted_display_time_ns=None,geometry_complete=True),compositor_correlation="unverified",paired_capture_candidate=True,
      synthetic_stereo="marker-oracle" if meta["case"]==0 else "CZ-pixel-witness",game_stereo="unverified",headset_acceptance="unverified")
    put(root/image,ppm)
    return record


def execute(args):
    # CLI confirmation is an explicit operator switch, not a grant of owner authority.
    if not args.reviewed_run:raise ValueError("reviewed coordinated run switch required")
    here=Path(__file__).resolve().parent;workspace=here.parents[2]
    requested=Path(args.output)
    if not requested.is_absolute():raise ValueError("absolute output required")
    no_links(requested);root=requested.resolve()
    if not root.is_relative_to(workspace/".local") or root.exists() or not root.parent.is_dir():
        raise ValueError("fresh absolute private workspace output required")
    requested_exe=Path(args.exe).absolute();no_links(requested_exe);exe=requested_exe.resolve()
    requested_receipt=Path(args.build_receipt).absolute();no_links(requested_receipt);receipt_path=requested_receipt.resolve()
    if not exe.is_relative_to(workspace/".local") or not receipt_path.is_relative_to(workspace/".local"):
        raise ValueError("proof executable/receipt must belong to this private worktree")
    receipt_raw=receipt_path.read_bytes()
    if p.sha(receipt_raw)!=args.build_receipt_sha256:raise ValueError("reviewed build receipt drift")
    build=p.strict_json(receipt_raw.decode("utf-8"))
    required=("source_commit","upstream_commit","overlay_sha256","sdk_manifest_sha256","exe_sha256","sdl_dll_sha256","source_manifest")
    if any(k not in build for k in required) or build.get("proof_only") is not True or build["upstream_commit"]!=PIN:
        raise ValueError("reviewed proof build provenance required")
    if exe.name.lower()!="n22_native_batch_proof.exe" or p.sha(exe.read_bytes())!=build["exe_sha256"]:
        raise ValueError("native executable identity drift")
    source_names=("paired_native.c","paired_oracle.h","paired_proof.py","paired_proof_protocol.py",
      "paired_proof_patch.py","paired_proof_support.h","native_batch.c","native_batch.h","native_patch.py",
      "pose_stage.c","pose_stage.h","CMakeLists.txt","prepare.py","source-hashes.json","oracle-manifest.json",
      "check_scene_vectors.py","check_composition_vectors.py","PAIRED-EVIDENCE-CONTRACT.md","SNAPSHOT-ORACLE-FREEZE.md",
      "paired_proof_schema.py","producer-interface.json","PAIRED-PRODUCER-INTERFACE.md")
    if set(build["source_manifest"])!=set(source_names):raise ValueError("reviewed source manifest coverage")
    for name in source_names:
        if p.sha((here/name).read_bytes())!=build["source_manifest"][name]:raise ValueError("source drift: "+name)
    model=p.oracle();root.mkdir();run_id=secrets.token_hex(16);tx=p.Transaction();events=queue.Queue()
    manifest=dict(schema="acn22-paired-proof/v1",evidence_class="producer-native-run",run_id=run_id,producer_kind="system22-native-synthetic",
      scenario_id="native-batch-paired-v1",capture_kind="producer-gl-backbuffer",attempt=1,result="failed",pairs=[],release_events=[],
      **{k:build[k] for k in required},build_receipt_sha256=args.build_receipt_sha256,
      contract_sha256=p.sha((here/"PAIRED-EVIDENCE-CONTRACT.md").read_bytes()),oracle_manifest_sha256=p.ORACLE_SHA,
      exit_code=None,owned_processes_absent=False,forced_cleanup=False,failure=None,private_launch_receipt_sha256=None,elapsed_ms=0)
    proc=None;stderr=None;start=time.monotonic();resources=None;snapshots={};images=[];failure=None
    def pump():
        try:
            for line in proc.stdout:
                if len(line)>4096:raise ValueError("native line bound")
                if line.startswith("ACN22/1 "):events.put(line.strip().split())
        except Exception as ex:events.put(ex)
        finally:events.put(None)
    def event(kind,sequence):
        remaining=15-(time.monotonic()-start)
        if remaining<=0:raise ValueError("controller run deadline")
        e=events.get(timeout=remaining)
        if isinstance(e,Exception):raise e
        if e!=["ACN22/1",kind,str(sequence)]:raise ValueError("native protocol order: "+str(e))
    def send(line):proc.stdin.write(line+"\n");proc.stdin.flush()
    try:
        stderr=(root/"native.stderr.txt").open("xb")
        proc=subprocess.Popen([str(exe),"--paired-proof",str(root)],cwd=exe.parent,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
          stderr=stderr,text=True,encoding="ascii",bufsize=1,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        threading.Thread(target=pump,daemon=True).start()
        for seq,(case,phase,lease) in enumerate(((0,0,1),(0,1,1),(0,0,1),(1,0,2)),1):
            event("PREFLIGHT",seq)
            device=read_json(root,"device.json")
            if device["pid"]!=proc.pid or device["width"]!=1280 or device["height"]!=480 or device["color_bits"]!=[8]*4:
                raise ValueError("owned target identity/format")
            dll=Path(read(root,"loaded_sdl_path.txt").decode("ascii"));no_links(dll)
            if p.sha(dll.read_bytes())!=build["sdl_dll_sha256"]:raise ValueError("actual loaded SDL drift")
            resources=[read(root,n,s) for n,s in zip(("palette.bin","texture.bin","tilemap.bin"),p.SIZES)]
            fog=read(root,"fog.bin",8192)
            raw=read(root,f"scene_{lease}.unhashed")
            snapshot=p.fill_snapshot(raw,resources,fog);name=f"scene_{lease}.snapshot.bin"
            p.scene_valid(raw,resources,fog,model,case,lease)
            if lease not in snapshots:
                put(root/name,snapshot);snapshots[lease]=snapshot
            elif snapshots[lease]!=snapshot:raise ValueError("same lease snapshot drift")
            before=read_json(root,f"pair_{seq}.before.json")
            composition_sha=p.pair_inputs(before,model,case,phase,lease,seq)
            if case==1:
                released=read_json(root,"release_1.json")
                if set(released)!=set(schema.RELEASE_KEYS) or released["lease_id"]!="1" or released["native_frame_counter"]!="1" or released["invalidated"] is not True:
                    raise ValueError("CZ acquisition preceded marker release")
            cache=read_json(root,f"pair_{seq}.cache_before.json");p.cache_valid(cache)
            expected_groups={1,2} if case==0 else {1,2,6}
            if {e["pal_group"] for e in cache["entries"]} != expected_groups:
                raise ValueError("missing/unexpected primed material")
            tx.preflight(seq);send(f"DRAW {seq} {p.sha(snapshot)} {composition_sha}");tx.draw()
            event("IMAGE",seq)
            after=read_json(root,f"pair_{seq}.after.json")
            if p.pair_inputs(after,model,case,phase,lease,seq)!=composition_sha or after["pid"]!=proc.pid or after["readback_qpc"]<=0 or after["qpc_frequency"]<=0:
                raise ValueError("postdraw correlation drift")
            if after["bound_snapshot_sha256"]!=p.sha(snapshot) or after["bound_composition_sha256"]!=composition_sha or any(e["entry_frame"]!=lease or e["exit_frame"]!=lease for e in after["eyes"]):
                raise ValueError("actual eye emission/source binding")
            if read(root,f"pair_{seq}.after.unhashed")!=raw:raise ValueError("native consumed snapshot changed")
            for n,s,blob in zip(("palette.bin","texture.bin","tilemap.bin"),p.SIZES,resources):
                if read(root,n,s)!=blob:raise ValueError("resource artifact changed")
            p.cache_valid(read_json(root,f"pair_{seq}.cache_after.json"),cache)
            rgb=read(root,f"pair_{seq}.rgb",1280*480*3);diagnostics=p.pixels(rgb,model,case,phase)
            if seq==2:p.temporal(images[0],rgb,model)
            if seq==3 and images[0]!=rgb:raise ValueError("same-pose pixel replay drift")
            images.append(rgb);record=pair_record(root,run_id,after,composition_sha,name,rgb,diagnostics)
            record["resource_artifacts"]=[dict(id=i+1,artifact=n,byte_count=s,sha256=p.sha(blob)) for i,(n,s,blob) in enumerate(zip(("palette.bin","texture.bin","tilemap.bin"),p.SIZES,resources))]
            if case==1:record["fog_artifacts"]=[dict(quad_index=0,artifact="fog.bin",byte_count=8192,sha256=p.sha(fog))]
            paired_name=f"pair_{seq}.receipt.json";data=json_bytes(record);put(root/paired_name,data)
            if set(record)!=set(schema.PAIR_KEYS) or any(set(e)!=set(schema.EYE_KEYS) for e in record["eyes"]):raise ValueError("producer schema drift")
            tx.validated();send(f"SWAP {seq} {p.sha(rgb)}")
            event("SWAPPED",seq);swap=read_json(root,f"pair_{seq}.swap.json")
            if set(swap)!=set(schema.SWAP_KEYS) or swap["pair_sequence"]!=str(seq) or swap["image_rgb_sha256"]!=p.sha(rgb) or swap["presentation"]!="swap-requested" or swap["lease_id"]!=str(lease) or swap["scene_frame_id"]!=str(lease):raise ValueError("swap mapping")
            tx.swap();manifest["pairs"].append(dict(artifact=paired_name,sha256=p.sha(data),swap_artifact=f"pair_{seq}.swap.json",swap_sha256=p.sha(read(root,f"pair_{seq}.swap.json"))))
        event("EXIT",0);code=proc.wait(timeout=max(.1,15-(time.monotonic()-start)))
        if code!=0:raise ValueError("native cleanup failed")
        for lease_id,last,role in ((1,3,"release-markers"),(2,4,"release-cz")):
            release=read_json(root,f"release_{lease_id}.json")
            if set(release)!=set(schema.RELEASE_KEYS) or release["role_id"]!=role or release["lease_id"]!=str(lease_id) or release["last_pair_sequence"]!=str(last) or release["invalidated"] is not True:
                raise ValueError("actual lease release order")
            manifest["release_events"].append(dict(artifact=f"release_{lease_id}.json",sha256=p.sha(read(root,f"release_{lease_id}.json"))))
        manifest["result"]="complete"
    except Exception as ex:
        failure=str(ex)
        if proc and proc.poll() is None:
            try:send("ABORT")
            except (BrokenPipeError,OSError):pass
            try:proc.wait(timeout=max(0.01,min(.5,15-(time.monotonic()-start))))
            except subprocess.TimeoutExpired:
                proc.terminate();proc.wait(timeout=3);manifest["forced_cleanup"]=True
    finally:
        if proc:
            manifest["exit_code"]=proc.returncode;manifest["owned_processes_absent"]=proc.poll() is not None
            if proc.stdin:proc.stdin.close()
            if proc.stdout:proc.stdout.close()
        if stderr:stderr.close()
        manifest["failure"]=failure
        manifest["elapsed_ms"]=round((time.monotonic()-start)*1000)
        if proc:
            launch=json_bytes(dict(pid=proc.pid,exe_sha256=build["exe_sha256"],run_id=run_id))
            put(root/"launch.receipt.json",launch)
            manifest["private_launch_receipt_sha256"]=p.sha(launch)
        if set(manifest)!=set(schema.RUN_KEYS):raise ValueError("run schema drift")
        put(root/"run.receipt.json",json_bytes(manifest))
    if manifest["result"]!="complete":raise RuntimeError(failure or "incomplete native run")


if __name__=="__main__":
    a=argparse.ArgumentParser();a.add_argument("--reviewed-run",action="store_true")
    a.add_argument("--exe",required=True);a.add_argument("--output",required=True)
    a.add_argument("--build-receipt",required=True);a.add_argument("--build-receipt-sha256",required=True)
    execute(a.parse_args())
