#!/usr/bin/env bash
# FIAT-HELL Web Flasher - offline spustenie (Linux / macOS)
# Web Serial vyzaduje https alebo localhost, preto sa stranka servuje lokalne.
cd "$(dirname "$0")"
URL="http://localhost:8123"
( sleep 1; xdg-open "$URL" 2>/dev/null || open "$URL" 2>/dev/null ) &
exec python3 -m http.server 8123
