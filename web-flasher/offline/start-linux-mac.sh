#!/usr/bin/env bash
# FIAT-HELL Web Flasher - offline spustenie (Linux / macOS)
# Web Serial vyzaduje https alebo localhost, preto sa stranka servuje lokalne.
set -eu

cd "$(dirname "$0")" || { echo "Nepodarilo sa prejst do priecinka skriptu."; exit 1; }

if ! command -v python3 >/dev/null 2>&1; then
  echo "Python 3 nie je nainstalovany - nainstaluj ho a spusti skript znova."
  exit 1
fi

URL="http://localhost:8123"
( sleep 1; xdg-open "$URL" 2>/dev/null || open "$URL" 2>/dev/null ) &
exec python3 -m http.server 8123
