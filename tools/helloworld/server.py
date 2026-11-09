#!/usr/bin/python3
"""helloworld: live view of m12d detection data in a browser (stdlib only).

  /usr/bin/python3 tools/helloworld/server.py [--tcp HOST:PORT | --serial DEV [--baud N] | --replay FILE] [--port 8765]
  then open http://127.0.0.1:8765/

Upstream is m12d's unified line protocol, over TCP today (default 10.254.239.1:5555)
or a serial port later (same lines). Push lines (detections, heartbeats) are relayed
to the page as Server-Sent Events; the page sends commands through POST /cmd.
The HTTP side binds 127.0.0.1 only.

Run with /usr/bin/python3: macOS Local Network privacy blocks Homebrew Python from
reaching the M12 ("No route to host").
"""
import argparse
import json
import os
import queue
import select
import socket
import sys
import termios
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))


class Link:
    """Routes upstream lines: replies (lines with "ok") go to the waiting request with the
    same id; everything else that parses as JSON is broadcast to the page."""

    def __init__(self, send, broadcast):
        self._send = send
        self._broadcast = broadcast
        self._lock = threading.Lock()
        self._next_id = 1
        self._waiting = {}  # id -> [threading.Event, reply]

    def feed_line(self, line):
        try:
            obj = json.loads(line)
        except ValueError:
            return
        if not isinstance(obj, dict):
            return
        if "ok" in obj:
            with self._lock:
                slot = self._waiting.get(obj.get("id"))
            if slot:
                slot[1] = obj
                slot[0].set()
            return
        self._broadcast(obj)

    def request(self, cmd, timeout=3.0):
        """Send one command (dict without id); return the reply dict or None on timeout."""
        with self._lock:
            rid = self._next_id
            self._next_id = self._next_id % 2_000_000_000 + 1
            slot = [threading.Event(), None]
            self._waiting[rid] = slot
        msg = dict(cmd)
        msg["id"] = rid
        try:
            self._send(json.dumps(msg, separators=(",", ":")))
            slot[0].wait(timeout)
            return slot[1]
        except OSError:
            return None
        finally:
            with self._lock:
                self._waiting.pop(rid, None)


