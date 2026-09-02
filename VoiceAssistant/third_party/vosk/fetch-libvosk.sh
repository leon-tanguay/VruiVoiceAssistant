#!/bin/bash
# Fetch the prebuilt libvosk.so into this directory (gitignored, ~25 MB). Downloads the
# official Vosk shared-library bundle from the alphacephei releases -- no Python/venv
# needed. The matching public header (vosk_api.h) is committed alongside this script.
#
# v2 change from v1: the extracted .so is verified against a pinned SHA-256 before
# being installed, rather than trusted on faith (see DEPENDENCIES.md #3). The pin
# below was computed from the copy already vendored in this machine's v1 checkout
# on 2026-09-01; if alphacephei ever re-publishes v0.3.45 with different bytes, this
# script will refuse to install it rather than silently linking something else.
set -e
DST="$(cd "$(dirname "$0")" && pwd)"
VER="0.3.45"
URL="https://github.com/alphacep/vosk-api/releases/download/v${VER}/vosk-linux-x86_64-${VER}.zip"

# sha256sum of the EXTRACTED libvosk.so, not of the .zip (unzip's own compression
# doesn't affect this, but pinning the file that actually gets linked is more direct).
EXPECT_SHA256="85c4654de3acdeb99abab86eeb2a6e603927d37089597c0fcc33d8638dc2ccaf"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
echo "Downloading libvosk ${VER}..."
curl -L -f -o "$TMP/vosk.zip" "$URL"
( cd "$TMP" && unzip -o vosk.zip >/dev/null )
SO="$(find "$TMP" -name libvosk.so | head -1)"

ACTUAL_SHA256="$(sha256sum "$SO" | cut -d' ' -f1)"
if [ "$ACTUAL_SHA256" != "$EXPECT_SHA256" ]; then
  echo "libvosk.so checksum mismatch:" >&2
  echo "  expected $EXPECT_SHA256" >&2
  echo "  got      $ACTUAL_SHA256" >&2
  echo "Refusing to install a libvosk.so that doesn't match the pinned checksum." >&2
  echo "If alphacephei genuinely re-published v${VER}, update EXPECT_SHA256 in this" >&2
  echo "script after verifying the new file by hand." >&2
  exit 1
fi

cp "$SO" "$DST/libvosk.so"
echo "Installed libvosk.so into $DST (checksum verified)"
