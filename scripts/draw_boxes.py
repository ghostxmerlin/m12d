#!/usr/bin/env python3
"""Draw detection results onto a 640x480 YUYV frame and write a PNG (stdlib only).

usage: draw_boxes.py <in.yuyv> <out.png> <dets>[:color] [<dets>[:color] ...]
<dets> is a file with "face x y w h prob lm.." lines, or JSON lines carrying a
"face" list (m12d detection line or snap reply). Colors: green red blue yellow
cyan magenta white. Coordinates are 640x480.
"""
import json
import struct
import sys
import zlib

W, H = 640, 480
COLORS = {"green": (0, 220, 0), "red": (255, 40, 40), "blue": (60, 120, 255), "yellow": (255, 220, 0),
          "cyan": (0, 230, 230), "magenta": (255, 0, 255), "white": (255, 255, 255)}


def yuyv_to_rgb(data):
    img = bytearray(W * H * 3)
    for i in range(0, W * H * 2, 4):
        y0, u, y1, v = data[i], data[i + 1] - 128, data[i + 2], data[i + 3] - 128
        p = (i // 2) * 3
        for k, y in enumerate((y0, y1)):
            c = 1.164 * (y - 16)
            for ch, val in enumerate((c + 1.596 * v, c - 0.392 * u - 0.813 * v, c + 2.017 * u)):
                img[p + k * 3 + ch] = 0 if val < 0 else 255 if val > 255 else int(val)
    return img


def load_faces(path):
    faces = []  # (x, y, w, h, [(lx, ly) * 5])
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line.startswith("face "):
                v = [float(x) for x in line.split()[1:]]
                faces.append((v[0], v[1], v[2], v[3], [(v[5 + 2 * k], v[6 + 2 * k]) for k in range(5)]))
            elif line.startswith("{"):
                for o in json.loads(line).get("face", []):
                    marks = [(m["x"], m["y"]) for m in o.get("mark", [])]
                    faces.append((o["x"], o["y"], o["w"], o["h"], marks))
    return faces


def put(img, x, y, rgb):
    x, y = int(round(x)), int(round(y))
    if 0 <= x < W and 0 <= y < H:
        p = (y * W + x) * 3
        img[p:p + 3] = bytes(rgb)


def draw(img, faces, rgb):
    for x, y, w, h, marks in faces:
        for t in range(2):  # 2px thick rectangle
            for xx in range(int(x), int(x + w) + 1):
                put(img, xx, y + t, rgb)
                put(img, xx, y + h - t, rgb)
            for yy in range(int(y), int(y + h) + 1):
                put(img, x + t, yy, rgb)
                put(img, x + w - t, yy, rgb)
        for lx, ly in marks:
            for dx in range(-2, 3):
                for dy in range(-2, 3):
                    put(img, lx + dx, ly + dy, rgb)


def write_png(path, img):
    raw = b"".join(b"\0" + bytes(img[y * W * 3:(y + 1) * W * 3]) for y in range(H))

    def chunk(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    if len(data) != W * H * 2:
        sys.exit(f"draw_boxes: {sys.argv[1]} is not 640x480 YUYV")
    img = yuyv_to_rgb(data)
    for spec in sys.argv[3:]:
        path, _, color = spec.partition(":")
        draw(img, load_faces(path), COLORS.get(color or "green", COLORS["green"]))
    write_png(sys.argv[2], img)


if __name__ == "__main__":
    main()
