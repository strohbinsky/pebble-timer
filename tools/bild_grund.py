"""Grundlage der Entwürfe: Bild 200 x 228, Formen 4-fach überabgetastet, Farben auf die Pebble-Palette gerundet
(je Kanal 0/85/170/255), Text danach einfarbig ohne Kantenglättung. Schrift Fredoka (OFL) aus resources/fonts."""
import os

from PIL import Image, ImageDraw, ImageFont

W, H, SS = 200, 228, 4
_FONTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'resources', 'fonts')
_cache = {}


def c(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def schrift(groesse, wght=600, wdth=100):
    datei = 'Fredoka-Bold.ttf' if wght >= 700 else 'Fredoka-SemiBold.ttf'
    k = (groesse, datei)
    if k not in _cache:
        _cache[k] = ImageFont.truetype(os.path.join(_FONTS, datei), groesse)
    return _cache[k]


class Bild:
    def __init__(self, grund):
        self.gross = Image.new('RGB', (W * SS, H * SS), grund)
        self.d = ImageDraw.Draw(self.gross)
        self.texte = []

    def rund(self, x0, y0, x1, y1, r, fill=None, outline=None, b=0):
        self.d.rounded_rectangle([x0 * SS, y0 * SS, x1 * SS - 1, y1 * SS - 1], radius=r * SS,
                                 fill=fill, outline=outline, width=b * SS)

    def kreis(self, x, y, r, fill=None, outline=None, b=0):
        self.d.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS],
                       fill=fill, outline=outline, width=b * SS)

    def poly(self, pts, fill):
        self.d.polygon([(x * SS, y * SS) for x, y in pts], fill=fill)

    def text(self, x, y, s, f, fill):
        self.texte.append((x, y, s, f, fill))

    def fertig(self):
        klein = self.gross.resize((W, H), Image.BOX)
        klein = klein.point(lambda v: min((0, 85, 170, 255), key=lambda p: abs(p - v)))
        d = ImageDraw.Draw(klein)
        d.fontmode = '1'
        for x, y, s, f, fill in self.texte:
            d.text((round(x), round(y)), s, font=f, fill=fill, anchor='ls')
        return klein
