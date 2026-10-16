"""Bilder der Ansicht Fallschirm für die Uhr: Figuren, Flugzeug, Tiere aus dem Entwurf (fallschirm_entwurf.py).

Jedes Bild wird zweimal gezeichnet (auf Schwarz und auf Weiß), daraus Deckkraft und Farbe bestimmt, beides auf die
Pebble-Palette gerundet (Farbe je Kanal 0/85/170/255, Deckkraft in 4 Stufen) und auf den Inhalt zugeschnitten.
Schreibt resources/images/fallschirm/*.png, src/c/fallschirm_bilder.h (Anker) und die Einträge in package.json.

    uv run -q --with pillow python tools/fallschirm_bilder.py
"""
import json
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fallschirm_entwurf as e
from bild_grund import Bild, W, H

APP = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
ORDNER = os.path.join(APP, 'resources', 'images', 'fallschirm')


def gesicht(b, cx=66, cy=104):
    """Frontalansicht ohne Backen und Hände (die bewegt die Uhr): Beine, Rumpf, Gesicht, Haube, Brille."""
    for x in (52, 80):
        e.kapsel(b, (x, 60), (x + (x - 66) * 0.2, 26), 9, e.ANZUG_D)
        e.form(b, [(x - 6, 22), (x + 6, 22), (x + 5, 30), (x - 5, 30)], e.SCHWARZ)
    e.form(b, [(40, 82), (92, 82), (88, 52), (44, 52)], e.ANZUG)
    b.kreis(cx, cy, 30, fill=e.HAUT, outline=e.SCHWARZ, b=1)
    e.ell(b, cx, cy + 34, 20, 14, fill=e.HAUT, rand=e.SCHWARZ)
    e.L(b, [(cx - 3, cy + 42), (cx, cy + 45), (cx + 3, cy + 42)], e.SCHWARZ, 1)
    e.ell(b, cx, cy + 18, 18, 9, fill=e.LEDER_D, rand=e.SCHWARZ)
    b.rund(cx - 15, cy + 10, cx + 15, cy + 16, 2, fill=e.WEISS)
    for x in range(-10, 15, 5):
        e.L(b, [(cx + x, cy + 10), (cx + x, cy + 16)], e.GRAU, 1)
    e.form(b, [(cx - 20, cy + 8), (cx, cy + 3), (cx + 20, cy + 8), (cx, cy + 9)], e.ROSA)
    pts = [(cx + 32 * math.cos(math.radians(w)), cy - 4 + 32 * math.sin(math.radians(w))) for w in range(180, 361, 10)]
    e.form(b, pts + [(cx + 31, cy + 14), (cx + 24, cy + 14), (cx + 24, cy - 2), (cx - 24, cy - 2), (cx - 24, cy + 14),
                     (cx - 31, cy + 14)], e.LEDER, e.LEDER_D)
    for w in (225, 270, 315):
        e.L(b, [(cx + 14 * math.cos(math.radians(w)), cy - 10 + 14 * math.sin(math.radians(w))),
                (cx + 30 * math.cos(math.radians(w)), cy - 4 + 30 * math.sin(math.radians(w)))], e.LEDER_D, 1)
    e.L(b, [(cx - 32, cy - 6), (cx + 32, cy - 6)], e.RAHMEN, 4)
    for g in (-1, 1):
        b.kreis(cx + g * 13, cy - 6, 11, fill=e.GLAS, outline=e.RAHMEN, b=3)
        b.kreis(cx + g * 11, cy - 4, 3, fill=e.SCHWARZ)
        b.kreis(cx + g * 16, cy - 10, 2, fill=e.WEISS)
        e.L(b, [(cx + g * 3, cy - 14), (cx + g * 22, cy - 22)], e.SCHWARZ, 4)
    e.form(b, [(cx - 4, cy), (cx + 4, cy), (cx + 2, cy + 6), (cx - 2, cy + 6)], e.HAUT)


def taube(b, tx=100, ty=100):
    e.ell(b, tx, ty, 13, 9, fill=e.GRAU, rand=e.SCHWARZ)
    e.form(b, [(tx - 10, ty - 2), (tx - 22, ty + 4), (tx - 10, ty + 4)], e.GRAU)
    for fx in (-4, 4):
        e.L(b, [(tx + fx, ty + 8), (tx + fx, ty + 11)], e.SCHNABEL, 1.5)
    b.kreis(tx + 8, ty - 9, 6, fill=e.GRAU, outline=e.SCHWARZ, b=1)
    b.kreis(tx + 10, ty - 10, 2.2, fill=e.WEISS, outline=e.SCHWARZ, b=1)
    e.L(b, [(tx + 7, ty - 13), (tx + 13, ty - 12)], e.SCHWARZ, 1)
    e.form(b, [(tx + 13, ty - 9), (tx + 18, ty - 8), (tx + 13, ty - 6)], e.SCHNABEL)
    b.kreis(tx + 9, ty - 4, 2, fill=(85, 170, 85))


def jumbo(b, x=96, y=100):
    e.form(b, [(x, y), (x + 32, y - 4), (x + 38, y), (x + 32, y + 3), (x, y + 3)], e.WEISS, e.GRAU_D)
    e.form(b, [(x + 14, y + 1), (x + 8, y + 10), (x + 14, y + 10), (x + 22, y + 1)], e.GRAU, e.GRAU_D)
    e.form(b, [(x + 2, y), (x - 2, y - 8), (x + 3, y - 8), (x + 7, y)], e.GRAU, e.GRAU_D)


