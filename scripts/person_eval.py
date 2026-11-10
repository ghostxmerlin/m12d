#!/usr/bin/python3
"""Score person_file output against ground truth (phase 1b experiment).

person_eval.py <person_raw.txt> <gt.json>... [--ignore-extra frame_substr ...]
gt.json maps frame path -> [x, y, w, h] of the one subject (640x480). Frames named
*/empty_* or */clothes_* have no person. A detection matches when IoU >= 0.5.
--ignore-extra: frames with other real people in them; unmatched boxes there are not FPs.
Prints per shot recall at each size/threshold, FP counts, and whether RetinaFace (192x144)
found a face, then a summary table.
"""
import collections, json, re, sys

args = sys.argv[1:]
ign = []
if "--ignore-extra" in args:
    k = args.index("--ignore-extra"); ign = args[k + 1:]; args = args[:k]
raw, gts = args[0], args[1:]
gt = {}
for g in gts: gt.update(json.load(open(g)))
det = collections.defaultdict(list); face = collections.defaultdict(float); frames = set()
for l in open(raw):
    p = l.split(); f = p[0]; frames.add(f)
    if p[1] == "face192": face[f] = max(face[f], float(p[2]))
    elif not p[1].endswith("_ms") and int(p[2]) == 15:
        det[(f, p[1])].append((float(p[3]), [float(v) for v in p[4:8]]))
frames = sorted(f for f in frames if f in gt or re.search(r"/(empty|clothes)_", f))

def iou(a, b):
    ax2, ay2, bx2, by2 = a[0] + a[2], a[1] + a[3], b[0] + b[2], b[1] + b[3]
    iw = max(0, min(ax2, bx2) - max(a[0], b[0])); ih = max(0, min(ay2, by2) - max(a[1], b[1]))
    i = iw * ih
    return i / (a[2] * a[3] + b[2] * b[3] - i)

SIZES = ["yolo160", "yolo224", "yolo320"]; THS = [0.3, 0.4, 0.5, 0.55, 0.6, 0.7]
shot = lambda f: re.sub(r"_\d+\.yuyv$", "", f.split("tests/data/")[-1])
rows = collections.OrderedDict()
for f in frames:
    r = rows.setdefault(shot(f), {"n": 0, "face": 0})
    r["n"] += 1; r["face"] += face[f] >= 0.8
    for s in SIZES:
        for th in THS:
            ds = [d for d in det[(f, s)] if d[0] >= th]
            hit = f in gt and any(iou(b, gt[f]) >= 0.5 for _, b in ds)
            fp = sum(1 for _, b in ds if not (f in gt and iou(b, gt[f]) >= 0.5))
            if any(x in f for x in ign): fp = 0
            r[(s, th, "tp")] = r.get((s, th, "tp"), 0) + hit
            r[(s, th, "fp")] = r.get((s, th, "fp"), 0) + fp
TH0 = float(dict(a.split("=") for a in ["th=0.5"])["th"])
print("shot                 n  face | " + " | ".join(f"{s[4:]}@.5 hit/fp" for s in SIZES))
for name, r in rows.items():
    print(f"{name:20s} {r['n']:2d}  {r['face']:4d} | " +
          " | ".join(f"{r[(s, 0.5, 'tp')]:5d}/{r[(s, 0.5, 'fp')]:<4d}  " for s in SIZES))
print()
print("summary: recall on person frames / FP boxes on person frames / FP boxes on empty+clothes frames")
for s in SIZES:
    for th in THS:
        P = [r for n, r in rows.items() if not re.search(r"/(empty|clothes)$", n)]
        E = [r for n, r in rows.items() if re.search(r"/(empty|clothes)$", n)]
        tp = sum(r[(s, th, "tp")] for r in P); n = sum(r["n"] for r in P)
        fpp = sum(r[(s, th, "fp")] for r in P); fpe = sum(r[(s, th, "fp")] for r in E); ne = sum(r["n"] for r in E)
        print(f"{s} th={th:.2f}: recall {tp}/{n}  fp_on_person_frames {fpp}  fp_on_empty {fpe}/{ne} frames")
