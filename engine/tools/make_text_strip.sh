#!/bin/sh
# Turn a text file into a 1920-wide 8-bit PGM strip for ought-scrolltest --strip:
# white text on black, 120 px side margins, wrapped at 1680 px, a 60 px gap between paragraphs.
#   make_text_strip.sh INPUT.txt OUTPUT.pgm [POINTSIZE]   (default 80)
# Needs ImageMagick ("convert") and the DejaVu fonts (both standard on Ubuntu).
#
# Each paragraph (text between blank lines) is rendered on its own and the raw pixels joined here:
# ImageMagick's caption fails on some multi-paragraph texts, and Ubuntu's policy caps image height,
# which a whole script would exceed. Text goes in as an argument (the policy blocks caption:@file),
# with ImageMagick's escape characters (% and \) doubled so they print literally.
set -eu
if [ $# -lt 2 ]; then
  echo "usage: $0 INPUT.txt OUTPUT.pgm [POINTSIZE]" >&2
  exit 2
fi
in=$1
out=$2
size=${3:-80}
width=1920
gap=60

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Split into paragraphs: para_0001, para_0002, ... (Windows line endings stripped first, so a blank
# line that is just "\r" still separates paragraphs.)
awk -v dir="$work" '
  { sub(/\r$/, "") }
  NF == 0 { if (open) { close(f); open = 0 } next }
  { if (!open) { n++; f = sprintf("%s/para_%04d", dir, n); open = 1 } print > f }
' "$in"

body="$work/body.gray"
: > "$body"
head -c $((width * gap)) /dev/zero > "$work/gap.gray"
first=1
for p in "$work"/para_*; do
  [ -e "$p" ] || continue
  [ $first -eq 1 ] || cat "$work/gap.gray" >> "$body"
  first=0
  text=$(sed -e 's/\\/\\\\/g' -e 's/%/%%/g' "$p")
  convert -size 1680x -background black -fill white -font DejaVu-Sans -pointsize "$size" \
    -interline-spacing 20 caption:"$text" -bordercolor black -border 120x0 \
    -colorspace Gray -depth 8 gray:- >> "$body"
done

bytes=$(wc -c < "$body")
if [ "$bytes" -eq 0 ] || [ $((bytes % width)) -ne 0 ]; then
  echo "$0: rendering failed (got $bytes bytes, not a whole number of $width-px rows)" >&2
  exit 1
fi
{ printf 'P5\n%d %d\n255\n' "$width" $((bytes / width)); cat "$body"; } > "$out"
echo "$out: ${width}x$((bytes / width))"