PI = math.pi
# Name, Zeichnung, Anker (Bildschirmpunkt der Zeichnung, an den die Uhr das Bild setzt)
BILDER = [
    ('STEH', lambda b: e.figur(b, 100, 140, 0, 1.0, 1, arme='warten', beine='stehen', schal=(-1, -0.1)), (100, 140)),
    ('HOCKE', lambda b: e.figur(b, 100, 140, 0.15, 1.0, 1, arme='zurueck', beine='hocke', schal=(-1, -0.1)), (100, 140)),
    ('FLUG', lambda b: e.figur(b, 100, 130, 0, 1.15, 1, arme='tauchen', beine='tauchen', schal=(0.3, 1)), (100, 130)),
    ('SEITE', lambda b: e.figur(b, 100, 100, PI - 0.18, 1.3, -1, arme='tauchen', beine='tauchen', schal=(0.25, -1)), (100, 100)),
    ('HAENG', lambda b: e.figur(b, 100, 140, 0, 1.05, 1, arme='haengen', beine='haengen', schal=(-1, 0.3)), (100, 140)),
    ('WINK', lambda b: e.figur(b, 100, 140, 0, 1.05, 1, arme='winken', beine='haengen', schal=(-1, 0.3), mund='grinsen'), (100, 140)),
    ('ZIEH', lambda b: e.figur(b, 100, 130, 0, 1.15, 1, arme='ziehen', beine='tauchen', schal=(0.3, 1)), (100, 130)),
    ('UHR', lambda b: e.figur(b, 100, 140, 0.25, 1.2, 1, arme='uhr', beine='haengen', schal=(-1, 0.6)), (100, 140)),
    ('KLEIN', lambda b: e.figur(b, 100, 100, PI - 0.05, 0.42, -1, arme='tauchen', beine='tauchen', schal=(0.2, -1)), (100, 100)),
    ('FLUGZEUG', lambda b: e.doppeldecker(b, 0, None, 0, blatt=False), (0, 0)),
    ('GESICHT', gesicht, (66, 104)),
    ('VOGEL_A', lambda b: e.vogel(b, 100, 100, 1.0, erschrocken=False, sx=1, schlag=0.0), (100, 100)),
    ('VOGEL_B', lambda b: e.vogel(b, 100, 100, 1.0, erschrocken=False, sx=1, schlag=1.0), (100, 100)),
    ('TAUBE', taube, (100, 100)),
    ('ENTE', lambda b: e.ente(b, 100, 100, 1.0, -1), (100, 100)),
    ('JUMBO', jumbo, (96, 100)),
]

STUFEN = (0, 85, 170, 255)


def rund(v):
    return min(STUFEN, key=lambda p: abs(p - v))


def bild(zeichne):
    lagen = []
    for grund in ((0, 0, 0), (255, 255, 255)):
        b = Bild(grund)
        b.clip = []
        zeichne(b)
        lagen.append(b.gross.resize((W, H), Image.BOX))
    k, w = (l.load() for l in lagen)
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    o = out.load()
    for y in range(H):
        for x in range(W):
            pk, pw = k[x, y], w[x, y]
            a = 1 - sum(pw[i] - pk[i] for i in range(3)) / (3 * 255)
            a4 = rund(a * 255)
            if a4 == 0:
                continue
            o[x, y] = tuple(rund(min(255, pk[i] / max(a, 1e-3))) for i in range(3)) + (a4,)
    return out


def main():
    os.makedirs(ORDNER, exist_ok=True)
    anker = []
    for name, zeichne, (ax, ay) in BILDER:
        img = bild(zeichne)
        x0, y0, x1, y1 = img.getbbox()
        img.crop((x0, y0, x1, y1)).save(os.path.join(ORDNER, name.lower() + '.png'))
        anker.append((name, ax - x0, ay - y0, x1 - x0, y1 - y0))
    zeilen = ['// Automatisch erzeugt von tools/fallschirm_bilder.py — nicht von Hand ändern.', '#pragma once',
              '#include <pebble.h>', '',
              'enum { ' + ', '.join(f'FB_{n}' for n, *_ in anker) + ', FB_N };',
              'static const uint32_t FB_RES[FB_N] = { ' + ', '.join(f'RESOURCE_ID_FS_{n}' for n, *_ in anker) + ' };',
              '// Anker im Bild (x, y): dorthin zeigt die Position beim Zeichnen; Größe nur zur Info',
              'static const int16_t FB_ANKER[FB_N][2] = {']
    zeilen += [f'  {{ {x}, {y} }},   // {n} {bw} x {bh}' for n, x, y, bw, bh in anker]
    zeilen += ['};', '']
    with open(os.path.join(APP, 'src', 'c', 'fallschirm_bilder.h'), 'w') as f:
        f.write('\n'.join(zeilen))
    pfad = os.path.join(APP, 'package.json')
    pk = json.load(open(pfad))
    media = [m for m in pk['pebble']['resources']['media'] if not m['name'].startswith('FS_')]
    media += [{'type': 'bitmap', 'name': f'FS_{n}', 'file': f'images/fallschirm/{n.lower()}.png'} for n, *_ in anker]
    pk['pebble']['resources']['media'] = media
    with open(pfad, 'w') as f:
        json.dump(pk, f, indent=2, ensure_ascii=False)
        f.write('\n')
    for n, x, y, bw, bh in anker:
        print(f'{n:9} {bw:3} x {bh:3}  Anker {x},{y}')


if __name__ == '__main__':
    main()
