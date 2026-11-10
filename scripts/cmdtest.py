#!/usr/bin/python3
"""Unified command-set test for m12d over TCP or a serial port (stdlib only).
Run with /usr/bin/python3: macOS Local Network privacy blocks Homebrew Python from
reaching 10.254.239.1 ("No route to host"); Apple-signed binaries are allowed.

  cmdtest.py --tcp HOST:PORT | --serial DEV [--baud 115200]
             --script tests/cmd_script.jsonl --expect tests/cmd_expected.jsonl [--save out.jsonl]
  cmdtest.py --tcp HOST:PORT | --serial DEV --watch SECONDS
  cmdtest.py --tcp HOST:PORT | --serial DEV --send '<json>'      (one request, print the reply)

Script lines: a raw request, "@crlf <req>" (sent with CRLF) or "@long N" (N junk bytes).
Replies are lines with an "ok" key; push lines (detection / heartbeat) are skipped.
Replies to "ver" and "status" are compared by shape (keys + value types), all others
byte for byte. Exit 0 when every reply matches.
"""
import argparse
import json
import os
import select
import socket
import sys
import termios
import time


class Conn:
    def __init__(self, tcp=None, serial=None, baud=115200):
        self.buf = b""
        if tcp:
            host, port = tcp.rsplit(":", 1)
            self.sock = socket.create_connection((host, int(port)), timeout=5)
            self.sock.setblocking(False)
            self.fd = self.sock.fileno()
            self.send_raw = self.sock.sendall
        else:
            self.sock = None
            self.fd = os.open(serial, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            t = termios.tcgetattr(self.fd)
            speed = getattr(termios, f"B{baud}")
            t[0] = 0                                   # iflag
            t[1] = 0                                   # oflag
            t[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            t[3] = 0                                   # lflag
            t[4] = t[5] = speed
            termios.tcsetattr(self.fd, termios.TCSANOW, t)
            termios.tcflush(self.fd, termios.TCIOFLUSH)
            self.send_raw = self._serial_write

    def _serial_write(self, data):
        while data:
            _, w, _ = select.select([], [self.fd], [], 2)
            if not w:
                raise TimeoutError("serial write timeout")
            n = os.write(self.fd, data)
            data = data[n:]

    def readline(self, timeout):
        end = time.time() + timeout
        while b"\n" not in self.buf:
            left = end - time.time()
            if left <= 0:
                return None
            r, _, _ = select.select([self.fd], [], [], left)
            if not r:
                return None
            chunk = self.sock.recv(4096) if self.sock else os.read(self.fd, 4096)
            if not chunk:
                return None
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode("utf-8", "replace").rstrip("\r")

    def next_reply(self, timeout=3.0):
        end = time.time() + timeout
        while True:
            line = self.readline(max(0.01, end - time.time()))
            if line is None:
                return None
            try:
                obj = json.loads(line)
            except ValueError:
                return line  # garbage is a failure the caller will see
            if isinstance(obj, dict) and "ok" in obj:
                return line


def load_script(path):
    reqs = []
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("@crlf "):
            reqs.append((line[6:], (line[6:] + "\r\n").encode()))
        elif line.startswith("@long "):
            reqs.append(("@long", b"a" * int(line[6:]) + b"\n"))
        elif line:
            reqs.append((line, (line + "\n").encode()))
    return reqs


def shape(v):
    if isinstance(v, dict):
        return {k: shape(x) for k, x in v.items()}
    if isinstance(v, list):  # element types only: e.g. the model list grows with features
        return ["list"] + sorted({json.dumps(shape(x), sort_keys=True) for x in v})
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        return "num"
    return type(v).__name__


def command_of(req_text):
    try:
        return json.loads(req_text).get("do")
    except (ValueError, AttributeError):
        return None


def run_script(conn, args):
    reqs = load_script(args.script)
    expected = [l.rstrip("\n") for l in open(args.expect) if l.strip()]
    if len(reqs) != len(expected):
        sys.exit(f"cmdtest: {len(reqs)} requests but {len(expected)} expected replies")
    got, fails = [], 0
    for (text, raw), exp in zip(reqs, expected):
        conn.send_raw(raw)
        rep = conn.next_reply()
        got.append(rep or "")
        if command_of(text) in ("ver", "status") and rep:
            ok = shape(json.loads(rep)) == shape(json.loads(exp))
        else:
            ok = rep == exp
        if not ok:
            fails += 1
            print(f"FAIL  {text[:60]}\n  want {exp}\n  got  {rep}")
    if args.save:
        with open(args.save, "w") as f:
            f.write("\n".join(got) + "\n")
    print(f"{'PASS' if fails == 0 else 'FAIL'} {len(reqs) - fails}/{len(reqs)} replies match")
    return 0 if fails == 0 else 1


def watch(conn, seconds):
    end = time.time() + seconds
    det = hb = other = 0
    while time.time() < end:
        line = conn.readline(end - time.time())
        if line is None:
            continue
        print(line)
        if line.startswith('{"f":'):
            det += 1
        elif line.startswith('{"hb":'):
            hb += 1
        else:
            other += 1
    print(f"watch: det_lines={det} hb_lines={hb} other={other} in {seconds}s")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--tcp")
    g.add_argument("--serial")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--script")
    ap.add_argument("--expect")
    ap.add_argument("--save")
    ap.add_argument("--watch", type=float)
    ap.add_argument("--send")
    args = ap.parse_args()
    conn = Conn(tcp=args.tcp, serial=args.serial, baud=args.baud)
    if args.watch:
        return watch(conn, args.watch)
    if args.send:
        conn.send_raw((args.send + "\n").encode())
        rep = conn.next_reply()
        print(rep)
        return 0 if rep else 1
    if not (args.script and args.expect):
        ap.error("--script and --expect are required unless --watch or --send is given")
    return run_script(conn, args)


if __name__ == "__main__":
    sys.exit(main())
