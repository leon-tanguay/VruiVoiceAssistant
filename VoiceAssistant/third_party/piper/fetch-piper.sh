#!/bin/bash
# Fetch the standalone (native, NO Python) Piper TTS into this directory. The binary,
# its bundled shared libraries (onnxruntime, espeak-ng, piper_phonemize) and the
# espeak-ng phoneme data total ~33 MB and are gitignored. Piper is run as a persistent
# subprocess by Tts.cpp; VoiceSettings.piperCmd points at ./piper. The binary is built
# with an $ORIGIN rpath, so it loads its sibling .so's wherever this directory lives.
#
# v2 change from v1: the extracted `piper` binary is verified against a pinned
# SHA-256 (see DEPENDENCIES.md #5). This project's upstream, rhasspy/piper, is
# archived -- the release asset still resolves as of 2026-09-01, but if it ever
# disappears, this script needs a mirror URL added below, and the checksum lets you
# trust that mirror without re-auditing the binary.
set -e
DST="$(cd "$(dirname "$0")" && pwd)"
VER="2023.11.14-2"
URL="https://github.com/rhasspy/piper/releases/download/${VER}/piper_linux_x86_64.tar.gz"
# MIRROR_URL="<add a mirror here if the upstream release ever 404s>"

# sha256sum of the extracted `piper` executable, pinned from the copy already
# vendored in this machine's v1 checkout on 2026-09-01.
EXPECT_SHA256="12672a94ca6716e5a8f335cfa68bf43bd9a33284960e3f9d16b85090bf7aab6b"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
echo "Downloading standalone piper ${VER}..."
curl -L -f -o "$TMP/piper.tar.gz" "$URL"
# The tarball extracts to a top-level piper/ directory; drop its contents into DST so
# that DST/piper is the executable and DST/espeak-ng-data sits beside it.
tar xzf "$TMP/piper.tar.gz" -C "$TMP"

ACTUAL_SHA256="$(sha256sum "$TMP/piper/piper" | cut -d' ' -f1)"
if [ "$ACTUAL_SHA256" != "$EXPECT_SHA256" ]; then
  echo "piper binary checksum mismatch:" >&2
  echo "  expected $EXPECT_SHA256" >&2
  echo "  got      $ACTUAL_SHA256" >&2
  echo "Refusing to install a piper binary that doesn't match the pinned checksum." >&2
  exit 1
fi

cp -a "$TMP"/piper/. "$DST/"
echo "Installed standalone piper into $DST (checksum verified)"
