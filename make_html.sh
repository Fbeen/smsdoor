#!/bin/bash

set -e  # stop bij fouten

SRC="/var/www/smsdoor/templates/index.html.gz"
DST_DIR="$HOME/pico/webserver/html"
DST="$DST_DIR/index.html.gz"
HEADER="$DST_DIR/index_html_gz.h"

echo "== SMSDOOR web build =="

# 1. Check of bronbestand bestaat
if [ ! -f "$SRC" ]; then
    echo "FOUT: $SRC bestaat niet!"
    exit 1
fi

# 2. Doelmap maken (indien nodig)
mkdir -p "$DST_DIR"

# 3. Kopiëren
echo "Kopiëren naar Pico project..."
cp "$SRC" "$DST"

# 4. Header genereren
echo "Genereren C header..."
cd "$DST_DIR"
xxd -i index.html.gz > index_html_gz.h

echo "Klaar!"
echo "Bestand: $HEADER"
