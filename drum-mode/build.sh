#!/usr/bin/env bash
# Build a Digitone mk1 OS 1.44 firmware with Drum Mode.
#
#   ./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx [output.syx]
#
# Needs: git, python3, and an m68k cross toolchain (m68k-elf-gcc or m68k-linux-gnu-gcc).
# The stock OS file is yours; it is never copied into this repo.
set -euo pipefail

ELEKLOADER_REPO="https://github.com/irpina/elekloader.git"
ELEKLOADER_COMMIT="9281c991e27c1516b43749dfb39a53c2c057859b"   # core-dn1 5.3, seq-events-dn1 0.1
VERSION_LABEL="DR81"

here="$(cd "$(dirname "$0")" && pwd)"
stock="${1:?usage: ./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx [output.syx]}"
stock="$(cd "$(dirname "$stock")" && pwd)/$(basename "$stock")"
out="${2:-$here/out/Digitone_OS1.44_DrumMode.syx}"
mkdir -p "$(dirname "$out")"
out="$(cd "$(dirname "$out")" && pwd)/$(basename "$out")"

# Cross toolchain prefix.
if [ -z "${ELEKLOADER_CROSS:-}" ]; then
    if command -v m68k-elf-gcc >/dev/null; then export ELEKLOADER_CROSS=m68k-elf-
    elif command -v m68k-linux-gnu-gcc >/dev/null; then export ELEKLOADER_CROSS=m68k-linux-gnu-
    else echo "No m68k cross compiler found (m68k-elf-gcc or m68k-linux-gnu-gcc)." >&2; exit 1
    fi
fi

# elekloader at a fixed commit, so every build uses the same core.
ek="$here/.cache/elekloader"
if [ ! -d "$ek/.git" ]; then
    git clone --quiet "$ELEKLOADER_REPO" "$ek"
fi
git -C "$ek" fetch --quiet origin "$ELEKLOADER_COMMIT" 2>/dev/null || true
git -C "$ek" checkout --quiet "$ELEKLOADER_COMMIT"
export PYTHONPATH="$ek"

build() { python3 -m elekloader.sdk.build "$1" --stock "$stock"; }

build "$ek/mods/core-dn1"
build "$ek/mods/seq-events-dn1"
rm -rf "$here/src/out"
build "$here/src"

core="$ek/mods/core-dn1/out/core-5.3-os1.44.elemod"
seq="$ek/mods/seq-events-dn1/out/seq-events-0.1-os1.44.elemod"
dm_version="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$here/src/mod.json")"
dm="$here/src/out/drummode-$dm_version-os1.44.elemod"
[ -f "$dm" ] || dm="$here/src/out/drummode-$dm_version.elemod"     # a mod for one OS version has no suffix

python3 -m elekloader.lint "$dm" --stock "$stock" --with "$core" --with "$seq"
python3 -m elekloader.patch --stock "$stock" --mod "$core" --mod "$seq" --mod "$dm" \
    --out "$out" --version "$VERSION_LABEL"

echo
echo "Built: $out  (shows as OS version $VERSION_LABEL on the unit)"
