#!/usr/bin/env bash
# Bump FW_VERSION, commit, tag v<version>. Push manually afterwards:
#   git push origin <branch> && git push origin v<version>
#
# Once the tag is on GitHub, .github/workflows/web-flasher.yml will:
#   - build firmware for both boards (esp32-8048s050 + esp32dev/WT32)
#   - deploy web-flasher to gh-pages (https://webiumsk.github.io/FIAT-HELL/)
#   - create a GitHub Release v<version> with the four .bin assets
#     (board-suffixed: *-s3.bin, *-wt32.bin + shared boot_app0.bin) — the version dropdown
#     on the gh-pages site reads these via the GitHub releases API.

set -euo pipefail

if [ $# -ne 1 ]; then
  echo "usage: $0 <version>   (e.g. $0 1.2.6)" >&2
  exit 1
fi

VER="$1"
if [[ ! "$VER" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "version must be N.N.N (got: $VER)" >&2
  exit 1
fi
TAG="v$VER"

ROOT="$(git rev-parse --show-toplevel)"
SRC="$ROOT/src/version.h"
[ -f "$SRC" ] || { echo "src/version.h not found at $SRC" >&2; exit 1; }

if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "working tree has uncommitted changes — commit or stash first" >&2
  exit 1
fi

if git rev-parse --verify --quiet "$TAG" >/dev/null; then
  echo "tag $TAG already exists" >&2
  exit 1
fi

OLD=$(grep -E '^#define FW_VERSION "' "$SRC" | sed -E 's/.*"(.*)".*/\1/')
if [ -z "$OLD" ]; then
  echo "couldn't read FW_VERSION from $SRC" >&2
  exit 1
fi

echo "FW_VERSION: $OLD -> $VER"
sed -i -E "s|^#define FW_VERSION \".*\"|#define FW_VERSION \"$VER\"|" "$SRC"

NEW=$(grep -E '^#define FW_VERSION "' "$SRC" | sed -E 's/.*"(.*)".*/\1/')
if [ "$NEW" != "$VER" ]; then
  echo "sed bump failed (file still says $NEW)" >&2
  exit 1
fi

git -C "$ROOT" add src/version.h
git -C "$ROOT" commit -m "chore: bump to $TAG"
git -C "$ROOT" tag -a "$TAG" -m "Release $TAG"

BRANCH=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
cat <<EOF

Created commit + tag $TAG on $BRANCH.

To publish:
  git push origin $BRANCH
  git push origin $TAG

GH Actions will then build, deploy gh-pages, and create the Release with assets.
EOF
