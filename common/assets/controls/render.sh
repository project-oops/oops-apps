#!/usr/bin/env bash
# Rasterise the shared controller wireframe into the PNG a title packages.
#
# The SVG is the source; `controller.png` is a build product that happens to be committed,
# because a title's `package` rule copies it into the game's data and CI has no rasteriser. Run
# this after editing the SVG and commit both.
#
# **White on transparent**, so a title tints it to its own menu colour instead of fighting a
# baked background. Padded to 1024 wide rather than scaled to it: the drawing is 1000 units and
# stretching a line drawing to a rounder number is a visible 2% squash for no gain, while a
# texture whose width is a multiple of 64 is one the GPU's pitch rules like.
#
# ImageMagick in a container, not a local tool - the same rule the icons follow.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

docker run --rm -v "$HERE:/a" -w /a alpine:3 sh -c '
    apk add --no-cache imagemagick imagemagick-svg librsvg >/dev/null 2>&1
    magick -background none controller.svg -resize 1000x640 \
           -background none -gravity center -extent 1024x640 \
           PNG32:controller.png
    magick -background "#101010" controller.svg -resize 1000x640 preview.png
'

echo "controls: $(identify -format '%wx%h' "$HERE/controller.png" 2>/dev/null || echo rendered) controller.png"
