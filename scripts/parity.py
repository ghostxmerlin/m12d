#!/usr/bin/env python3
"""Compare two detection result files (B-layer parity).

usage: parity.py <a.txt> <b.txt> --coord PX --prob P [--label NAME]
Each file has one "face x y w h prob lm0x lm0y .. lm4x lm4y" line per face.
Faces are paired greedily by highest IoU. Prints counts and the max coordinate
difference (box + landmarks) and max prob difference. Exit 0 if counts match and
both maxima are within tolerance, else 1.
"""
import argparse
import sys


def load(path):
    faces = []
    with open(path) as f:
        for line in f:
            p = line.split()
            if not p or p[0] != "face":
                continue
            v = [float(x) for x in p[1:]]
            if len(v) != 15:
                sys.exit(f"parity: bad line in {path}: {line.strip()}")
            faces.append(v)
    return faces


def iou(a, b):
    ax0, ay0, ax1, ay1 = a[0], a[1], a[0] + a[2], a[1] + a[3]
    bx0, by0, bx1, by1 = b[0], b[1], b[0] + b[2], b[1] + b[3]
    w = min(ax1, bx1) - max(ax0, bx0)
    h = min(ay1, by1) - max(ay0, by0)
    if w <= 0 or h <= 0:
        return 0.0
    inter = w * h
    return inter / (a[2] * a[3] + b[2] * b[3] - inter)


def compare(a, b):
    """Return (pairs, max_coord, max_prob)."""
    cand = sorted(((iou(x, y), i, j) for i, x in enumerate(a) for j, y in enumerate(b)), reverse=True)
    used_a, used_b, pairs = set(), set(), []
    for v, i, j in cand:
        if v > 0 and i not in used_a and j not in used_b:
            used_a.add(i)
            used_b.add(j)
            pairs.append((i, j))
    max_coord = max_prob = 0.0
    for i, j in pairs:
        x, y = a[i], b[j]
        coords = [k for k in range(15) if k != 4]
        max_coord = max(max_coord, max(abs(x[k] - y[k]) for k in coords))
        max_prob = max(max_prob, abs(x[4] - y[4]))
    return pairs, max_coord, max_prob


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--coord", type=float, required=True)
    ap.add_argument("--prob", type=float, required=True)
    ap.add_argument("--label", default="")
    o = ap.parse_args()
    a, b = load(o.a), load(o.b)
    pairs, mc, mp = compare(a, b)
    ok = len(a) == len(b) == len(pairs) and mc <= o.coord and mp <= o.prob
    print(f"{'PASS' if ok else 'FAIL'} {o.label} faces={len(a)}/{len(b)} paired={len(pairs)} "
          f"max_coord={mc:.3f} max_prob={mp:.4f}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
