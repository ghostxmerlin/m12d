#!/usr/bin/env python3
"""onnxruntime reference for insightface genderage (runs in the m12d-mxnet container).

usage: ref_genderage.py <genderage.onnx> <crop_dir>
For every <name>.rgb (96x96 RGB) runs the original ONNX model (input = raw RGB floats,
normalisation is in the graph; same as insightface Attribute with mean 0 / std 1) and
compares with <name>.out from tools/genderage_file (ncnn). Exit 0 if all decodes match
and max |diff| < 1e-2.
"""
import glob
import os
import sys

import numpy as np
import onnxruntime as ort


def decode(o):
    return int(np.argmax(o[:2])), int(round(float(o[2]) * 100))


def main():
    sess = ort.InferenceSession(sys.argv[1], providers=["CPUExecutionProvider"])
    name = sess.get_inputs()[0].name
    worst, bad, n = 0.0, 0, 0
    for path in sorted(glob.glob(os.path.join(sys.argv[2], "*.rgb"))):
        img = np.fromfile(path, dtype=np.uint8).reshape(96, 96, 3).astype(np.float32)
        ref = sess.run(None, {name: np.transpose(img, (2, 0, 1))[np.newaxis]})[0][0]
        ours = np.loadtxt(path[:-4] + ".out", dtype=np.float32)
        d = float(np.max(np.abs(ref - ours)))
        same = decode(ref) == decode(ours)
        worst, bad, n = max(worst, d), bad + (not same), n + 1
        print(f"{os.path.basename(path)[:-4]:22} max|diff|={d:.5f} onnx={decode(ref)} ncnn={decode(ours)}"
              f"{'' if same else '  MISMATCH'}")
    print(f"crops={n} worst_diff={worst:.5f} decode_mismatches={bad}")
    sys.exit(0 if n and bad == 0 and worst < 1e-2 else 1)


if __name__ == "__main__":
    main()
