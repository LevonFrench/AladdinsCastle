# SPDX-License-Identifier: GPL-3.0-only
"""Analyze read-only OutRun scaler captures. Python 3.12+, stdlib only.

Road distance is a flat-ground screen proxy, not measured metres/world Z.
Both raw-register reciprocal and renderer-correct effective-scale reciprocal
are fitted to prevent mistaking the hardware sampling increment for scale.
"""
from __future__ import annotations
import argparse
import collections
import json
import math
from pathlib import Path
import statistics
import sys


def quantile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * fraction)]


def fit(pairs):
    """OLS with intercept plus physical-model through-origin k and residuals."""
    if len(pairs) < 3:
        return {"n": len(pairs), "status": "insufficient_samples"}
    xs, ys = zip(*pairs)
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    xx = sum((x - mx)**2 for x in xs)
    yy = sum((y - my)**2 for y in ys)
    xy = sum((x - mx) * (y - my) for x, y in pairs)
    if xx == 0 or yy == 0:
        return {"n": len(pairs), "status": "zero_variance"}
    slope = xy / xx
    intercept = my - slope * mx
    k = sum(x*y for x, y in pairs) / sum(x*x for x in xs)
    errors = [abs(k*x - y)/y for x,y in pairs]
    rmse = math.sqrt(statistics.fmean((k*x-y)**2 for x,y in pairs))
    return {"n": len(pairs), "status": "ok", "pearson_r": xy/math.sqrt(xx*yy),
            "r_squared_with_intercept": xy*xy/(xx*yy), "k_through_origin": k,
            "ols_slope": slope, "ols_intercept": intercept,
            "origin_nrmse": rmse/my, "median_relative_error": quantile(errors,.5),
            "p90_relative_error": quantile(errors,.9),
            "fraction_error_over_25pct": sum(e>.25 for e in errors)/len(errors)}


def distribution(counter):
    total = sum(counter.values())
    values = list(counter.elements())
    return {"n": total, "unique": len(counter), "min": min(counter) if counter else None,
            "p10": quantile(values,.1), "median": quantile(values,.5),
            "p90": quantile(values,.9), "max": max(counter) if counter else None,
            "most_common": counter.most_common(12)}


