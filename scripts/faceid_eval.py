#!/usr/bin/python3
"""Phase-2 offline face-ID evaluation over faceid_eval output (stdlib only).

faceid_eval.py <feat.txt> --enroll SUBSTR [SUBSTR ...] --user SUBSTR [...] --others SUBSTR [...]
                [--enroll-n 5] [--pick largest|all]

feat.txt lines: <file> <idx> <x> <y> <w> <h> <prob> <128 floats> (tools/faceid_eval.cpp).
- enroll: frames whose path contains any SUBSTR; the largest face of each, first --enroll-n of
  them form the library (like one or more `learn`s).
- user:   test frames of the enrolled person (largest face per frame), grouped by shot name.
- others: every face in matching frames counts as "not the user".
Prints per-shot recall at thresholds 0.30..0.80, the impostor maximum and false accepts.
Features are biometric: keep feat.txt out of the repo.
"""
import argparse, collections, math, re

ap = argparse.ArgumentParser()
ap.add_argument("feat")
ap.add_argument("--enroll", nargs="+", required=True)
ap.add_argument("--user", nargs="+", required=True)
ap.add_argument("--others", nargs="*", default=[])
ap.add_argument("--enroll-n", type=int, default=5)
a = ap.parse_args()

faces = collections.defaultdict(list)  # file -> [(area, vec)]
for line in open(a.feat):
    p = line.split()
    if len(p) != 7 + 128:
        continue
    faces[p[0]].append((float(p[4]) * float(p[5]), [float(v) for v in p[7:]]))

def largest(f):
    return max(faces[f])[1]

def match(f, subs):
    return any(s in f for s in subs)

files = sorted(faces)
enroll = [largest(f) for f in files if match(f, a.enroll)][: a.enroll_n]
enroll_files = {f for f in files if match(f, a.enroll)}
lib = enroll
def best(v):
    return max(sum(x * y for x, y in zip(v, e)) for e in lib)

shot = lambda f: re.sub(r"_\d+\.yuyv$", "", f.split("tests/data/")[-1])
user = collections.defaultdict(list)
for f in files:
    if match(f, a.user) and f not in enroll_files:
        user[shot(f)].append(best(largest(f)))
imp = collections.defaultdict(list)
for f in files:
    if match(f, a.others):
        for _, v in faces[f]:
            imp[shot(f)].append(best(v))

print(f"library: {len(lib)} samples from {sorted({shot(f) for f in enroll_files})}")
ths = [0.30 + 0.05 * i for i in range(11)]
print("shot                  n   min    med  | recall at " + " ".join(f"{t:.2f}" for t in ths))
for k in sorted(user):
    v = sorted(user[k])
    print(f"{k:20s} {len(v):3d}  {v[0]:.3f}  {v[len(v)//2]:.3f} | " + " ".join(f"{sum(x >= t for x in v):4d}" for t in ths))
allimp = [x for v in imp.values() for x in v]
if allimp:
    print(f"impostors: {len(allimp)} faces, max {max(allimp):.3f}")
    for k in sorted(imp):
        print(f"  {k:20s} n={len(imp[k]):3d} max={max(imp[k]):.3f}")
    print("false accepts at   " + " ".join(f"{sum(x >= t for x in allimp):4d}" for t in ths))
else:
    print("impostors: none")
