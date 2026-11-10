#!/usr/bin/python3
"""Phase-1b device acceptance, voice-guided (macOS `say`), reading m12d over TCP.

accept1b.py [--tcp HOST:PORT] [--out results/xxx.jsonl]
Stages: near (60 s: fps/temp with a face), stand 1.5/2.5/3.5 m and far (5 s each),
coat on a chair with nobody in view (60 s). Prints a per-stage summary; raw lines go to --out.
"""
import argparse, json, socket, subprocess, time

STAGES = [  # name, spoken prompt, settle s, record s
    ("near", "请站在镜头前一米以内，正对镜头，保持一分钟，直到听到好", 8, 60),
    ("d15", "退到一点五米，正对镜头站好", 8, 5),
    ("d25", "退到两点五米", 7, 5),
    ("d35", "退到三点五米", 7, 5),
    ("far", "退到最远处", 7, 5),
    ("coat", "把外套挂到椅子上，放到两米左右，然后走出画面，保持一分钟，直到听到好", 40, 60),
]


def say(t):
    subprocess.run(["say", "-v", "Tingting", t], check=False)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tcp", default="10.254.239.1:5555")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    host, port = a.tcp.rsplit(":", 1)
    s = socket.create_connection((host, int(port)), timeout=3)
    s.settimeout(0.5)
    out = open(a.out, "w") if a.out else None
    buf = b""

    def pump(secs, keep):
        nonlocal buf
        end = time.time() + secs
        while time.time() < end:
            try:
                chunk = s.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                raise SystemExit("connection closed")
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if keep is not None:
                    try:
                        keep.append(json.loads(line))
                    except ValueError:
                        pass

    print("stage  lines  with_face  with_person  paired  hb_fps(avg/min)  temp(max)")
    for name, prompt, settle, rec in STAGES:
        say(prompt)
        pump(settle, None)
        say("开始")
        got = []
        pump(rec, got)
        say("好")
        det = [o for o in got if "f" in o]
        hb = [o["hb"] for o in got if "hb" in o]
        wf = sum(1 for o in det if o.get("num", 0) > 0)
        wp = sum(1 for o in det if o.get("person"))
        pr = sum(1 for o in det if any(p.get("face", -1) >= 0 for p in o.get("person", [])))
        fps = [h["fps"] for h in hb]
        tmax = max((h["t"] for h in hb), default=-1)
        print(f"{name:6s} {len(det):5d}  {wf:9d}  {wp:11d}  {pr:6d}  "
              f"{(sum(fps) / len(fps) if fps else 0):6.1f}/{(min(fps) if fps else 0):4.1f}  {tmax:5d}", flush=True)
        if out:
            for o in got:
                out.write(json.dumps({"stage": name, **o}, separators=(",", ":")) + "\n")
    say("验收结束")


if __name__ == "__main__":
    main()
