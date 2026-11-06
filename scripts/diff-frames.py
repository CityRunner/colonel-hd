#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pixel difference of two PNG images of the same size (stage 5 safety net,
review/BACKLOG_V2.md B-18): writes an image where every differing pixel is
red and every equal pixel a dimmed grey copy of the first image, and prints
the number of differing pixels and their bounding box.

    scripts/diff-frames.py <first.png> <second.png> <difference.png>

Reads the 8-bit RGB or RGBA, non-interlaced PNGs the tools write
(tools/common/png.cpp); standard library only. Exits with 1 if the images
differ, 2 on a read error.
"""

import struct
import sys
import zlib


def read_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos, idat, header = 8, b"", None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos : pos + 8])
        body = data[pos + 8 : pos + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color not in (2, 6) or interlace != 0:
        raise ValueError(f"{path}: only 8-bit RGB/RGBA, non-interlaced")
    channels = 3 if color == 2 else 4
    raw = zlib.decompress(idat)
    stride = width * channels
    rows, previous = [], bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1 : (y + 1) * (stride + 1)])
        for i in range(stride):
            left = line[i - channels] if i >= channels else 0
            up = previous[i]
            corner = previous[i - channels] if i >= channels else 0
            if kind == 1:
                line[i] = (line[i] + left) & 0xFF
            elif kind == 2:
                line[i] = (line[i] + up) & 0xFF
            elif kind == 3:
                line[i] = (line[i] + (left + up) // 2) & 0xFF
            elif kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 0xFF
        rows.append(bytes(line[j : j + 3]) for j in range(0, stride, channels))
        rows[-1] = list(rows[-1])
        previous = line
    return width, height, rows


def write_png(path, width, height, rows):
    raw = b"".join(b"\x00" + b"".join(row) for row in rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        out.write(chunk(b"IDAT", zlib.compress(raw, 6)))
        out.write(chunk(b"IEND", b""))


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        a = read_png(sys.argv[1])
        b = read_png(sys.argv[2])
    except (OSError, ValueError, zlib.error) as error:
        print(error, file=sys.stderr)
        return 2
    if a[:2] != b[:2]:
        print(f"sizes differ: {a[0]}x{a[1]} and {b[0]}x{b[1]}")
        return 1
    width, height = a[:2]
    out, count, box = [], 0, None
    for y in range(height):
        row = []
        for x in range(width):
            pa, pb = a[2][y][x], b[2][y][x]
            if pa != pb:
                count += 1
                box = (x, y, x, y) if box is None else (min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y))
                row.append(b"\xff\x00\x00")
            else:
                row.append(bytes(c // 3 + 40 for c in pa))
        out.append(row)
    write_png(sys.argv[3], width, height, out)
    if count == 0:
        print("identical")
        return 0
    print(f"{count} pixels differ, in x {box[0]}-{box[2]}, y {box[1]}-{box[3]}; difference image {sys.argv[3]}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