class Upstream(threading.Thread):
    """Keeps a line connection to m12d (TCP or serial) and reconnects with backoff."""

    def __init__(self, tcp=None, serial=None, baud=115200, on_line=None, on_state=None):
        super().__init__(daemon=True)
        self.tcp, self.serial, self.baud = tcp, serial, baud
        self.on_line, self.on_state = on_line, on_state
        self.src = f"串口 {serial}" if serial else f"网络 {tcp}"
        self._wlock = threading.Lock()
        self._fd = None
        self._sock = None

    def send(self, line):
        with self._wlock:
            data = (line + "\n").encode()
            if self._sock is not None:
                self._sock.sendall(data)
            elif self._fd is not None:
                os.write(self._fd, data)
            else:
                raise OSError("not connected")

    def _open(self):
        if self.serial:
            fd = os.open(self.serial, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            t = termios.tcgetattr(fd)
            speed = getattr(termios, f"B{self.baud}")
            t[0] = t[1] = t[3] = 0
            t[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            t[4] = t[5] = speed
            termios.tcsetattr(fd, termios.TCSANOW, t)
            self._fd = fd
            return fd
        host, port = self.tcp.rsplit(":", 1)
        s = socket.create_connection((host, int(port)), timeout=3)
        s.setblocking(False)
        self._sock = s
        return s.fileno()

    def _close(self):
        with self._wlock:
            if self._sock is not None:
                self._sock.close()
            elif self._fd is not None:
                os.close(self._fd)
            self._sock = self._fd = None

    def run(self):
        backoff = 1.0
        while True:
            try:
                fd = self._open()
            except OSError as e:
                self.on_state(False, f"连不上 {self.src}：{e.strerror or e}")
                time.sleep(backoff)
                backoff = min(backoff * 2, 5.0)
                continue
            backoff = 1.0
            self.on_state(True, f"已连接 {self.src}")
            buf = b""
            try:
                while True:
                    r, _, _ = select.select([fd], [], [], 5.0)
                    if not r:
                        continue  # m12d sends a heartbeat every 2 s; silence is reported by the page
                    chunk = self._sock.recv(65536) if self._sock else os.read(fd, 65536)
                    if not chunk:
                        raise OSError("对端关闭了连接")
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        self.on_line(line.decode("utf-8", "replace").rstrip("\r"))
                    if len(buf) > 65536:
                        buf = b""  # a runaway line without newline; drop it
            except OSError as e:
                self._close()
                self.on_state(False, f"连接断开（{e}），正在重连")
                time.sleep(1.0)


class Replay(threading.Thread):
    """Plays recorded m12d lines in a loop (UI development without a device).
    Detection lines are spaced `period` seconds apart; other lines are immediate."""

    def __init__(self, path, period=1 / 18, on_line=None, on_state=None):
        super().__init__(daemon=True)
        self.path, self.period = path, period
        self.on_line, self.on_state = on_line, on_state
        self.src = f"回放 {os.path.basename(path)}"

    def send(self, line):
        raise OSError("回放模式不能发命令")

    def run_once(self, on_line):
        with open(self.path) as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                on_line(line)
                if line.startswith('{"f":') and self.period:
                    time.sleep(self.period)

    def run(self):
        self.on_state(True, f"{self.src}（不是实时数据）")
        while True:
            self.run_once(self.on_line)


class Hub:
    """Fan-out of events to every open /events stream (bounded per client)."""

    def __init__(self):
        self._lock = threading.Lock()
        self._clients = set()
        self.state = {"link": {"up": False, "msg": "正在连接"}}

    def add(self):
        q = queue.Queue(maxsize=256)
        with self._lock:
            self._clients.add(q)
        return q

    def remove(self, q):
        with self._lock:
            self._clients.discard(q)

    def publish(self, obj):
        data = json.dumps(obj, separators=(",", ":"), ensure_ascii=False)
        with self._lock:
            clients = list(self._clients)
        for q in clients:
            try:
                q.put_nowait(data)
            except queue.Full:
                pass  # slow page: drop rather than block the upstream reader

    cmds = True  # False in replay mode: the page disables device settings

    def set_link(self, up, msg):
        self.state = {"link": {"up": up, "msg": msg, "cmds": self.cmds and up}}
        self.publish(self.state)


def request_allowed(headers, port):
    """Only the page served by this relay may use it: Host must be this loopback address
    (blocks DNS rebinding) and Origin, when sent, must be the same origin (blocks other
    sites' scripts)."""
    hosts = {f"127.0.0.1:{port}", f"localhost:{port}"}
    if headers.get("Host") not in hosts:
        return False
    origin = headers.get("Origin")
    return origin is None or origin in {f"http://{h}" for h in hosts}


def allowed_command(cmd):
    """The page only reads the config and toggles detection / landmarks / attributes."""
    if not isinstance(cmd, dict) or cmd.get("do") != "cfg":
        return False
    extra = set(cmd) - {"do"}
    return extra <= {"det", "lm", "attr"} and all(isinstance(cmd[k], bool) for k in extra)


def make_handler(hub, link, port):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            pass

        def _send(self, code, ctype, body):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if not request_allowed(self.headers, port):
                self._send(403, "text/plain; charset=utf-8", "只接受本机页面的请求".encode())
                return
            if self.path in ("/", "/helloworld", "/helloworld.html"):
                with open(os.path.join(HERE, "helloworld.html"), "rb") as f:
                    self._send(200, "text/html; charset=utf-8", f.read())
            elif self.path == "/events":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                q = hub.add()
                try:
                    self.wfile.write(f"data: {json.dumps(hub.state, ensure_ascii=False)}\n\n".encode())
                    self.wfile.flush()
                    while True:
                        try:
                            data = q.get(timeout=15)
                            self.wfile.write(f"data: {data}\n\n".encode())
                        except queue.Empty:
                            self.wfile.write(b": keepalive\n\n")
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError, OSError):
                    pass
                finally:
                    hub.remove(q)
            else:
                self._send(404, "text/plain; charset=utf-8", "没有这个页面".encode())

        def do_POST(self):
            if self.path != "/cmd":
                self._send(404, "text/plain; charset=utf-8", "没有这个接口".encode())
                return
            # application/json makes cross-site fetches need a CORS preflight, which we never grant.
            if not request_allowed(self.headers, port) or \
                    self.headers.get("Content-Type", "").split(";")[0].strip() != "application/json":
                self._send(403, "application/json", b'{"ok":false,"err":"forbidden"}')
                return
            n = int(self.headers.get("Content-Length") or 0)
            try:
                cmd = json.loads(self.rfile.read(min(n, 4096)) or b"{}")
            except ValueError:
                self._send(400, "application/json", b'{"ok":false,"err":"badjson"}')
                return
            if not allowed_command(cmd):
                self._send(403, "application/json", b'{"ok":false,"err":"notallowed"}')
                return
            reply = link.request(cmd)
            if reply is None:
                self._send(504, "application/json", b'{"ok":false,"err":"timeout"}')
            else:
                self._send(200, "application/json", json.dumps(reply).encode())

    return Handler


def main():
    ap = argparse.ArgumentParser(description="helloworld live view for m12d")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--tcp", default="10.254.239.1:5555")
    g.add_argument("--serial")
    g.add_argument("--replay", help="replay a file of recorded m12d lines instead of a device")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--port", type=int, default=8765)
    args = ap.parse_args()

    hub = Hub()
    hub.cmds = not args.replay
    if args.replay:
        up = Replay(args.replay)
    else:
        up = Upstream(tcp=None if args.serial else args.tcp, serial=args.serial, baud=args.baud)
    link = Link(send=up.send, broadcast=hub.publish)
    up.on_line = link.feed_line
    up.on_state = hub.set_link
    up.start()
    httpd = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(hub, link, args.port))
    httpd.daemon_threads = True
    print(f"helloworld: http://127.0.0.1:{args.port}/  (upstream {up.src})", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
