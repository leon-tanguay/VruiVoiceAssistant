#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# Fetch everything the (pure-C++) Vrui Voice Assistant needs to run offline:
#   1. Vosk English STT model        (speech-to-text + wake word)   ~128 MB
#   2. A Piper voice                 (neural text-to-speech)         ~64 MB
#   3. The vendored native binaries  (libvosk.so, standalone piper) ~58 MB
#
# The large STT/TTS *models* (1 + 2) are downloaded into a per-user directory
#   ${XDG_DATA_HOME:-$HOME/.local/share}/vrui-assistant/models
# which the installed vislet reads at runtime -- so this needs NO root access.
# The vendored *binaries* (3) are fetched into the source tree because they are
# needed at build/install time (`make` links libvosk; `make install` copies
# both into the Vrui installation).
#
# Usage:
#   bash scripts/download_models.sh            fetch anything missing (default)
#   bash scripts/download_models.sh --check     report what's present/missing, fetch nothing
#   bash scripts/download_models.sh --prune     list files not used by the current
#                                                VoiceSettings.h defaults (dry run)
#   bash scripts/download_models.sh --prune --force   actually delete them
#
# v2 change from v1: the Piper voice files are checksummed after download (see
# DEPENDENCIES.md #6), and --check/--prune exist because a machine used for
# tuning can accumulate models the current settings no longer reference (on
# this box that was ~560 MB of an unused small Vosk model and four unused
# Piper voices before this script existed).
#
# Re-running (without --prune) is safe: existing downloads are skipped. No
# Python/venv involved.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Per-user model directory (must match defaultUserModelDir() in VoiceSettings.h):
MODELS="${XDG_DATA_HOME:-$HOME/.local/share}/vrui-assistant/models"

# What VoiceSettings.h's defaults actually reference. Keep this in sync by hand
# when you change VoiceSettings.h's voskModelPath / piperModel defaults --
# there is deliberately no clever auto-parsing of the header here.
VOSK_NAME="vosk-model-en-us-0.22-lgraph"
PIPER_VOICE="en_GB-semaine-medium"

MODE="fetch"
FORCE=0
for arg in "$@"; do
  case "$arg" in
    --check) MODE="check" ;;
    --prune) MODE="prune" ;;
    --force) FORCE=1 ;;
    *) echo "Unknown argument: $arg" >&2; exit 1 ;;
  esac
done

fetch() {
  if command -v curl >/dev/null 2>&1; then curl -fL -o "$2" "$1"
  elif command -v wget >/dev/null 2>&1; then wget -O "$2" "$1"
  else echo "Need curl or wget installed." >&2; exit 1
  fi
}

# ── --check: report and exit ────────────────────────────────────────────────
if [ "$MODE" = "check" ]; then
  echo "Models directory: $MODELS"
  if [ -d "$MODELS/$VOSK_NAME" ]; then
    echo "  OK    Vosk model:  $VOSK_NAME ($(du -sh "$MODELS/$VOSK_NAME" 2>/dev/null | cut -f1))"
  else
    echo "  MISS  Vosk model:  $VOSK_NAME"
  fi
  if [ -f "$MODELS/piper/$PIPER_VOICE.onnx" ]; then
    echo "  OK    Piper voice: $PIPER_VOICE ($(du -sh "$MODELS/piper/$PIPER_VOICE.onnx" 2>/dev/null | cut -f1))"
  else
    echo "  MISS  Piper voice: $PIPER_VOICE"
  fi
  [ -f "$ROOT/VoiceAssistant/third_party/vosk/libvosk.so" ] \
    && echo "  OK    libvosk.so"  || echo "  MISS  libvosk.so"
  [ -x "$ROOT/VoiceAssistant/third_party/piper/piper" ] \
    && echo "  OK    piper binary" || echo "  MISS  piper binary"
  exit 0
fi

