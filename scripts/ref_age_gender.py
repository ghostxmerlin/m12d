#!/usr/bin/env python3
"""MXNet reference for the age/gender model (runs in the m12d-mxnet container).

usage: ref_age_gender.py <model_dir> <crop_dir>
For every <name>.rgb (112x112 RGB) in crop_dir, runs the original MXNet model and
compares fc1 with <name>.fc1 written by tools/attr_file (ncnn). Prints per-crop max |diff|
and both decodes. Exit 0 if every crop decodes identically and max |diff| < 1e-2.
"""
import glob
import os
import sys

import mxnet as mx
import numpy as np


def decode(fc1):
    g = int(np.argmax(fc1[0:2]))
    a = fc1[2:202].reshape((100, 2))
    return int(np.sum(np.argmax(a, axis=1))), g


def main():
    model_dir, crop_dir = sys.argv[1], sys.argv[2]
    sym, arg, aux = mx.model.load_checkpoint(os.path.join(model_dir, "model"), 0)
    sym = sym.get_internals()["fc1_output"]
    mod = mx.mod.Module(symbol=sym, context=mx.cpu(), label_names=None)
    mod.bind(data_shapes=[("data", (1, 3, 112, 112))], for_training=False)
    mod.set_params(arg, aux)
    worst, mismatches, n = 0.0, 0, 0
    for path in sorted(glob.glob(os.path.join(crop_dir, "*.rgb"))):
        img = np.fromfile(path, dtype=np.uint8).reshape(112, 112, 3).astype(np.float32)
        data = mx.nd.array(np.transpose(img, (2, 0, 1))[np.newaxis])
        mod.forward(mx.io.DataBatch(data=(data,)), is_train=False)
        ref = mod.get_outputs()[0].asnumpy()[0]
        ours = np.loadtxt(path[:-4] + ".fc1", dtype=np.float32)
        d = float(np.max(np.abs(ref - ours)))
        ra, rg = decode(ref)
        oa, og = decode(ours)
        same = (ra, rg) == (oa, og)
        worst, mismatches, n = max(worst, d), mismatches + (not same), n + 1
        print(f"{os.path.basename(path)[:-4]:24} max|diff|={d:.5f} mxnet age={ra} g={rg}  ncnn age={oa} g={og}"
              f"{'' if same else '  MISMATCH'}")
    print(f"crops={n} worst_diff={worst:.5f} decode_mismatches={mismatches}")
    sys.exit(0 if n and mismatches == 0 and worst < 1e-2 else 1)


if __name__ == "__main__":
    main()
