#!/bin/zsh
# Timer: baut die App außerhalb des Quellordners (~/.local/share/timer-build) und installiert sie.
#   tools/bauen.sh            nur bauen
#   tools/bauen.sh emu        bauen + Emulator emery + Logs
#   tools/bauen.sh uhr <IP>   bauen + über das Handy im WLAN (Entwicklerverbindung in der Pebble-App)
# Die fertige App liegt danach als timer.pbw neben dem Quellordner, wenn dieser in einem Ordner "dateien"
# liegt (Notizablage, per Sync aufs Handy), sonst in dist/timer.pbw.
set -e
export PATH="$HOME/.local/bin:$PATH"
QUELLE="${0:A:h:h}/"
ZIEL="$HOME/.local/share/timer-build"
mkdir -p "$ZIEL"
if [ "${QUELLE:h:t}" = "dateien" ]; then
  AUSGABE="${QUELLE}../timer.pbw"
else
  AUSGABE="${QUELLE}dist/timer.pbw"
fi
rsync -a --delete --exclude build --exclude tools --exclude dist --exclude docs --exclude .git "$QUELLE" "$ZIEL/"
cd "$ZIEL"
rm -rf build   # pebble clean reicht nicht: rsync --delete entfernt die waf-Statusdatei, dann bleibt build/ veraltet
pebble build 2>&1 | grep -v "PebblePacket\|Represents" | tail -3
# Quellkarte (Debug) raus: enthält Build-Pfade mit dem Benutzernamen des Rechners, die App braucht sie nicht
zip -q -d build/*.pbw pebble-js-app.js.map >/dev/null 2>&1 || true
# Sperre gegen persönliche Daten (Begriffsliste außerhalb des Repos), falls auf diesem Rechner eingerichtet
if [ -x "$HOME/.local/bin/github-sperre" ] && ! "$HOME/.local/bin/github-sperre" build/*.pbw >/dev/null; then
  echo "ABBRUCH: github-sperre schlägt bei der gebauten App an — nicht kopiert"
  "$HOME/.local/bin/github-sperre" build/*.pbw
  exit 1
fi
mkdir -p "${AUSGABE:h}"
cp build/*.pbw "$AUSGABE"
case "$1" in
  emu) pebble install --emulator emery --logs ;;
  uhr) if [ -n "$2" ]; then pebble install --phone "$2" --logs; else pebble install --cloudpebble --logs; fi ;;
esac
