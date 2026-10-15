#!/usr/bin/env python3
"""App-Symbol im Phosphor-Stil: Sanduhr aus 2x2-Punkten auf schwarzer Tafel, 25 x 25 px.
Schreibt resources/images/symbol_timer.png (für die Uhr) und eine 8-fach vergrößerte Vorschau neben den Quellordner.
Aufruf: python3 tools/symbol_erzeugen.py"""
import struct, zlib
from pathlib import Path

MUSTER = [
    "########",
    ".#....#.",
    "..#**#..",
    "...##...",
    "...##...",
    "..#..#..",
    ".#.**.#.",
    "########",
]
GRUEN, HELL, AUS, TAFEL = (85, 255, 85, 255), (0, 255, 255, 255), (0, 85, 0, 255), (0, 0, 0, 255)

def bild():
    px = [[(0, 0, 0, 0)] * 25 for _ in range(25)]
    for y in range(25):
        for x in range(25):
            if min(x, 24 - x) + min(y, 24 - y) >= 2: px[y][x] = TAFEL
    for gy, zeile in enumerate(MUSTER):
        for gx, c in enumerate(zeile):
            x0, y0 = 1 + gx * 3, 1 + gy * 3
            farbe = HELL if c == "*" else GRUEN
            if c in "#*":
                for dy in range(2):
                    for dx in range(2): px[y0 + dy][x0 + dx] = farbe
            else:
                px[y0][x0] = AUS
    return px

def png(px, pfad, s=1):
    h, w = len(px) * s, len(px[0]) * s
    roh = b"".join(b"\x00" + b"".join(bytes(px[y // s][x // s]) for x in range(w)) for y in range(h))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    pfad.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(roh)) + chunk(b"IEND", b""))

basis = Path(__file__).resolve().parent.parent
p = bild()
png(p, basis / "resources" / "images" / "symbol_timer.png")
png(p, basis.parent / "timer-symbol.png", 8)
print("geschrieben: symbol_timer.png")
