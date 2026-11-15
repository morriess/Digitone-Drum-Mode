#!/usr/bin/env bash
# Build a Digitone mk1 OS 1.44 firmware with DigiArp MIDI (by aleks-karachevtsev).
#
#   ./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx [output.syx]
#
# Rebuilds the combination DigiArp's author tested: elekloader core 4.1.1 + seq-events 0.1 + the
# author's own prebuilt DigiArp DA27 module. Nothing of DigiArp is copied into this repo; it is
# fetched from the author's repository at a fixed commit.
#
# Needs: git, python3, and an m68k cross toolchain (m68k-elf-gcc or m68k-linux-gnu-gcc).
set -euo pipefail

ELEKLOADER_REPO="https://github.com/irpina/elekloader.git"
ELEKLOADER_TAG="v0.9.0"                                          # core-dn1 4.1.1, seq-events-dn1 0.1
DIGIARP_REPO="https://github.com/aleks-karachevtsev/Digitone-MIDI-Arpeggiator.git"
DIGIARP_COMMIT="2a853b7a819607a14b7651f1f3b1a5c7cbec7cd9"        # DA27 beta, 2026-11-07
DIGIARP_ELEMOD="DigiArp-MIDI-DA27-0.9-dev11.elemod"
DIGIARP_SHA256="b82506a6adb3c39545eb6daa731e5e28d09e6c61a07dcedb312a65066b84ff58"
VERSION_LABEL="ARP1"

here="$(cd "$(dirname "$0")" && pwd)"
stock="${1:?usage: ./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx [output.syx]}"
stock="$(cd "$(dirname "$stock")" && pwd)/$(basename "$stock")"
out="${2:-$here/out/Digitone_OS1.44_DigiArpMIDI_DA27.syx}"
mkdir -p "$(dirname "$out")"
out="$(cd "$(dirname "$out")" && pwd)/$(basename "$out")"

if [ -z "${ELEKLOADER_CROSS:-}" ]; then
    if command -v m68k-elf-gcc >/dev/null; then export ELEKLOADER_CROSS=m68k-elf-
    elif command -v m68k-linux-gnu-gcc >/dev/null; then export ELEKLOADER_CROSS=m68k-linux-gnu-
    else echo "No m68k cross compiler found (m68k-elf-gcc or m68k-linux-gnu-gcc)." >&2; exit 1
    fi
fi

cache="$here/.cache"
mkdir -p "$cache"
[ -d "$cache/elekloader/.git" ] || git clone --quiet --depth 1 --branch "$ELEKLOADER_TAG" "$ELEKLOADER_REPO" "$cache/elekloader"
if [ ! -d "$cache/digiarp/.git" ]; then
    git clone --quiet "$DIGIARP_REPO" "$cache/digiarp"
fi
git -C "$cache/digiarp" checkout --quiet "$DIGIARP_COMMIT"

dm="$cache/digiarp/$DIGIARP_ELEMOD"
sha="$(sha256sum "$dm" | cut -d" " -f1)"
if [ "$sha" != "$DIGIARP_SHA256" ]; then
    echo "DigiArp module checksum does not match the author's published one ($sha != $DIGIARP_SHA256)." >&2
    exit 1
fi

export PYTHONPATH="$cache/elekloader"
build() { python3 -m elekloader.sdk.build "$1" --stock "$stock"; }
build "$cache/elekloader/mods/core-dn1"
build "$cache/elekloader/mods/seq-events-dn1"

core="$cache/elekloader/mods/core-dn1/out/core-4.1.1-os1.44.elemod"
seq="$cache/elekloader/mods/seq-events-dn1/out/seq-events-0.1-os1.44.elemod"

python3 -m elekloader.lint "$dm" --stock "$stock" --with "$core" --with "$seq"
python3 -m elekloader.patch --stock "$stock" --mod "$core" --mod "$seq" --mod "$dm" \
    --out "$out" --version "$VERSION_LABEL"

echo
echo "Built: $out  (shows as OS version $VERSION_LABEL on the unit)"
