#!/usr/bin/env python3
"""Give the generated app icon a clean, fully opaque tile with smooth edges.

Generated artwork arrives with a hand-cut alpha channel: a one-pixel stepped
edge, a tile that is only ~99% opaque, and stray specks in the transparent
margin. This keeps every pixel of the artwork but replaces the alpha with an
analytically anti-aliased rounded square fitted to the tile, then clears the
margin. Run it before tools/make-icons.sh. Standard library only.

    python3 tools/clean-icon-mask.py assets/branding/stacks-icon.png
"""

import math
import struct
import sys
import zlib


def read_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path}: not a PNG")
    pos, idat, header = 8, b"", None
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind, chunk = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            idat += chunk
    if header is None:
        raise SystemExit(f"{path}: missing IHDR")
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour != 6 or interlace != 0:
        raise SystemExit(f"{path}: expected 8-bit non-interlaced RGBA")
    raw, stride = zlib.decompress(idat), width * 4
    pixels, previous, offset = bytearray(height * stride), bytearray(stride), 0
    for y in range(height):
        kind = raw[offset]
        line = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        for x in range(stride):
            left = line[x - 4] if x >= 4 else 0
            up = previous[x]
            corner = previous[x - 4] if x >= 4 else 0
            if kind == 1:
                line[x] = (line[x] + left) & 255
            elif kind == 2:
                line[x] = (line[x] + up) & 255
            elif kind == 3:
                line[x] = (line[x] + ((left + up) >> 1)) & 255
            elif kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                line[x] = (line[x] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        pixels[y * stride:(y + 1) * stride] = line
        previous = line
    return width, height, pixels


def write_png(path, width, height, pixels):
    stride, rows, previous = width * 4, [], bytearray(width * 4)
    for y in range(height):
        line = pixels[y * stride:(y + 1) * stride]
        filtered = bytearray(stride)
        for x in range(stride):  # Paeth filter: compact for photographic art.
            left = line[x - 4] if x >= 4 else 0
            up = previous[x]
            corner = previous[x - 4] if x >= 4 else 0
            p = left + up - corner
            pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
            filtered[x] = (line[x] - (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        rows.append(b"\x04" + bytes(filtered))
        previous = line

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
        out.write(chunk(b"IDAT", zlib.compress(b"".join(rows), 9)))
        out.write(chunk(b"IEND", b""))


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    width, height, pixels = read_png(path)
    alpha = lambda x, y: pixels[(y * width + x) * 4 + 3]

    # The tile is wherever the artwork is mostly opaque.
    rows = {}
    for y in range(height):
        xs = [x for x in range(width) if alpha(x, y) > 128]
        if xs:
            rows[y] = (xs[0], xs[-1])
    if not rows:
        raise SystemExit(f"{path}: no opaque tile found")
    top, bottom = min(rows), max(rows)
    left = min(span[0] for span in rows.values())
    right = max(span[1] for span in rows.values())

    # Fit the corner radius from how far the edge pulls in near the top:
    # for a circular corner, inset i at depth d gives r = i + d + sqrt(2id).
    estimates = []
    for depth in range(24, 72, 6):
        inset = rows[top + depth][0] - left
        if 0 < inset < depth * 6:
            estimates.append(inset + depth + math.sqrt(2 * inset * depth))
    radius = sorted(estimates)[len(estimates) // 2] if estimates else (right - left) * 0.2

    # Sit half a pixel inside the old cut so no fringe pixel stays visible.
    x0, y0, x1, y1 = left + 0.5, top + 0.5, right + 0.5, bottom + 0.5
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2 - 0.5, (y1 - y0) / 2 - 0.5
    radius = min(radius, hx, hy)

    def coverage(px, py):
        # Signed distance from the pixel centre to the rounded square.
        qx = abs(px + 0.5 - cx) - (hx - radius)
        qy = abs(py + 0.5 - cy) - (hy - radius)
        outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
        distance = outside + min(max(qx, qy), 0.0) - radius
        return min(1.0, max(0.0, 0.5 - distance))

    cleaned = bytearray(len(pixels))
    for y in range(height):
        for x in range(width):
            c = coverage(x, y)
            if c <= 0.0:
                continue
            i = (y * width + x) * 4
            sx, sy = x, y
            # Faint edge pixels carry little colour: borrow from just inside.
            while alpha(sx, sy) < 200:
                step_x = 0 if abs(sx + 0.5 - cx) <= 2 else (1 if sx < cx else -1)
                step_y = 0 if abs(sy + 0.5 - cy) <= 2 else (1 if sy < cy else -1)
                if step_x == 0 and step_y == 0:
                    break
                sx, sy = sx + step_x, sy + step_y
            j = (sy * width + sx) * 4
            cleaned[i:i + 3] = pixels[j:j + 3]
            cleaned[i + 3] = round(255 * c)

    write_png(path, width, height, cleaned)
    print(f"{path}: tile {right - left + 1}x{bottom - top + 1}px at ({left},{top}), corner radius {radius:.1f}px")


if __name__ == "__main__":
    main()
