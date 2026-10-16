"""Entwurf „Fallschirm" für die Timer-App: Pebble Time 2 (emery, 200 x 228, 64 Farben).
Standbilder und animierte GIFs. Formen 4-fach überabgetastet, auf die Pebble-Palette gerundet.

    uv run -q --with pillow python tools/fallschirm_entwurf.py <zielordner> [--bilder]
Die Bilder für die App macht tools/fallschirm_bilder.py aus den Zeichenfunktionen hier.

v2 (2026-10-16): Doppeldecker seitlich, Sprechblase passend, Schirm mit Leinen zum Gurt,
POV-Uhr rund mit Restzeit und rückwärts laufendem Zeiger, Anflug ohne Maßlinie, Animationen.
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFont
from bild_grund import Bild, schrift, c, W, H, SS

SCHWARZ, WEISS, NAVY = c('#000000'), c('#FFFFFF'), c('#000055')
HAUT, HAUT_D = c('#FFAAAA'), c('#AA5555')
LEDER, LEDER_D = c('#AA5500'), c('#550000')
ANZUG, ANZUG_D = c('#FF5500'), c('#AA0000')
PACK = c('#555500')
GLAS, RAHMEN = c('#AAFFFF'), c('#555555')
HIMMEL, HIMMEL_L, HIMMEL_D, HIMMEL_DD = c('#55AAFF'), c('#AAFFFF'), c('#0055AA'), c('#000055')
WOLKE_R = c('#AAAAAA')
ROSA, ROT, ROT_L, GELB, GELB_D = c('#FF55AA'), c('#FF0000'), c('#FF5555'), c('#FFFF00'), c('#AAAA00')
RASEN, RASEN_D, RASEN_L = c('#55AA00'), c('#005500'), c('#AAFF55')
WASSER, WASSER_D = c('#55AAFF'), c('#0055AA')
ENTE, SCHNABEL = c('#FFFF00'), c('#FF5500')
GB0, GB1, GB2, GB3 = c('#AAAA55'), c('#AAAA00'), c('#555500'), c('#005500')
GRAU, GRAU_D = c('#AAAAAA'), c('#555555')
HUND = c('#FFAA55')

FPS = 12.5                     # 80 ms je Bild
GESAMT = 15 * 60               # Vorführ-Timer


# ---------- Grundformen ----------

def clamp(x, a=0.0, b=1.0):
    return max(a, min(b, x))


def ease(x):
    x = clamp(x)
    return x * x * (3 - 2 * x)


def mix(a, b, t):
    if isinstance(a, (int, float)):
        return a + (b - a) * t
    return type(a)(mix(x, y, t) for x, y in zip(a, b))


def zt(sek):
    """Restzeit wie auf der Uhr: aufgerundet, m:ss."""
    s = max(0, int(math.ceil(sek - 1e-9)))
    return f'{s // 60}:{s % 60:02d}'


def rot(pts, cx, cy, a, k=1.0, sx=1):
    ca, sa = math.cos(a), math.sin(a)
    return [(cx + (x * sx * k) * ca - (y * k) * sa, cy + (x * sx * k) * sa + (y * k) * ca) for x, y in pts]


def L(b, pts, farbe, w):
    b.d.line([(x * SS, y * SS) for x, y in pts], fill=farbe, width=max(1, int(round(w * SS))), joint='curve')


def form(b, pts, farbe, rand=SCHWARZ, w=1):
    b.poly(pts, farbe)
    if rand:
        L(b, list(pts) + [pts[0]], rand, w)


def ell(b, x, y, rx, ry, fill=None, rand=None, w=1):
    b.d.ellipse([(x - rx) * SS, (y - ry) * SS, (x + rx) * SS, (y + ry) * SS], fill=fill,
                outline=rand, width=int(w * SS) if rand else 0)


def kapsel(b, p0, p1, d, farbe, rand=SCHWARZ):
    for col, dd in ((rand, d + 2), (farbe, d)):
        if col is None:
            continue
        L(b, [p0, p1], col, dd)
        for p in (p0, p1):
            b.kreis(p[0], p[1], dd / 2, fill=col)


def mitte_text(b, cx, base, s, f, fill, rand=None, rw=1):
    x = cx - f.getlength(s) / 2
    if rand:
        for dx in range(-rw, rw + 1):
            for dy in range(-rw, rw + 1):
                if dx or dy:
                    b.text(x + dx, base + dy, s, f, rand)
    b.text(x, base, s, f, fill)


def zahl(b, cx, cy, s, f, fill, rand=None, rw=2):
    """Ziffern senkrecht um cy zentriert."""
    h = -f.getbbox('0', anchor='ls')[1]
    mitte_text(b, cx, cy + h / 2, s, f, fill, rand, rw)


def wolke(b, x, y, s=1.0, farbe=WEISS, rand=WOLKE_R):
    teile = [(-14, 2, 9), (-4, -5, 12), (9, -2, 10), (18, 4, 7), (2, 5, 9)]
    for col, plus in ((rand, 1.2), (farbe, 0)):
        for dx, dy, r in teile:
            b.kreis(x + dx * s, y + dy * s, r * s + plus, fill=col)


def pille(b, x0, y0, text, f, fill=ROSA, farbe=NAVY, pfeil=True):
    w = f.getlength(text)
    x1 = x0 + w + (26 if pfeil else 16)
    b.rund(x0, y0, x1 + 12, y0 + 24, 12, fill=fill, outline=NAVY, b=1)
    b.text(x0 + 9, y0 + 17, text, f, farbe)
    if pfeil:
        px = x0 + 13 + w
        b.poly([(px, y0 + 6), (px + 8, y0 + 12), (px, y0 + 18)], farbe)


def blase(b, text, f, x0, y0, spitze):
    """Sprechblase, so breit wie der Text; spitze = Punkt, auf den der Zipfel zeigt."""
    w = f.getlength(text)
    x1, y1 = x0 + w + 22, y0 + 30
    sx = clamp(spitze[0] - 7, x0 + 14, x1 - 28)
    b.rund(x0, y0, x1, y1, 14, fill=WEISS, outline=SCHWARZ, b=1)
    b.poly([(sx, y1 - 1), (spitze[0], spitze[1]), (sx + 14, y1 - 1)], WEISS)
    L(b, [(sx, y1 - 1), (spitze[0], spitze[1]), (sx + 14, y1 - 1)], SCHWARZ, 1)
    b.text(x0 + 11, y0 + 21, text, f, NAVY)


def linien(b, t, seed, n, v, richtung='hoch', farbe=WEISS, x0=0, x1=W, laenge=(10, 34)):
    """Fahrtwindlinien, laufen mit Geschwindigkeit v (px/s)."""
    r = random.Random(seed)
    for _ in range(n):
        x, y, l = r.uniform(x0, x1), r.uniform(0, H + 40), r.uniform(*laenge)
        w = r.choice((1, 1, 2))
        if richtung == 'hoch':
            yy = (y - v * t) % (H + 60) - 30
            L(b, [(x, yy), (x, yy + l)], farbe, w)
        else:
            xx = (r.uniform(0, W + 60) - v * t) % (W + 60) - 30
            yy = r.uniform(0, H)
            L(b, [(xx, yy), (xx + l, yy)], farbe, w)


# ---------- Figur im Profil ----------
# Ursprung = Hüfte, y nach unten, Blick nach +x. Lederhaube, Fliegerbrille, Kinn, orangefarbener Anzug.

ARME = {
    'warten':  [((0, -19), (7, -11), (-1, -13)), ((1, -19), (9, -10), (0, -10))],
    'zurueck': [((0, -19), (-10, -12), (-17, -5)), ((1, -19), (-8, -11), (-15, -3))],
    'tauchen': [((0, -19), (-5, -36), (-4, -56)), ((1, -19), (-3, -36), (-1, -56))],
    'ziehen':  [((0, -19), (-5, -36), (-4, -56)), ((1, -19), (-6, -12), (-10, -5))],
    'haengen': [((0, -19), (-6, -36), (-5, -56)), ((1, -19), (-3, -36), (-1, -57))],
    'winken':  [((0, -19), (-6, -36), (-5, -56)), ((1, -19), (12, -26), (14, -42))],
    'uhr':     [((0, -19), (-3, -8), (2, -2)), ((1, -19), (9, -10), (16, -20))],
}
BEINE = {
    'stehen':  [((-2, 0), (-2, 12), (-2, 23)), ((3, 0), (3, 12), (3, 23))],
    'tippen':  [((-2, 0), (-2, 12), (-2, 23)), ((3, 0), (5, 12), (4, 21))],
    'hocke':   [((-2, 0), (7, 8), (-1, 17)), ((3, 0), (12, 8), (4, 17))],
    'tauchen': [((-2, 0), (-3, 12), (-3, 23)), ((3, 0), (2, 12), (1, 23))],
    'haengen': [((-2, 0), (-1, 12), (-3, 23)), ((3, 0), (5, 11), (4, 22))],
    'sprung':  [((-2, 0), (-9, 9), (-16, 16)), ((3, 0), (-3, 11), (-10, 20))],
}


def pose(tab, x):
    return tab[x] if isinstance(x, str) else x


def figur(b, cx, cy, a=0.0, k=1.3, sx=1, arme='tauchen', beine='tauchen', schal=(-1, 0), mund='grimmig',
          phase=0.0, brille_schief=0.0):
    P = lambda pts: rot(pts, cx, cy, a, k, sx)
    p = lambda x, y: P([(x, y)])[0]
    d = lambda v: v * k
    arme, beine = pose(ARME, arme), pose(BEINE, beine)

    def glied(pts, dicke, farbe, hand=None, stiefel=False, rand=SCHWARZ):
        q = P(pts)
        kapsel(b, q[0], q[1], d(dicke), farbe, rand)
        kapsel(b, q[1], q[2], d(dicke * 0.9), farbe, rand)
        if hand:
            b.kreis(q[2][0], q[2][1], d(3.2), fill=hand, outline=SCHWARZ, b=1)
        if stiefel:
            fx, fy = pts[2]
            form(b, P([(fx - 3, fy - 2), (fx + 3, fy - 2), (fx + 7, fy + 1), (fx + 7, fy + 3), (fx - 3, fy + 3)]), SCHWARZ)

    glied(arme[0], 5, ANZUG_D, hand=LEDER_D)
    glied(beine[0], 6, ANZUG_D, stiefel=True)
    # Schal: weht in Bildrichtung schal (Bildschirmkoordinaten), Welle läuft mit phase
    hx, hy = p(-1, -22)
    vx, vy = schal
    nx, ny = -vy, vx
    pts_o, pts_u = [], []
    for i in range(7):
        t = i / 6
        welle = math.sin(t * 9 - phase) * d(2.8) * t
        bx, by = hx + vx * d(30) * t + nx * welle, hy + vy * d(30) * t + ny * welle
        breite = d(3.2) * (1 - 0.5 * t)
        pts_o.append((bx + nx * breite, by + ny * breite))
        pts_u.append((bx - nx * breite, by - ny * breite))
    form(b, pts_o + pts_u[::-1], WEISS, SCHWARZ)
    form(b, P([(-14, -21), (-6, -22), (-6, -2), (-14, -3)]), PACK)
    form(b, P([(-7, 1), (7, 1), (8, -11), (6, -22), (-6, -22), (-8, -10)]), ANZUG)
    L(b, P([(-6, -21), (6, -4), (7, 1)]), SCHWARZ, d(1.3))
    L(b, P([(-7, -2), (7, -2)]), SCHWARZ, d(1.3))
    b.kreis(*p(5, -3), d(1.6), fill=GELB, outline=SCHWARZ, b=1)   # Reißgriff
    glied(beine[1], 6, ANZUG, stiefel=True)
    hx, hy = p(3, -35)
    b.kreis(hx, hy, d(12), fill=HAUT, outline=SCHWARZ, b=1)
    form(b, P([(6, -31), (16, -30), (19, -24), (14, -19), (4, -22)]), HAUT)     # Riesenkinn
    L(b, P([(14, -21), (16, -20)]), SCHWARZ, 1)
    form(b, P([(13, -39), (19, -34), (13, -33)]), HAUT)                          # Nase
    if mund == 'grimmig':
        L(b, P([(10, -28), (16, -28.5)]), SCHWARZ, d(1.2))
    elif mund == 'schrei':
        form(b, P([(9, -30), (17, -30), (16, -23), (10, -24)]), LEDER_D)
        L(b, P([(10, -29), (16, -29)]), WEISS, d(1.2))
    elif mund == 'grinsen':
        L(b, P([(9, -29), (13, -26), (17, -29)]), SCHWARZ, d(1.2))
    kap = [(3 + 12.5 * math.cos(math.radians(t)), -35 + 12.5 * math.sin(math.radians(t))) for t in range(150, 341, 10)]
    kap += [(13, -42), (6, -41), (0, -37), (-1, -29), (-5, -26), (-9, -30)]
    form(b, P(kap), LEDER, LEDER_D)
    L(b, P([(-6, -42), (6, -46)]), LEDER_D, 1)
    L(b, P([(-3, -46), (10, -45)]), LEDER_D, 1)
    form(b, P([(-3, -36), (3, -35), (3, -28), (-2, -26)]), LEDER, LEDER_D)
    L(b, P([(1, -27), (7, -22)]), LEDER_D, d(1))
    bs = brille_schief
    L(b, P([(-8, -40 - bs), (8, -38 - bs)]), RAHMEN, d(2))
    gx, gy = p(12, -37 - bs)
    b.kreis(gx, gy, d(5), fill=GLAS, outline=RAHMEN, b=2)
    b.kreis(*p(14, -36.5), d(2.2 if bs else 1.6), fill=SCHWARZ)
    b.kreis(*p(10.5, -39 - bs), d(1), fill=WEISS)
    L(b, P([(6, -45 - bs), (18, -40.5 - bs)]), SCHWARZ, d(2.6))
    glied(arme[1], 5, ANZUG, hand=LEDER)
    return p


# ---------- Requisiten ----------

def vogel(b, x, y, s=1.0, erschrocken=True, sx=1, schlag=0.0):
    X = lambda dx: x + dx * s * sx
    ell(b, x, y, 9 * s, 6 * s, fill=WEISS, rand=SCHWARZ)
    fy = -13 + 16 * schlag
    form(b, [(X(-2), y - 2 * s), (X(-14), y + fy * s), (X(4), y - 4 * s)], WEISS)
    form(b, [(X(8), y - 4 * s), (X(16), y - 2 * s), (X(8), y)], SCHNABEL)
    b.kreis(X(4), y - 3 * s, 2.6 * s, fill=WEISS, outline=SCHWARZ, b=1)
    b.kreis(X(4.6), y - 3 * s, 1.1 * s, fill=SCHWARZ)
    form(b, [(X(-9), y), (X(-15), y + 3 * s), (X(-9), y + 3 * s)], WEISS)
    if erschrocken:
        b.text(X(8), y - 10 * s, '!', schrift(14, 700), ROT)


def ente(b, x, y, s=1.0, sx=1, blick='hoch'):
    X = lambda dx: x + dx * s * sx
    ell(b, x, y, 9 * s, 5.5 * s, fill=ENTE, rand=SCHWARZ)
    form(b, [(X(-9), y - 2 * s), (X(-13), y - 6 * s), (X(-7), y - 3 * s)], ENTE)
    b.kreis(X(5), y - 7 * s, 5 * s, fill=ENTE, outline=SCHWARZ, b=1)
    form(b, [(X(9), y - 8 * s), (X(14), y - 6 * s), (X(9), y - 4.5 * s)], SCHNABEL)
    if blick == 'hoch':
        b.kreis(X(6), y - 9 * s, 2.2 * s, fill=WEISS, outline=SCHWARZ, b=1)
        b.kreis(X(6.4), y - 10 * s, 1 * s, fill=SCHWARZ)
    else:
        b.kreis(X(6.5), y - 8 * s, 1 * s, fill=SCHWARZ)


def becken(b, cx, cy, rx, ry, rand_h, hinten=True, vorn=True):
    if hinten:
        ell(b, cx, cy, rx, ry, fill=WASSER, rand=WASSER_D, w=1)
    if vorn:
        pts = [(cx - rx + 2 * i * rx / 40, cy + ry * math.sqrt(max(0, 1 - ((2 * i / 40) - 1) ** 2))) for i in range(41)]
        unten = [(x, y + rand_h) for x, y in pts]
        b.poly(pts + unten[::-1], WEISS)
        for i in range(0, 40, 4):
            q = pts[i:i + 3]
            b.poly(q + [(x, y + rand_h) for x, y in q[::-1]], ROSA)
        L(b, pts, SCHWARZ, 1)
        L(b, unten, SCHWARZ, 1)
        L(b, [pts[0], unten[0]], SCHWARZ, 1)
        L(b, [pts[-1], unten[-1]], SCHWARZ, 1)


def doppeldecker(b, t, zeit, oy=0.0, blatt=True):
    """Roter Doppeldecker streng von der Seite, Nase rechts. Zeit als Schriftzug auf dem Rumpf."""
    o = lambda pts: [(x, y + oy) for x, y in pts]
    for x in (138, 166):                                                    # Flügelstreben, hinter dem Rumpf
        form(b, o([(x, 114), (x + 4, 114), (x + 4, 190), (x, 190)]), LEDER)
    L(b, o([(140, 116), (168, 188)]), GRAU_D, 1)
    L(b, o([(168, 116), (140, 188)]), GRAU_D, 1)
    form(b, o([(4, 152), (12, 110), (30, 110), (44, 142)]), ROT)            # Seitenleitwerk
    form(b, o([(10, 118), (28, 118), (31, 126), (12, 126)]), WEISS, None)
    form(b, o([(0, 154), (34, 149), (42, 153), (34, 157)]), GELB, GELB_D)   # Höhenleitwerk
    # Pilot (Hund) im offenen Sitz, Ohr und Schal flattern
    flat = math.sin(t * 14)
    form(b, o([(92, 128), (70 - 2 * flat, 120 + 3 * flat), (72, 131), (92, 134)]), WEISS)
    b.kreis(98, 126 + oy, 9, fill=HUND, outline=SCHWARZ, b=1)
    form(b, o([(92, 121), (78, 117 + 3 * flat), (90, 128)]), LEDER)
    L(b, o([(90, 123), (108, 123)]), RAHMEN, 2)
    b.kreis(103, 123 + oy, 3.5, fill=GLAS, outline=RAHMEN, b=1)
    form(b, o([(105, 128), (111, 129), (106, 132)]), SCHWARZ)
    form(b, o([(4, 152), (44, 140), (176, 136), (188, 142), (192, 152), (192, 178), (182, 188),
               (150, 194), (44, 186), (4, 160)]), ROT)                      # Rumpf
    L(b, o([(44, 146), (176, 142)]), ROT_L, 2)
    L(b, o([(44, 182), (150, 190)]), c('#AA0000'), 2)
    b.rund(84, 132 + oy, 118, 139 + oy, 3, fill=SCHWARZ)                    # Sitzöffnung
    form(b, o([(118, 137), (124, 124), (127, 137)]), GLAS, RAHMEN)          # Windschutz
    for x in (130, 160):                                                    # Baldachinstreben
        L(b, o([(x, 137), (x + 6, 114)]), LEDER_D, 3)
    b.rund(178, 140 + oy, 198, 190 + oy, 6, fill=GRAU_D, outline=SCHWARZ, b=1)
    for y in (150, 160, 170, 180):
        L(b, o([(182, y), (194, y)]), SCHWARZ, 1)
    ell(b, 202, 165 + oy, 3, 32, fill=None, rand=GRAU, w=1)                 # Propellerkreis
    if blatt:
        lp = 30 * math.cos(t * 31)
        L(b, o([(201, 165 - lp), (201, 165 + lp)]), GRAU_D, 4)
        b.kreis(201, 165 + oy, 5, fill=GRAU, outline=SCHWARZ, b=1)
    form(b, o([(112, 112), (140, 106), (170, 104), (180, 109), (174, 115), (112, 114)]), GELB, GELB_D)
    form(b, o([(112, 190), (140, 187), (168, 186), (178, 190), (170, 196), (112, 193)]), GELB, GELB_D)
    L(b, o([(146, 194), (134, 212)]), SCHWARZ, 2)                           # Fahrwerk
    L(b, o([(162, 192), (134, 212)]), SCHWARZ, 2)
    b.kreis(134, 214 + oy, 9, fill=SCHWARZ)
    b.kreis(134, 214 + oy, 3, fill=GRAU)
    if zeit:
        zahl(b, 80, 164 + oy, zeit, schrift(32, 700), WEISS, NAVY, 2)


def kappe(b, kx, ky, rx, ry, auf=1.0, bahnen=8):
    """Rundkappe von der Seite. auf < 1: noch nicht offen (schmal). Gibt die Randpunkte zurück."""
    rx_e = rx * (0.12 + 0.88 * auf)
    ry_e = ry * (0.55 + 0.45 * min(auf, 1.1))

    def meridian(i):
        xi = kx - rx_e + 2 * rx_e * i / bahnen
        return [(kx + (xi - kx) * math.sin(u * math.pi / 2), ky - ry_e * math.cos(u * math.pi / 2))
                for u in [j / 12 for j in range(13)]]
    rand = []
    for i in range(bahnen):
        m0, m1 = meridian(i), meridian(i + 1)
        x0, x1 = m0[-1][0], m1[-1][0]
        welle = [(x0 + (x1 - x0) * j / 6, ky + 6 * min(auf, 1) * math.sin(math.pi * j / 6)) for j in range(1, 6)]
        form(b, m0 + welle + m1[::-1], ROSA if i % 2 == 0 else WEISS, SCHWARZ)
        rand.append((x0, ky))
    rand.append((kx + rx_e, ky))
    return rand


def leinen(b, rand, kx, ky, schultern, anteil=0.35):
    """Fangleinen vom Kappenrand zum Verbindungsglied über den Schultern, von dort Tragegurte zum Gurtzeug."""
    mx = sum(s[0] for s in schultern) / len(schultern)
    my = sum(s[1] for s in schultern) / len(schultern)
    knoten = (mx + (kx - mx) * anteil, my + (ky - my) * anteil)
    for p in rand:
        L(b, [p, knoten], GRAU_D, 1)
    for s in schultern:
        L(b, [knoten, s], PACK, 2)
    return knoten


# ---------- Höhenband ----------

def band(b, rest, cy=118, x0=146, dx=0.0):
    """Restzeit statt Höhe, 1 Minute = 44 px. Fallend läuft das Band nach oben."""
    x0 += dx
    b.d.rectangle([int(x0 * SS), 0, (W + int(dx) + 60) * SS, H * SS], fill=GRAU_D)
    L(b, [(x0, 0), (x0, H)], SCHWARZ, 1)
    f = schrift(13, 600)
    for m in range(0, GESAMT // 60 + 1):
        for v in range(4):
            sek = m * 60 + v * 15
            y = cy + (rest - sek) / 60 * 44
            if -10 < y < H + 10:
                L(b, [(x0, y), (x0 + (10 if v == 0 else 5), y)], WEISS, 1)
                if v == 0 and not (cy - 30 < y < cy + 30) and y > 40:
                    b.text(x0 + 13, y + 5, f'{m}', f, WEISS)
    fz = schrift(30, 700)
    bx0 = 120 + dx
    form(b, [(bx0, cy - 20), (W + 2 + dx, cy - 20), (W + 2 + dx, cy + 20), (bx0, cy + 20), (bx0 - 8, cy)], SCHWARZ, WEISS, 2)
    # Sekunden-Trommel: kurz vor dem Wechsel rollt die letzte Ziffer
    zeit = zt(rest)
    frac = rest - math.floor(rest)
    roll = ease((0.25 - frac) / 0.25) if 0 < frac < 0.25 and zeit[-1] != '0' else 0.0
    vor, letzte = zeit[:-1], zeit[-1]
    h = -fz.getbbox('0', anchor='ls')[1]
    base = cy + h / 2
    x = (bx0 + W + dx) / 2 - fz.getlength(zeit) / 2 + 2
    b.text(x, base, vor, fz, WEISS)
    xl = x + fz.getlength(vor)
    box = (int(xl - 1), int(cy - 18), int(xl + fz.getlength('0') + 2), int(cy + 18))
    b.clip.append((xl, base - roll * 30, letzte, fz, WEISS, box))
    b.clip.append((xl, base - roll * 30 + 30, str((int(letzte) - 1) % 10), fz, WEISS, box))
    b.text(151 + dx, 222, 'REST', schrift(12, 600), WEISS)


# ---------- Bildaufbau ----------

def neu(grund):
    b = Bild(grund)
    b.clip = []
    return b


def fertig(b):
    img = b.fertig()
    for x, y, s, f, fill, box in b.clip:
        m = Image.new('L', img.size, 0)
        dm = ImageDraw.Draw(m)
        dm.fontmode = '1'
        dm.text((round(x), round(y)), s, font=f, fill=255, anchor='ls')
        maske = Image.new('L', img.size, 0)
        maske.paste(m.crop(box), box[:2])
        img.paste(fill, (0, 0, img.size[0], img.size[1]), maske)
    return img


# ---------- Szenen (je Funktion: Zeit t → Bild) ----------

def s_flugzeug(t, t_start=None, rest0=GESAMT):
    """Bereit auf dem Flügel; ab t_start Absprung. Kamera folgt ihm, das Flugzeug zieht nach oben weg,
    das Höhenband fliegt von rechts ein. Bis dahin steht die Zeit auf dem Rumpf."""
    ts = None if t_start is None or t < t_start else t - t_start
    rest = rest0 if ts is None else rest0 - ts
    hx, hy0 = 146, 82
    # Flugbahn in Weltkoordinaten, Kamera hält ihn höchstens bei y = 104
    x = y = None
    cam = 0.0
    if ts is not None and ts >= 0.35:
        u = ts - 0.35
        x = hx + 14 * math.sin(min(u, 0.6) / 0.6 * math.pi) - 80 * ease(u / 1.8)
        y = hy0 + 6 - 34 * u + 120 * u * u
        cam = max(0.0, y - 104)
    b = neu(HIMMEL)
    for x0, wy, s_ in ((60, 50, 1.1), (190, 86, 0.8), (130, 30, 0.6), (20, 104, 0.7), (90, 300, 1.0), (170, 420, 0.8)):
        yy = wy - cam * 0.6
        if -30 < yy < H + 30:
            wolke(b, (x0 - 55 * t) % 280 - 40, yy, s_)
    linien(b, t, 3, 12, 260, 'quer')
    if cam > 4:
        linien(b, t, 9, int(min(16, cam / 6)), 300, 'hoch', x0=0, x1=140)
    oy = 1.5 * math.sin(t * 3) - cam
    if oy > -240:
        doppeldecker(b, t, zt(rest), oy)
    hy = hy0 + oy + cam
    if ts is None:
        tip = 'tippen' if int(t * 4) % 2 else 'stehen'
        figur(b, hx, hy, 0, 0.95, 1, arme='warten', beine=tip, schal=(-1, -0.1), phase=t * 14)
        pille(b, 116, 6, 'Start', schrift(15, 600))
    elif ts < 0.35:
        u = ease(ts / 0.35)
        figur(b, hx, hy + 6 * u, 0.15 * u, 0.95, 1, arme=mix(ARME['warten'], ARME['zurueck'], u),
              beine=mix(BEINE['stehen'], BEINE['hocke'], u), schal=(-1, -0.1), phase=t * 14)
        if ts < 0.15:
            pille(b, 116, 6, 'Start', schrift(15, 600), fill=GELB)
    else:
        u = ts - 0.35
        a = 2.75 * ease(u / 0.9)
        arme = mix(ARME['zurueck'], ARME['tauchen'], ease(u / 0.25))
        beine = mix(BEINE['hocke'], BEINE['sprung'], ease(u / 0.2))
        if u > 0.5:
            beine = mix(BEINE['sprung'], BEINE['tauchen'], ease((u - 0.5) / 0.5))
        if u < 0.6:                                               # Staubwölkchen vom Absprung
            for i, dx in enumerate((-8, 4, 14)):
                b.kreis(hx + dx - 20 * u, 104 + oy - 4 * u * i, 2 + 8 * u, fill=WEISS, outline=WOLKE_R, b=1)
        p = figur(b, x, y - cam + 1.5 * math.sin(t * 3), a, 0.95 + 0.25 * ease((u - 0.6) / 1.0), 1,
                  arme=arme, beine=beine, schal=(-1, -0.5), mund='schrei', phase=t * 18)
        if u < 1.6:
            kx, ky = p(14, -30)
            blase(b, 'GERONIMOOO!', schrift(16, 700), 6, 8, (clamp(kx, 30, 140), clamp(ky - 12, 52, 200)))
        if cam > 120:                                             # Band fliegt ein, sobald der Rumpf weg ist
            band(b, rest, dx=(1 - ease((cam - 120) / 90)) * 100)
    return fertig(b)


VOGEL_SEITE = [(118, 0.8, 4.5, -1), (34, 3.0, 4.5, 1)]


def s_seite(t, rest):
    b = neu(HIMMEL)
    linien(b, t, 7, 22, 300, 'hoch', x0=0, x1=140)
    wolke(b, 30, H + 50 - ((t * 140 + 40) % 420), 1.3)
    wolke(b, 104, H + 50 - ((t * 140 + 250) % 420), 0.8)
    jy = 70 - 18 * t                                              # Jumbo, langsam
    form(b, [(96, jy), (128, jy - 4), (134, jy), (128, jy + 3), (96, jy + 3)], WEISS, GRAU_D)
    form(b, [(110, jy + 1), (104, jy + 10), (110, jy + 10), (118, jy + 1)], GRAU, GRAU_D)
    form(b, [(98, jy), (94, jy - 8), (99, jy - 8), (103, jy)], GRAU, GRAU_D)
    for x, t0, periode, sx in VOGEL_SEITE:                        # Vögel kommen von unten
        u = (t - t0) % periode
        y = H + 30 - u * 170
        if -30 < y < H + 30:
            vogel(b, x, y, 1.0, sx=-sx, schlag=0.5 + 0.5 * math.sin(t * 20))
            for dx in (-6, 0, 6):
                L(b, [(x + dx, y + 10), (x + dx, y + 24)], WEISS, 1)
    a = math.pi - 0.18 + 0.05 * math.sin(t * 4)
    figur(b, 66, 82 + 2 * math.sin(t * 3), a, 1.35, -1, arme='tauchen', beine='tauchen', schal=(0.25, -1), phase=t * 18)
    band(b, rest)
    return fertig(b)


def uhr_rund(b, cx, cy, r, rest, richtung):
    """Runde Uhr am Handgelenk: Restzeit digital, gelber Ring = Restsekunden der Minute, roter Zeiger läuft rückwärts."""
    dx, dy = richtung
    nx, ny = -dy, dx
    for g in (-1, 1):
        a0 = (cx + g * dx * (r - 6), cy + g * dy * (r - 6))
        a1 = (cx + g * dx * (r + 14), cy + g * dy * (r + 14))
        form(b, [(a0[0] + nx * 16, a0[1] + ny * 16), (a1[0] + nx * 15, a1[1] + ny * 15),
                 (a1[0] - nx * 15, a1[1] - ny * 15), (a0[0] - nx * 16, a0[1] - ny * 16)], LEDER_D)
    b.kreis(cx, cy, r + 5, fill=GRAU, outline=SCHWARZ, b=1)
    b.kreis(cx, cy, r, fill=NAVY)
    sek = rest % 60
    for i in range(60):
        wi = math.radians(-90 + i * 6)
        lang = i % 5 == 0
        ri = r - (7 if lang else 4)
        L(b, [(cx + ri * math.cos(wi), cy + ri * math.sin(wi)), (cx + (r - 1) * math.cos(wi), cy + (r - 1) * math.sin(wi))],
          GRAU if i < sek else GRAU_D, 1.5 if lang else 1)
    if sek > 0.05:
        b.d.arc([(cx - r + 10) * SS, (cy - r + 10) * SS, (cx + r - 10) * SS, (cy + r - 10) * SS],
                -90, -90 + 360 * sek / 60, fill=GELB, width=4 * SS)
    wz = math.radians(-90 + 360 * sek / 60)
    L(b, [(cx + (r - 20) * math.cos(wz), cy + (r - 20) * math.sin(wz)), (cx + (r - 1) * math.cos(wz), cy + (r - 1) * math.sin(wz))], ROT, 3)
    mitte_text(b, cx, cy - 12, 'NOCH', schrift(10, 600), GELB)
    zahl(b, cx, cy + 6, zt(rest), schrift(30, 700), WEISS)
    b.rund(cx + r + 3, cy - 6, cx + r + 8, cy + 6, 1, fill=GRAU_D)


def s_pov(t, rest, fortschritt=0.4):
    b = neu(HIMMEL_D)
    for x, y in ((14, 14), (60, 30), (170, 20), (130, 8), (190, 60), (8, 70), (40, 110), (176, 118)):
        b.kreis(x, y, 1, fill=WEISS)
    ex, ey = 100, 58
    er = 20 + 16 * fortschritt + 0.4 * t
    b.kreis(ex, ey, er + 3, fill=HIMMEL_L)
    b.kreis(ex, ey, er, fill=c('#0055FF'), outline=NAVY, b=1)
    s = er / 30
    for kont in ([(82, 66), (96, 60), (104, 70), (98, 82), (86, 84), (78, 76)],
                 [(106, 86), (118, 80), (124, 92), (114, 106), (104, 100)],
                 [(78, 94), (88, 92), (90, 102), (82, 104)]):
        form(b, [(ex + (x - 100) * s, ey + (y - 84) * s) for x, y in kont], RASEN, None)
    b.kreis(ex + (98 - 100) * s, ey + (75 - 84) * s, 1.6, fill=ROSA)
    for i in range(5):                                            # Wolkenfetzen fliegen nach außen
        u = (t * 0.45 + i / 5) % 1
        w = math.radians(30 + i * 73)
        rr = er + 6 + u * 120
        wolke(b, ex + rr * math.cos(w), ey + rr * math.sin(w), 0.15 + 0.6 * u)
    for i in range(18):
        w = i / 18 * 2 * math.pi + 0.2
        r0 = er + 14 + (i * 23 + t * 160) % 110
        L(b, [(ex + r0 * math.cos(w), ey + r0 * math.sin(w)),
              (ex + (r0 + 14) * math.cos(w), ey + (r0 + 14) * math.sin(w))], WEISS, 1)
    zit = math.sin(t * 25) * 0.8                                  # Arme zittern im Fahrtwind
    for (x0, x1, hx, hy) in ((-50, 64, 78, 108 + zit), (136, 250, 122, 108 - zit)):
        form(b, [(x0, 250), (x1, 250), (hx + 12, hy + 14), (hx - 12, hy + 14)], ANZUG)
        for k in range(1, 4):
            u = k / 4
            ya = 250 + (hy + 14 - 250) * u
            xa = x0 + (hx - 12 - x0) * u
            xb = x1 + (hx + 12 - x1) * u
            L(b, [(xa + 4, ya), (xa + (xb - xa) * 0.4, ya + 3)], ANZUG_D, 1)
        g = 1 if hx < 100 else -1
        ell(b, hx + g * 12, hy + 2, 6, 8, fill=LEDER, rand=SCHWARZ)
        b.rund(hx - 12, hy - 26, hx + 12, hy + 12, 10, fill=LEDER, outline=SCHWARZ, b=1)
        for fx in (-6, 0, 6):
            L(b, [(hx + fx, hy - 24), (hx + fx, hy - 12)], LEDER_D, 1)
        L(b, [(hx - 12, hy + 8), (hx + 12, hy + 8)], LEDER_D, 2)
    ax, ay = 162, 188                                             # Höhenmesser rechts
    b.kreis(ax, ay, 22, fill=WEISS, outline=SCHWARZ, b=2)
    for i in range(10):
        w = i / 10 * 2 * math.pi - math.pi / 2
        L(b, [(ax + 16 * math.cos(w), ay + 16 * math.sin(w)), (ax + 20 * math.cos(w), ay + 20 * math.sin(w))], SCHWARZ, 1)
    wa = -math.pi / 2 + 2 * math.pi * (rest / GESAMT)
    L(b, [(ax, ay), (ax + 14 * math.cos(wa), ay + 14 * math.sin(wa))], ROT, 2)
    b.kreis(ax, ay, 2, fill=SCHWARZ)
    b.text(ax - 8, ay + 14, 'ALT', schrift(9, 600), SCHWARZ)
    n = math.hypot(114, 142)
    uhr_rund(b, 58, 174, 48, rest, (142 / n, 114 / n))
    return fertig(b)


def s_frontal(t, rest):
    b = neu(HIMMEL)
    cx, cy = 66, 104 + 2 * math.sin(t * 7)
    for i in range(28):
        w = i / 28 * 2 * math.pi
        r0 = 50 + (i * 37 + t * 220) % 90
        L(b, [(cx + r0 * math.cos(w), cy + r0 * math.sin(w)),
              (cx + (r0 + 26) * math.cos(w), cy + (r0 + 26) * math.sin(w))], HIMMEL_L, 2)
    for x in (52, 80):
        kapsel(b, (x, 60), (x + (x - 66) * 0.2, 26), 9, ANZUG_D)
        form(b, [(x - 6, 22), (x + 6, 22), (x + 5, 30), (x - 5, 30)], SCHWARZ)
    form(b, [(40, 82), (92, 82), (88, 52), (44, 52)], ANZUG)
    for g in (-1, 1):                                             # Backen: Welle läuft nach außen
        aussen = [(cx + g * (40 + 5 * math.sin(k * 1.9 - t * 22)), cy + 22 - k * 4 + 2 * math.sin(t * 22 + k)) for k in range(7)]
        pts = [(cx + g * 18, cy + 26)] + aussen + [(cx + g * 26, cy - 2)]
        form(b, pts, HAUT)
        for k in (1, 3, 5):
            x, y = aussen[k]
            L(b, [(x - g * 12, y + 1), (x - g * 3, y)], HAUT_D, 1)
    b.kreis(cx, cy, 30, fill=HAUT, outline=SCHWARZ, b=1)
    ell(b, cx, cy + 34, 20, 14, fill=HAUT, rand=SCHWARZ)
    L(b, [(cx - 3, cy + 42), (cx, cy + 45), (cx + 3, cy + 42)], SCHWARZ, 1)
    ell(b, cx, cy + 18, 18, 9 + 2 * math.sin(t * 30), fill=LEDER_D, rand=SCHWARZ)
    b.rund(cx - 15, cy + 10, cx + 15, cy + 16, 2, fill=WEISS)
    for x in range(-10, 15, 5):
        L(b, [(cx + x, cy + 10), (cx + x, cy + 16)], GRAU, 1)
    lw = 2 * math.sin(t * 30)
    form(b, [(cx - 20, cy + 8), (cx, cy + 3 - lw), (cx + 20, cy + 8), (cx, cy + 9)], ROSA)
    pts = [(cx + 32 * math.cos(math.radians(w)), cy - 4 + 32 * math.sin(math.radians(w))) for w in range(180, 361, 10)]
    form(b, pts + [(cx + 31, cy + 14), (cx + 24, cy + 14), (cx + 24, cy - 2), (cx - 24, cy - 2), (cx - 24, cy + 14), (cx - 31, cy + 14)], LEDER, LEDER_D)
    for w in (225, 270, 315):
        L(b, [(cx + 14 * math.cos(math.radians(w)), cy - 10 + 14 * math.sin(math.radians(w))),
              (cx + 30 * math.cos(math.radians(w)), cy - 4 + 30 * math.sin(math.radians(w)))], LEDER_D, 1)
    L(b, [(cx - 32, cy - 6), (cx + 32, cy - 6)], RAHMEN, 4)
    for g in (-1, 1):
        b.kreis(cx + g * 13, cy - 6, 11, fill=GLAS, outline=RAHMEN, b=3)
        b.kreis(cx + g * 11, cy - 4, 3, fill=SCHWARZ)
        b.kreis(cx + g * 16, cy - 10, 2, fill=WEISS)
        L(b, [(cx + g * 3, cy - 14), (cx + g * 22, cy - 22)], SCHWARZ, 4)
    form(b, [(cx - 4, cy), (cx + 4, cy), (cx + 2, cy + 6), (cx - 2, cy + 6)], HAUT)
    for hx, hy, r in ((22, 186, 24), (104, 200, 22)):
        hy += 1.5 * math.sin(t * 9 + hx)
        kapsel(b, (66 + (hx - 66) * 0.3, 140), (hx, hy), 18, ANZUG)
        b.kreis(hx, hy, r, fill=LEDER, outline=SCHWARZ, b=1)
        for i in range(4):
            fx = hx - r * 0.6 + i * r * 0.4
            L(b, [(fx, hy - r * 0.9), (fx, hy - r * 0.3)], LEDER_D, 1)
    band(b, rest)
    # Taube hat sich auf das Ablesefenster gesetzt und putzt sich
    tx, ty = 170, 88
    ell(b, tx, ty, 13, 9, fill=GRAU, rand=SCHWARZ)
    form(b, [(tx - 10, ty - 2), (tx - 22, ty + 4), (tx - 10, ty + 4)], GRAU)
    for fx in (-4, 4):
        L(b, [(tx + fx, ty + 8), (tx + fx, ty + 11)], SCHNABEL, 1.5)
    nick = 2 * math.sin(t * 9)
    b.kreis(tx + 8, ty - 9 + nick, 6, fill=GRAU, outline=SCHWARZ, b=1)
    b.kreis(tx + 10, ty - 10 + nick, 2.2, fill=WEISS, outline=SCHWARZ, b=1)
    L(b, [(tx + 7, ty - 13 + nick), (tx + 13, ty - 12 + nick)], SCHWARZ, 1)
    form(b, [(tx + 13, ty - 9 + nick), (tx + 18, ty - 8 + nick), (tx + 13, ty - 6 + nick)], SCHNABEL)
    b.kreis(tx + 9, ty - 4 + nick, 2, fill=c('#55AA55'))
    if int(t * 2) % 4 == 0:
        b.text(tx - 24, ty - 16, '...', schrift(13, 700), NAVY)
    return fertig(b)


def s_anflug(t, rest, z=1.0):
    """Senkrechte Übersicht, zoomt auf das Becken (104/200) zu. Ohne Maßlinie."""
    b = neu(HIMMEL)
    for x, y0, s in ((30, 60, 0.8), (170, 130, 0.7), (90, 200, 0.9)):
        wolke(b, x, (y0 - 70 * t) % 260 - 30, s)
    T = lambda x, y: (66 + (x - 104) * z, 200 + (y - 200) * z)
    TP = lambda pts: [T(x, y) for x, y in pts]
    yh = T(0, 150)[1]
    b.d.rectangle([0, int(yh * SS), W * SS, H * SS], fill=RASEN)
    for x, y, rx, ry in ((30, 152, 60, 8), (170, 154, 50, 7)):
        px, py = T(x, y)
        ell(b, px, py, rx * z, ry * z, fill=RASEN_L)
    form(b, TP([(-200, 150), (400, 150), (400, 158), (-200, 158)]), RASEN, None)
    form(b, TP([(8, 168), (52, 168), (52, 196), (8, 196)]), c('#FFFFAA'))
    form(b, TP([(2, 170), (30, 150), (58, 170)]), c('#AA0000'))
    form(b, TP([(18, 178), (30, 178), (30, 188), (18, 188)]), HIMMEL_L)
    form(b, TP([(36, 180), (46, 180), (46, 196), (36, 196)]), LEDER)
    for x in range(150, 230, 12):
        px, py = T(x, 168)
        b.kreis(px, py, 8 * z, fill=RASEN_D, outline=SCHWARZ, b=1)
    form(b, TP([(150, 208), (154, 196), (158, 208)]), ROT)
    px, py = T(154, 210)
    b.kreis(px, py, 3 * z, fill=HAUT, outline=SCHWARZ, b=1)
    form(b, TP([(150, 212), (158, 212), (158, 220), (150, 220)]), c('#0055AA'))
    for rx, ry in ((60, 20), (44, 14)):
        ell(b, 66, 200, rx * z, ry * z, rand=WEISS, w=2)
    becken(b, 66, 198, 26 * z, 8 * z, 7 * z)
    ente(b, *T(112, 197), 0.35 * z, -1)
    figur(b, 66 + 3 * math.sin(t * 2), 40, math.pi - 0.05, 0.42, -1, arme='tauchen', beine='tauchen',
          schal=(0.2, -1), phase=t * 18)
    band(b, rest)
    b.text(6, 22, f'ZOOM x{round(12 * z)}', schrift(11, 600), NAVY)
    return fertig(b)


def s_schirm_auf(t, rest0=61.0):
    """Freifall → Griff ziehen → Hilfsschirm → Kappe streckt sich → öffnet mit Ruck → hängt ruhig."""
    rest = rest0 - t
    b = neu(HIMMEL)
    tempo = 300 if t < 1.5 else 300 - 240 * ease((t - 1.5) / 0.8)
    linien(b, t, 21, 20 if t < 1.6 else 10, tempo, 'hoch')
    kx, ky, rx, ry = 64, 66, 60, 50
    band(b, rest)
    if t < 0.7:                                                   # Freifall, greift zum Griff
        u = ease((t - 0.3) / 0.4)
        figur(b, 64, 96, math.pi + 0.1, 1.25, 1, arme=mix(ARME['tauchen'], ARME['ziehen'], u),
              beine='tauchen', schal=(-0.2, -1), phase=t * 18)
        return fertig(b)
    if t < 1.0:                                                   # Hilfsschirm schießt nach oben
        u = (t - 0.7) / 0.3
        p = figur(b, 64, 96, math.pi + 0.1, 1.25, 1, arme='ziehen', beine='tauchen', schal=(-0.2, -1), phase=t * 18)
        px, py = p(-10, -12)
        hy = py - 10 - 100 * ease(u)
        L(b, [(px, py), (px + 6, hy)], GRAU_D, 1)
        kappe(b, px + 6, hy, 14, 10, 1.0, 4)
        b.text(px - 78, hy - 2, 'PLOPP!', schrift(15, 700), NAVY)
        return fertig(b)
    # Kappe streckt sich, Figur dreht sich aufrecht, Ruck beim Öffnen
    u_dreh = ease((t - 1.0) / 0.6)
    a = math.pi + 0.1 + (math.pi - 0.1 + 0.12) * u_dreh
    ruck = math.sin((t - 1.45) / 0.65 * math.pi) * 14 if 1.45 < t < 2.1 else 0.0
    cy = 96 + 78 * ease((t - 1.0) / 0.8) + ruck
    sway = 0.07 * math.sin((t - 2.0) * 2.4) if t > 2.0 else 0.0
    a += sway
    auf = ease((t - 1.35) / 0.35)
    if t > 1.7:
        auf = 1.0 + 0.12 * math.sin(clamp((t - 1.7) / 0.4) * math.pi)
    kky = ky - 60 * (1 - ease((t - 1.0) / 0.5))
    kkx = kx - 30 * sway
    k = 1.05
    schultern = rot([(-1, -20), (2, -20)], 64, cy, a, k, 1)
    rand = kappe(b, kkx, kky, rx, ry, auf)
    leinen(b, rand, kkx, kky, schultern)
    arme = 'haengen' if t > 1.6 else mix(ARME['ziehen'], ARME['haengen'], ease((t - 1.0) / 0.6))
    figur(b, 64, cy, a, k, 1, arme=arme, beine='haengen' if t > 1.6 else 'tauchen',
          schal=(-1, 0.3), mund='schrei' if 1.4 < t < 2.2 else 'grimmig', phase=t * 10,
          brille_schief=3 if 1.45 < t < 2.3 else 0)
    if 1.45 < t < 2.2:
        mitte_text(b, 64, 222, 'RUMMS!', schrift(18, 700), ROT, WEISS, 1)
    return fertig(b)


def s_ende(t):
    """Letzte 3 s: Kappe abwerfen, kopfüber drehen, eintauchen bei 0:00, Platsch."""
    rest = 3.0 - t
    b = neu(HIMMEL)
    linien(b, t, 13, 14 if t > 0.9 else 6, 120 if t < 0.9 else 320, 'hoch')
    b.d.rectangle([0, 176 * SS, W * SS, H * SS], fill=RASEN)
    for x in range(-4, 210, 14):
        b.kreis(x, 176, 7, fill=RASEN_D, outline=SCHWARZ, b=1)
    if t < 3.0:
        becken(b, 64, 204, 58, 12, 14)
        ex = 100 + (22 * ease((t - 2.0) / 0.9) if t > 2.0 else 0)  # Ente paddelt panisch weg
        ente(b, ex, 203 + math.sin(t * 8), 0.9, -1)
        if t > 1.6:
            b.text(ex + 10, 186, '!!', schrift(14, 700), ROT)
        k = 1.05
        if t < 0.9:                                               # hängt, winkt, wirft die Kappe ab
            sway = 0.08 * math.sin(t * 3)
            cy = 80
            u = ease((t - 0.55) / 0.35)
            kx, ky = 92 + 160 * u * u, 8 - 120 * u * u
            rand = kappe(b, kx, ky, 44, 34, 1.0)
            if u == 0:
                leinen(b, rand, kx, ky, rot([(-1, -20), (2, -20)], 84, cy, sway, k, 1))
            else:
                for p in rand:
                    L(b, [p, (kx - 6, ky + 36)], GRAU_D, 1)
            figur(b, 84, cy, sway, k, 1, arme='winken' if t > 0.55 else 'haengen', beine='haengen',
                  schal=(-1, 0.3), mund='grinsen', phase=t * 10)
            if t > 0.55:
                b.text(6, 40, 'TSCHÜSS!', schrift(13, 700), NAVY)
        else:                                                     # dreht kopfüber und taucht
            u = ease((t - 0.9) / 0.6)
            f = clamp((t - 0.9) / 2.1)
            figur(b, 84 - 20 * u, 80 + 72 * f * f, math.pi * u, k, 1, arme=mix(ARME['haengen'], ARME['tauchen'], u),
                  beine='tauchen', schal=(0.2, -1), mund='grinsen', phase=t * 18)
        band(b, rest)
        return fertig(b)
    u = t - 3.0                                                   # Platsch
    becken(b, 64, 204, 58, 12, 14, vorn=False)
    hoehe = 70 * math.sin(math.pi * clamp(u / 1.1))
    for i in range(11):
        v = i / 10
        x = 22 + v * 84
        hgt = hoehe * (0.55 + 0.45 * math.sin(v * math.pi))
        form(b, [(x - 8, 206), (x - 2, 206 - hgt), (x + 2, 206 - hgt - 6), (x + 8, 206)], WASSER, WASSER_D)
    r = random.Random(5)
    for _ in range(14):
        vx, vy = r.uniform(-110, 110), r.uniform(-260, -150)
        x, y = 64 + vx * 0.7 * u, 198 + vy * u + 300 * u * u
        rr = r.uniform(2.5, 5)
        if y < 210:
            b.kreis(x, y, rr, fill=WASSER, outline=WASSER_D, b=1)
    wack = math.sin(u * 14) * 0.25                                # Stiefel ragen raus und zappeln
    for x, a in ((57, -0.15 + wack), (71, 0.2 - wack)):
        kapsel(b, (x, 208), (x + 14 * math.sin(a), 182), 8, ANZUG)
        form(b, [(x + 14 * math.sin(a) - 5, 184), (x + 14 * math.sin(a) + 5, 184),
                 (x + 14 * math.sin(a) + 6, 172), (x + 14 * math.sin(a) - 9, 174)], SCHWARZ)
    becken(b, 64, 204, 58, 12, 14, hinten=False)
    ente(b, 100 + 40 * u, 202 - 230 * u + 160 * u * u, 0.9, -1)
    if u > 0.05:
        mitte_text(b, 64, 60, 'PLATSCH!', schrift(18, 700), WASSER_D, WEISS, 1)
    band(b, 0.0)
    return fertig(b)


try:
    PIX = ImageFont.load_default_imagefont()
except AttributeError:
    PIX = ImageFont.load_default()


def gb_maske(text, z):
    m = Image.new('L', (len(text) * 8 + 4, 14), 0)
    ImageDraw.Draw(m).text((1, 1), text, font=PIX, fill=255)
    m = m.crop(m.getbbox())
    return m.resize((m.width * z, m.height * z), Image.NEAREST)


def gb_text(img, text, cx, cy, z, farbe, n=None):
    """n: nur die ersten n Zeichen zeigen (tippt sich ein), Lage wie der ganze Text."""
    m = gb_maske(text, z)
    x0, y0 = round(cx - m.width / 2), round(cy - m.height / 2)
    if n is not None and n < len(text):
        if n == 0:
            return
        breite = gb_maske(text[:n], z).width if text[:n].strip() else 0
        m = m.crop((0, 0, breite, m.height))
    img.paste(farbe, (x0, y0), m)


GB_ENTEN = [(28, 58), (70, 58), (36, 62), (62, 63), (24, 64), (76, 64), (48, 66), (56, 57), (31, 55), (68, 55)]


def s_level(t, ueber):
    """Game Boy: 100 x 114 Pixel, doppelt. Schrift tippt sich ein, Sterne nacheinander, Enten kommen dazu."""
    w, h = 100, 114
    img = Image.new('RGB', (w, h), GB0)
    d = ImageDraw.Draw(img)
    d.rectangle([2, 2, w - 3, h - 3], outline=GB3)
    d.rectangle([4, 4, w - 5, h - 5], outline=GB2)
    gb_text(img, 'COURSE CLEAR!', 50, 13, 1, GB3, n=int(t / 0.06))
    for i, sx in enumerate((30, 50, 70)):
        if t > 0.9 + i * 0.3:
            hop = 2 if t < 1.05 + i * 0.3 else 0
            d.polygon([(sx, 20 - hop), (sx + 2, 24 - hop), (sx + 6, 24 - hop), (sx + 3, 27 - hop), (sx + 4, 31 - hop),
                       (sx, 28 - hop), (sx - 4, 31 - hop), (sx - 3, 27 - hop), (sx - 6, 24 - hop), (sx - 2, 24 - hop)], fill=GB3)
    d.ellipse([18, 52, 82, 66], fill=GB1, outline=GB3)
    d.rectangle([18, 59, 82, 70], fill=GB1)
    d.ellipse([18, 63, 82, 77], fill=GB0, outline=GB3)
    for x in range(22, 80, 8):
        d.rectangle([x, 66, x + 3, 74], fill=GB2)
    d.line([18, 59, 18, 70], fill=GB3)
    d.line([82, 59, 82, 70], fill=GB3)
    d.ellipse([20, 53, 80, 65], fill=GB1, outline=GB2)
    wipp = int(t * 4) % 2
    d.rectangle([44, 50, 56, 60], fill=GB2)
    d.ellipse([42, 36, 58, 52], fill=GB0, outline=GB3)
    d.chord([42, 35, 58, 49], 180, 360, fill=GB3)
    d.rectangle([43, 39, 57, 41], fill=GB2)
    d.ellipse([45, 38, 49, 42], fill=GB0, outline=GB3)
    d.ellipse([51, 38, 55, 42], fill=GB0, outline=GB3)
    d.arc([45, 42, 55, 49], 20, 160, fill=GB3)
    d.line([57, 52, 64, 40 - wipp], fill=GB3, width=2)
    d.ellipse([60, 32 - wipp, 68, 38 - wipp], fill=GB1, outline=GB3)
    d.rectangle([68, 34 - wipp, 70, 35 - wipp], fill=GB3)
    for i, (ex, ey) in enumerate(GB_ENTEN[:min(len(GB_ENTEN), 3 + int(ueber))]):
        ey += (int(t * 3) + i) % 2
        d.ellipse([ex - 4, ey - 2, ex + 4, ey + 2], fill=GB0, outline=GB3)
        d.ellipse([ex + 1, ey - 6, ex + 5, ey - 2], fill=GB0, outline=GB3)
        d.point((ex + 3, ey - 4), fill=GB3)
        d.line([ex + 6, ey - 4, ex + 7, ey - 4], fill=GB3)
    s = int(ueber)
    if (ueber % 0.9) < 0.6:                                       # blinkt 600/300 ms wie in der App
        gb_text(img, f'+{s // 60}:{s % 60:02d}', 50, 88, 2, GB3)
    gb_text(img, 'TASTE: AUS', 50, 104, 1, GB2)
    return img.resize((W, H), Image.NEAREST)


def gb_uebergang(img, u):
    """Mosaik und Umfärben ins Game-Boy-Grün."""
    blk = max(1, int(1 + 9 * u))
    m = img.resize((max(1, W // blk), max(1, H // blk)), Image.BOX).resize((W, H), Image.NEAREST)
    if u > 0.5:
        g = m.convert('L')
        stufen = [GB3, GB2, GB1, GB0]
        m = Image.merge('RGB', [g.point(lambda v, i=i: stufen[min(3, v * 4 // 256)][i]) for i in range(3)])
    return m


def s_pause():
    b = neu(HIMMEL)
    wolke(b, 36, 196, 1.2)
    wolke(b, 110, 40, 0.7)
    figur(b, 70, 120, 0.25, 1.35, 1, arme='uhr', beine='haengen', schal=(-1, 0.6))
    vogel(b, 86, 52, 0.9, erschrocken=False, sx=-1)
    b.text(36, 30, 'Zzz', schrift(12, 600), NAVY)
    band(b, 462.0)
    b.rund(124, 142, 196, 160, 8, fill=GELB, outline=SCHWARZ, b=1)
    b.text(136, 156, 'PAUSE', schrift(13, 700), NAVY)
    pille(b, 116, 8, 'Weiter', schrift(15, 600))
    return fertig(b)


# ---------- Übergänge ----------

def wisch(alt, neu_, u):
    """Perspektivwechsel: neues Bild schiebt sich von unten herein (Fallrichtung)."""
    u = ease(u)
    out = Image.new('RGB', (W, H))
    dy = int(round(H * u))
    out.paste(alt, (0, -dy))
    out.paste(neu_, (0, H - dy))
    d = ImageDraw.Draw(out)
    for x in range(10, W, 24):
        d.line([(x, H - dy - 14), (x, H - dy + 14)], fill=WEISS, width=1)
    return out


def vorspulen(img, text, u):
    """Nur im Entwurf: Zeitsprung mit Streifen wie beim Vorspulen."""
    out = img.copy()
    d = ImageDraw.Draw(out)
    r = random.Random(int(u * 50))
    for _ in range(6):
        y = r.randrange(0, H)
        d.rectangle([0, y, W, y + r.randrange(1, 4)], fill=WEISS)
    d.rounded_rectangle([24, 84, 176, 146], 12, fill=NAVY)
    for x in (78, 100):
        d.polygon([(x, 94), (x + 20, 106), (x, 118)], fill=WEISS)
    d.fontmode = '1'
    d.text((100, 138), text, font=schrift(13, 600), fill=GELB, anchor='ms')
    return out


# ---------- Ablauf ----------

def folge(fn, dauer, *args):
    return [fn(i / FPS, *args) for i in range(int(round(dauer * FPS)))]


def ablauf():
    """Ganzer Ablauf im Entwurf: 15-Minuten-Timer, Zeitsprünge zwischen den Phasen."""
    absprung = [s_flugzeug(i / FPS, 1.4) for i in range(int(4.2 * FPS))]
    r0 = GESAMT - 2.0
    seite = [s_seite(i / FPS, r0 - i / FPS) for i in range(int(3 * FPS))]
    pov = [s_pov(i / FPS, r0 - 3 - i / FPS, 0.1) for i in range(int(3 * FPS))]
    frontal = [s_frontal(i / FPS, r0 - 6 - i / FPS) for i in range(int(3 * FPS))]
    anflug = [s_anflug(i / FPS, 464 - i / FPS, 1.0 + 0.2 * i / FPS) for i in range(int(3 * FPS))]
    seite2 = [s_seite(i / FPS, 461 - i / FPS) for i in range(int(2 * FPS))]
    schirm = [s_schirm_auf(i / FPS, 61.0) for i in range(int(3.6 * FPS))]
    ende = [s_ende(i / FPS) for i in range(int(4.3 * FPS))]
    gb = [gb_uebergang(ende[-1], i / (0.6 * FPS)) for i in range(int(0.6 * FPS))]
    gb += [s_level(i / FPS, 1.9 + i / FPS) for i in range(int(5.5 * FPS))]
    ende = ende + gb

    def ueber(a, b_):
        return [wisch(a[-1], b_[0], (i + 1) / 5) for i in range(4)]

    def sprung(a, text):
        return [vorspulen(a[-1], text, i / 6) for i in range(7)]

    alles = (absprung + ueber(absprung, seite) + seite + ueber(seite, pov) + pov + ueber(pov, frontal) + frontal
             + sprung(frontal, 'ZEITSPRUNG: NOCH 7:44') + anflug + ueber(anflug, seite2) + seite2
             + sprung(seite2, 'ZEITSPRUNG: NOCH 1:01') + schirm + sprung(schirm, 'ZEITSPRUNG: NOCH 0:03') + ende)
    return {'ablauf': alles, 'absprung': absprung, 'schirm': schirm, 'ende': ende}


def gif(frames, pfad, z=2):
    pal = Image.new('P', (1, 1))
    farben = [v for r in (0, 85, 170, 255) for g in (0, 85, 170, 255) for b in (0, 85, 170, 255) for v in (r, g, b)]
    pal.putpalette(farben + [0] * (768 - len(farben)))
    out = [f.resize((W * z, H * z), Image.NEAREST).quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    out[0].save(pfad, save_all=True, append_images=out[1:], duration=int(1000 / FPS), loop=0)


ANSICHTEN = [
    ('bereit', 'Bereit: Flügel', lambda: s_flugzeug(0.4)),
    ('absprung', 'Start: Absprung', lambda: s_flugzeug(2.15, 1.4)),
    ('seite', 'Seite + Höhenband', lambda: s_seite(0.9, 462.3)),
    ('pov', 'POV / Ruhe', lambda: s_pov(0.5, 462.3, 0.3)),
    ('frontal', 'Frontal', lambda: s_frontal(0.3, 462.3)),
    ('anflug', 'Anflug (T-10 min)', lambda: s_anflug(0.5, 462.3, 1.1)),
    ('schirm-auf', 'Schirm öffnet', lambda: s_schirm_auf(1.62)),
    ('schirm', 'Schirm offen (T-1 min)', lambda: s_schirm_auf(3.2)),
    ('eintauchen', 'Eintauchen (T-3 s)', lambda: s_ende(2.3)),
    ('platsch', 'Platsch (0:00)', lambda: s_ende(3.35)),
    ('level', 'Zeit um: Level', lambda: s_level(3.0, 42.45)),
    ('pause', 'Pause', s_pause),
]


def main(ziel, nur_bilder=False):
    tafel_bilder = []
    for name, titel, fn in ANSICHTEN:
        img = fn()
        img.save(os.path.join(ziel, f'timer-skin-fallschirm-{name}.png'))
        tafel_bilder.append((titel, img))
    z, sp, rand, kopf = 2, 4, 12, 26
    zeilen = (len(tafel_bilder) + sp - 1) // sp
    bw, bh = W * z, H * z
    tafel = Image.new('RGB', (sp * (bw + rand) + rand, zeilen * (bh + kopf + rand) + rand), (34, 34, 34))
    d = ImageDraw.Draw(tafel)
    ft = schrift(20, 600)
    for i, (titel, img) in enumerate(tafel_bilder):
        x = rand + (i % sp) * (bw + rand)
        y = rand + (i // sp) * (bh + kopf + rand)
        d.text((x, y + 19), titel, font=ft, fill=(230, 230, 230), anchor='ls')
        tafel.paste(img.resize((bw, bh), Image.NEAREST), (x, y + kopf))
    tafel.save(os.path.join(ziel, 'timer-skin-fallschirm-uebersicht.png'))
    if nur_bilder:
        return
    teile = ablauf()
    for name, frames in teile.items():
        gif(frames, os.path.join(ziel, f'timer-skin-fallschirm-{name}.gif'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.', '--bilder' in sys.argv)
