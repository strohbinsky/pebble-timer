# Ziffernschrift für die Ansicht Papier: Roboto Bold (Apache 2.0, aus dem Pebble-SDK), nur Ziffern und wenige Zeichen,
# Doppelpunkt angehoben, damit er mittig zwischen den Ziffern steht (in Roboto sitzt er auf der Grundlinie).
# Aufruf: uv run --with fonttools python3 tools/ziffern_schrift.py \
#   ~/Library/Application\ Support/Pebble\ SDK/SDKs/4.33.1/toolchain/moddable/contributed/conversationalAI/fonts/Roboto-Bold.ttf \
#   resources/fonts/Ziffern-Roboto-Bold.ttf
import sys
from fontTools.ttLib import TTFont
from fontTools import subset

ZEICHEN = "0123456789:'+-"
HOCH = 0.087          # Anteil der Em-Größe: 8 px bei 92 px, im Emulator gemessen (Ziffern 50–116, Doppelpunkt 66–116)

f = TTFont(sys.argv[1])
glyf = f["glyf"]
name = f.getBestCmap()[ord(":")]
g = glyf[name]
d = round(HOCH * f["head"].unitsPerEm)
if g.isComposite():
    for c in g.components: c.y += d
else:
    g.coordinates.translate((0, d))
g.recalcBounds(glyf)
o = subset.Options()
o.hinting = False      # Hinting passt nach dem Verschieben nicht mehr
o.layout_features = []
o.name_IDs = ["*"]
s = subset.Subsetter(o)
s.populate(text=ZEICHEN)
s.subset(f)
f.save(sys.argv[2])
print("Doppelpunkt um", d, "Einheiten angehoben,", len(f.getGlyphOrder()), "Glyphen")
