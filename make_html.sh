#!/bin/bash

set -e  # stop bij fouten

SRC="/var/www/smsdoor/templates/index.html"
DST_DIR="$HOME/pico/webserver/html"
DST_HTML="$DST_DIR/index.html"
MIN_HTML="$DST_DIR/index.min.html"
GZ_HTML="$DST_DIR/index.min.html.gz"
HEADER="$DST_DIR/index_html_gz.h"

echo "== SMSDOOR web build =="

# 1. Check bronbestand
if [ ! -f "$SRC" ]; then
    echo "FOUT: $SRC bestaat niet!"
    exit 1
fi

# 2. Doelmap maken
mkdir -p "$DST_DIR"

# 3. Originele HTML kopiëren (voor Git 👍)
echo "Kopiëren originele HTML..."
cp "$SRC" "$DST_HTML"

cd "$DST_DIR"

# 4. Minify (met 'minify' tool)
echo "Minify HTML..."
minify index.html > index.min.html

# 5. Gzip
echo "Gzip..."
gzip -kf -9 index.min.html

# 6. Header genereren
echo "Genereren C header..."
xxd -i index.min.html.gz > index_html_gz.h

# 7. Cleanup
echo "Opschonen..."
rm -f index.min.html index.min.html.gz

echo "Klaar!"
echo "Header: $HEADER"