# ── --prune: list (or delete) anything not matching the names above ────────
if [ "$MODE" = "prune" ]; then
  found_extra=0
  for d in "$MODELS"/*/ ; do
    [ -d "$d" ] || continue
    name="$(basename "$d")"
    [ "$name" = "piper" ] && continue
    if [ "$name" != "$VOSK_NAME" ]; then
      echo "unused Vosk model: $d ($(du -sh "$d" 2>/dev/null | cut -f1))"
      found_extra=1
      [ "$FORCE" = "1" ] && rm -rf "$d"
    fi
  done
  if [ -d "$MODELS/piper" ]; then
    for f in "$MODELS"/piper/*.onnx ; do
      [ -f "$f" ] || continue
      base="$(basename "$f" .onnx)"
      if [ "$base" != "$PIPER_VOICE" ]; then
        echo "unused Piper voice: $f ($(du -sh "$f" 2>/dev/null | cut -f1))"
        found_extra=1
        [ "$FORCE" = "1" ] && rm -f "$f" "$f.json"
      fi
    done
  fi
  if [ "$found_extra" = "0" ]; then
    echo "Nothing to prune."
  elif [ "$FORCE" != "1" ]; then
    echo "(dry run -- re-run with --prune --force to actually delete these)"
  else
    echo "Pruned."
  fi
  exit 0
fi

# ── fetch mode ───────────────────────────────────────────────────────────────
mkdir -p "$MODELS" "$MODELS/piper"
echo "Models directory: $MODELS"

# ── 1. Vosk STT model ────────────────────────────────────────────────────────
# The larger lgraph model understands near-homophones much better (e.g. it lets
# the LLM recover "recent the camera" -> "reset the camera").
VOSK_URL="https://alphacephei.com/vosk/models/${VOSK_NAME}.zip"
if [ -d "$MODELS/$VOSK_NAME" ]; then
  echo "✓ Vosk model already present: $MODELS/$VOSK_NAME"
else
  echo "↓ Downloading Vosk model ($VOSK_NAME)…"
  fetch "$VOSK_URL" "$MODELS/$VOSK_NAME.zip"
  echo "  Extracting…"
  ( cd "$MODELS" && unzip -q "$VOSK_NAME.zip" && rm -f "$VOSK_NAME.zip" )
  echo "✓ Vosk model ready: $MODELS/$VOSK_NAME"
fi

# ── 2. Piper voice ───────────────────────────────────────────────────────────
# en_GB-semaine-medium — a British English multi-speaker model; speaker 0 (prudence)
# is the default female voice. Swap the URLs below for any other voice from
# https://huggingface.co/rhasspy/piper-voices
PIPER_BASE="https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_GB/semaine/medium"
PIPER_ONNX="$MODELS/piper/${PIPER_VOICE}.onnx"
PIPER_JSON="$MODELS/piper/${PIPER_VOICE}.onnx.json"
# Pinned from the copy already downloaded on this machine, 2026-09-01. If you
# swap in a different voice, update (or remove) these two lines to match.
EXPECT_ONNX_SHA256="d6dab6f3b92db43ea3f78c7f20dc8eadb47a1f15d8a1c9d451cf3ccd201a2f66"
EXPECT_JSON_SHA256="6425dcb878684043b77d772b173ae006d86a583b110303edda48b8438ecee5ee"
if [ -f "$PIPER_ONNX" ] && [ -f "$PIPER_JSON" ]; then
  echo "✓ Piper voice already present: $PIPER_ONNX"
else
  echo "↓ Downloading Piper voice (${PIPER_VOICE})…"
  fetch "$PIPER_BASE/${PIPER_VOICE}.onnx"      "$PIPER_ONNX"
  fetch "$PIPER_BASE/${PIPER_VOICE}.onnx.json" "$PIPER_JSON"

  actual_onnx="$(sha256sum "$PIPER_ONNX" | cut -d' ' -f1)"
  actual_json="$(sha256sum "$PIPER_JSON" | cut -d' ' -f1)"
  if [ "$actual_onnx" != "$EXPECT_ONNX_SHA256" ] || [ "$actual_json" != "$EXPECT_JSON_SHA256" ]; then
    echo "Piper voice checksum mismatch -- removing the download rather than" >&2
    echo "leaving a possibly-corrupt or substituted model in place." >&2
    rm -f "$PIPER_ONNX" "$PIPER_JSON"
    exit 1
  fi
  echo "✓ Piper voice ready: $PIPER_ONNX (checksum verified)"
fi

# ── 3. Vendored native engine binaries (into the source tree, for build/install) ─
echo "↓ Fetching vendored native binaries (libvosk, standalone piper)…"
bash "$ROOT/VoiceAssistant/third_party/vosk/fetch-libvosk.sh"
bash "$ROOT/VoiceAssistant/third_party/piper/fetch-piper.sh"

echo
echo "All set. Build and install the assistant:"
echo "  make VRUI_MAKEDIR=<Vrui make dir> -j\$(nproc)"
echo "  sudo make VRUI_MAKEDIR=<Vrui make dir> install"
echo "then enable the 'VoiceAssistant' vislet in your Vrui configuration, or run e.g.:"
echo "  VruiDemo -vislet VoiceAssistant ';'"
