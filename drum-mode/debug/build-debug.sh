#!/usr/bin/env bash
# Build a Drum Mode debug firmware: Drum Mode (src/) with one of the debug patches in this folder.
#
#   ./debug/build-debug.sh 1|2 /path/to/Digitone_and_Digitone_Keys_OS1.44.syx [output.syx]
#
# Debug 1 shows as DRMD on the boot screen, debug 2 as DRD2. What each shows: ../docs/debugging.md.
# Like the normal build, the result is for your own unit only: never share it.
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
n="${1:?usage: ./debug/build-debug.sh 1|2 /path/to/OS1.44.syx [output.syx]}"
stock="${2:?usage: ./debug/build-debug.sh 1|2 /path/to/OS1.44.syx [output.syx]}"
case "$n" in
    1) label=DRMD ;;
    2) label=DRD2 ;;
    *) echo "Debug build 1 or 2." >&2; exit 1 ;;
esac
out="${3:-$here/out/Digitone_OS1.44_DrumMode_DEBUG$n.syx}"

# The normal build first: it fetches elekloader and builds core and seq-events.
"$here/build.sh" "$stock" "$here/out/.normal.syx" >/dev/null

stock="$(cd "$(dirname "$stock")" && pwd)/$(basename "$stock")"
mkdir -p "$(dirname "$out")"
out="$(cd "$(dirname "$out")" && pwd)/$(basename "$out")"
ek="$here/.cache/elekloader"
export PYTHONPATH="$ek"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp -r "$here/src" "$work/src"
rm -rf "$work/src/out"
(cd "$work/src" && patch -s -p1 < "$here/debug/debug$n.patch")
python3 -m elekloader.sdk.build "$work/src" --stock "$stock"

core="$ek/mods/core-dn1/out/core-5.3-os1.44.elemod"
seq="$ek/mods/seq-events-dn1/out/seq-events-0.1-os1.44.elemod"
dm="$(ls "$work/src/out/"*.elemod)"
python3 -m elekloader.lint "$dm" --stock "$stock" --with "$core" --with "$seq"
python3 -m elekloader.patch --stock "$stock" --mod "$core" --mod "$seq" --mod "$dm" \
    --out "$out" --version "$label"
rm -f "$here/out/.normal.syx" "$here/out/.normal.syx".*.json

echo
echo "Built: $out  (shows as OS version $label on the unit)"
