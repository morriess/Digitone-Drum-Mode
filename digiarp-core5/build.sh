#!/usr/bin/env bash
# Build DigiArp MIDI for elekloader core 5.3, and one Digitone mk1 OS 1.44 firmware with it and Drum Mode.
#
#   ./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx
#
# Writes out/Digitone_OS1.44_DrumMode_DigiArp.syx (both mods, OS version DMA1) and
# out/Digitone_OS1.44_DigiArp_core5.syx (DigiArp alone on core 5.3, OS version DAC5).
#
# DigiArp MIDI is aleks-karachevtsev's (GPL-2.0-or-later): this fetches it at a fixed commit and applies
# digiarp-core5.patch, which only renames its two names for the MIDI senders (see README.md).
# Needs: git, python3, and an m68k cross toolchain. The stock OS file is yours; it is never copied into this repo.
set -euo pipefail

DIGIARP_REPO="https://github.com/aleks-karachevtsev/Digitone-MIDI-Arpeggiator.git"
DIGIARP_COMMIT="2a853b7"     # DA27, 0.9.0-beta (0.9-dev11): the build the elekloader Mod Shop lists

here="$(cd "$(dirname "$0")" && pwd)"
stock="${1:?usage: ./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx}"
stock="$(cd "$(dirname "$stock")" && pwd)/$(basename "$stock")"
mkdir -p "$here/out"

# Drum Mode first: its build fetches elekloader at the pinned commit (core 5.3, seq-events 0.1) and builds both.
"$here/../drum-mode/build.sh" "$stock"
ek="$here/../drum-mode/.cache/elekloader"
export PYTHONPATH="$ek"
if [ -z "${ELEKLOADER_CROSS:-}" ]; then
    if command -v m68k-elf-gcc >/dev/null; then export ELEKLOADER_CROSS=m68k-elf-
    else export ELEKLOADER_CROSS=m68k-linux-gnu-
    fi
fi

# DigiArp at its commit, with the patch.
da="$here/.cache/digiarp"
if [ ! -d "$da/.git" ]; then
    git clone --quiet "$DIGIARP_REPO" "$da"
fi
git -C "$da" fetch --quiet origin 2>/dev/null || true
git -C "$da" checkout --quiet --force "$DIGIARP_COMMIT"
git -C "$da" clean --quiet -fdx
git -C "$da" apply "$here/digiarp-core5.patch"
python3 -m elekloader.sdk.build "$da" --stock "$stock"

core="$ek/mods/core-dn1/out/core-5.3-os1.44.elemod"
seq="$ek/mods/seq-events-dn1/out/seq-events-0.1-os1.44.elemod"
dm_version="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$here/../drum-mode/src/mod.json")"
dm="$here/../drum-mode/src/out/drummode-$dm_version-os1.44.elemod"
[ -f "$dm" ] || dm="$here/../drum-mode/src/out/drummode-$dm_version.elemod"
arp="$da/out/digiarp-midi-0.9-dev11-core5.elemod"
cp "$arp" "$here/out/"

python3 -m elekloader.lint "$arp" --stock "$stock" --with "$core" --with "$seq" --with "$dm"
python3 -m elekloader.patch --stock "$stock" --mod "$core" --mod "$seq" --mod "$dm" --mod "$arp" \
    --out "$here/out/Digitone_OS1.44_DrumMode_DigiArp.syx" --version DMA1
python3 -m elekloader.patch --stock "$stock" --mod "$core" --mod "$arp" \
    --out "$here/out/Digitone_OS1.44_DigiArp_core5.syx" --version DAC5

echo
echo "Built: $here/out/Digitone_OS1.44_DrumMode_DigiArp.syx  (both mods; OS version DMA1)"
echo "       $here/out/Digitone_OS1.44_DigiArp_core5.syx     (DigiArp alone on core 5.3; OS version DAC5)"
