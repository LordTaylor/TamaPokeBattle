#!/bin/bash
# Builds the PvP test-opponent sketch (tools/pvp_bot) for an ESP32-S3 board.
# The sketch needs the PvP core files, so they are staged into a folder named
# like the sketch. Usage: bash tools/build_pvp_bot.sh <output-dir>
set -euo pipefail
OUT="$(mkdir -p "${1:?output dir required}" && cd "$1" && pwd)"  # resolve before cd into the repo
cd "$(dirname "$0")/.."
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
SRC="$OUT/src/pvp_bot"
rm -rf "$OUT/src" "$OUT/build"
mkdir -p "$SRC" "$OUT/build"
cp tools/pvp_bot/pvp_bot.ino "$SRC/"
cp pvp_*.h pvp_*.cpp battle.cpp battle.h battle_core.h trainer.cpp trainer.h dex.h "$SRC/"
# game-only and diagnostic units are not part of the bot
rm -f "$SRC"/pvp_diag*.cpp "$SRC"/pvp_diag*.h "$SRC"/pvp_host_pet.* "$SRC"/pvp_ui_*.cpp "$SRC"/pvp_ui.cpp "$SRC"/pvp_strings.* "$SRC"/pvp_app_instance.*
arduino-cli compile --fqbn "$FQBN" --build-path "$OUT/build" "$SRC"