def analyze(path, args):
    metadata, ending = None, None
    samples = []
    reasons = collections.Counter()
    zoom_h, zoom_v = collections.Counter(), collections.Counter()
    priority = collections.Counter()
    horizons = collections.Counter()
    frame_count, valid_frames, latched_frames = 0, 0, 0
    families = collections.defaultdict(list)
    for line_no,line in enumerate(path.open(encoding="utf-8-sig"),1):
        try:
            row=json.loads(line)
        except json.JSONDecodeError as exc:
            raise ValueError(f"Line {line_no}: malformed JSON") from exc
        if row.get("kind")=="metadata":
            if metadata is not None or row.get("schema") != 1 or row.get("system") != "outrun":
                raise ValueError("Unexpected metadata/schema/system")
            metadata=row
            continue
        if row.get("kind")=="end":
            ending=row
            continue
        if row.get("kind") != "frame":
            raise ValueError(f"Line {line_no}: unexpected record")
        frame_count+=1
        if row["frame"] != frame_count:
            raise ValueError("Capture frames are not consecutive")
        if row.get("road_latches",0)>0:
            latched_frames+=1
        if row["frame"] < args.min_frame:
            continue
        road=row["road"]
        horizon=row["first_road_y"]
        road_ok=(row.get("road_latches",0)>0 and row.get("road_control_known",False)
                 and isinstance(road,list) and len(road)==224 and horizon>=0)
        if road_ok:
            valid_frames+=1
            horizons[horizon]+=1
        for sprite in row["sprites"]:
            if sprite["ending"]:
                reasons["end_marker"]+=1
                continue
            if sprite["hidden"]:
                reasons["hidden"]+=1
                continue
            hz,vz=sprite["hzoom"],sprite["vzoom"]
            zoom_h[hz]+=1;zoom_v[vz]+=1
            priority[sprite["priority"]]+=1
            bottom=sprite["bottom"]
            if not road_ok:
                reason="no_latched_road"
            elif bottom<0 or bottom>=224 or sprite["top"]<0:
                reason="clipped_vertical_bounds"
            elif not 0<=sprite["x"]<320:
                reason="offscreen_x_anchor"
            elif not args.min_height<=sprite["height"]<=args.max_height:
                reason="tiny_or_giant_height"
            elif hz<64 or vz<64:
                reason="clamped_zoom"
            elif max(hz,vz)/min(hz,vz)>args.max_anisotropy:
                reason="nonuniform_scale"
            elif bottom-horizon<args.clearance or not road[bottom]["visible"]:
                reason="above_road_or_horizon"
            else:
                reason="road_scanline_candidate"
            reasons[reason]+=1
            if reason != "road_scanline_candidate":
                continue
            # Unknown camera-height*focal sets the units; choose 1 screen unit.
            zroad=1.0/(bottom-horizon)
            sample={"frame":row["frame"],"slot":sprite["slot"],
                    "priority":sprite["priority"],"bank":sprite["bank"],
                    "offset":sprite["offset"],"hzoom":hz,"vzoom":vz,
                    "bottom":bottom,"horizon":horizon,"zroad":zroad,
                    "inverse_effective_scale":vz/512.0,"inverse_register_zoom":512.0/vz}
            samples.append(sample)
            families[(sprite["bank"],sprite["offset"])].append(sample)
    if metadata is None:
        raise ValueError("Missing metadata")
    if not ending or ending.get("reason") != "frame_limit" or ending.get("frames") != frame_count:
        raise ValueError("Incomplete/error capture; no successful-frame-limit trailer")
    if frame_count != metadata["requested_frames"]:
        raise ValueError("Capture did not reach requested frame count")
    models={}
    split=int(frame_count*.8)
    for name in ("inverse_effective_scale","inverse_register_zoom"):
        result=fit([(s[name],s["zroad"]) for s in samples])
        training=fit([(s[name],s["zroad"]) for s in samples if s["frame"]<=split])
        holdout=[s for s in samples if s["frame"]>split]
        if training.get("status")=="ok" and holdout:
            k=training["k_through_origin"]
            result["time_holdout"]={"split_frame":split,"train_n":training["n"],
                "test_n":len(holdout),"train_k":k,
                "median_relative_error":quantile([abs(k*s[name]-s["zroad"])/s["zroad"] for s in holdout],.5),
                "p90_relative_error":quantile([abs(k*s[name]-s["zroad"])/s["zroad"] for s in holdout],.9)}
        models[name]=result
    chosen=models["inverse_effective_scale"]
    outliers=[]
    if chosen.get("status")=="ok":
        k=chosen["k_through_origin"]
        # Report distinct frame/slot observations, not every repeated frame.
        seen=set()
        for s in sorted(samples,key=lambda s:abs(k*s["inverse_effective_scale"]-s["zroad"])/s["zroad"],reverse=True):
            key=(s["bank"],s["offset"],s["vzoom"],s["bottom"])
            if key in seen:
                continue
            seen.add(key)
            outliers.append(dict(s,relative_error=abs(k*s["inverse_effective_scale"]-s["zroad"])/s["zroad"]))
            if len(outliers)>=12:
                break
    groups=[]
    for (bank,offset),group in families.items():
        fitted=fit([(s["inverse_effective_scale"],s["zroad"]) for s in group])
        if fitted.get("status")=="ok" and len(group)>=30:
            groups.append({"bank":bank,"offset":offset,**fitted})
    group_stats={"eligible_groups":len(groups),
                 "groups_with_r_at_least_0_9":sum(g["pearson_r"]>=.9 for g in groups),
                 "min_k":min((g["k_through_origin"] for g in groups),default=None),
                 "max_k":max((g["k_through_origin"] for g in groups),default=None)}
    best_groups=sorted(groups,key=lambda g:g["pearson_r"],reverse=True)[:8]
    robustness={"excluding_raw_zoom_512":fit([(s["inverse_effective_scale"],s["zroad"])
                 for s in samples if s["vzoom"]!=512]),
                "common_fixed_horizons":{str(h):fit([(s["inverse_effective_scale"],s["zroad"])
                  for s in samples if s["horizon"]==h]) for h,_ in horizons.most_common(2)}}
    groups.sort(key=lambda g:g["n"],reverse=True)
    # Downweight repeated frames: also fit distinct register/footpoint tuples.
    unique={(s["vzoom"],s["bottom"],s["horizon"]) for s in samples}
    distinct_fit=fit([(v/512.0,1.0/(b-h)) for v,b,h in unique])
    return {"schema":1,"system":"outrun","mame":metadata["mame"],
            "frames":frame_count,"frames_with_latched_road":latched_frames,
            "frames_with_eligible_road":valid_frames,"min_frame":args.min_frame,
            "filters":{"clearance":args.clearance,"min_height":args.min_height,
                       "max_height":args.max_height,"max_anisotropy":args.max_anisotropy},
            "horizon_distribution":distribution(horizons),"hzoom_distribution":distribution(zoom_h),
            "vzoom_distribution":distribution(zoom_v),"priority_counts":dict(priority),
            "rejection_counts":dict(reasons),"road_candidate_samples":len(samples),
            "models":models,"distinct_geometry_fit":distinct_fit,
            "by_priority":{str(p):fit([(s["inverse_effective_scale"],s["zroad"]) for s in samples if s["priority"]==p]) for p in sorted(priority)},
            "identity_groups":groups[:20],"identity_group_statistics":group_stats,
            "best_correlated_identity_groups":best_groups,"robustness":robustness,"worst_outliers":outliers,
            "limits":["zroad is a flat-ground screen proxy, not world Z or metres",
                      "road scanline and x anchor do not prove physical road contact",
                      "no road/sprite ROM graphics are read; opaque pixel bounds unavailable",
                      "sprite identities may change bank/offset during animation",
                      "frames/objects are correlated; r is descriptive, not independent evidence"]}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture",type=Path)
    parser.add_argument("--output",type=Path)
    parser.add_argument("--min-frame",type=int,default=300)
    parser.add_argument("--clearance",type=int,default=8)
    parser.add_argument("--min-height",type=int,default=4)
    parser.add_argument("--max-height",type=int,default=150)
    parser.add_argument("--max-anisotropy",type=float,default=1.25)
    parser.add_argument("--text-plot",action="store_true")
    args=parser.parse_args()
    if args.min_frame<1 or args.clearance<1 or args.min_height<1 or args.max_height<args.min_height or not math.isfinite(args.max_anisotropy) or args.max_anisotropy<1:
        parser.error("Invalid filter limits")
    try:
        summary=analyze(args.capture,args)
        output=args.output or Path(".local/mame-probe")/(args.capture.stem+"-summary.json")
        output=output.resolve()
        # Keep reports in an explicitly named .local ancestor.
        if ".local" not in output.parts:
            parser.error("Summary output must remain inside a .local directory")
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps(summary,indent=2,allow_nan=False)+"\n",encoding="utf-8")
        print(f"Frames: {summary['frames']}; road candidates: {summary['road_candidate_samples']}")
        for name,model in summary["models"].items():
            if model.get("status")=="ok":
                print(f"{name}: k={model['k_through_origin']:.9g}, r={model['pearson_r']:.6f}, "
                      f"median error={model['median_relative_error']:.2%}, p90={model['p90_relative_error']:.2%}")
            else:
                print(f"{name}: {model['status']} (n={model['n']})")
        print("Units: normalized flat-ground proxy; no metric/world-depth validation.")
        print(f"Summary: {output}")
        if args.text_plot:
            counter=summary["vzoom_distribution"]["most_common"]
            maximum=max((count for _,count in counter),default=1)
            text="\n".join(f"{zoom:4}: {'#'*round(40*count/maximum)} {count}" for zoom,count in counter)+"\n"
            output.with_suffix(".txt").write_text(text,encoding="utf-8")
            print(text,end="")
        return 0
    except (OSError,ValueError,KeyError,TypeError) as exc:
        print(f"Probe analysis failed: {exc}",file=sys.stderr)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
