#!/bin/sh
# make_text_strip.sh: a script saved with Windows line endings (as Word exports often are) must give the
# same strip as the same text with Unix line endings. Skips (exit 77) without ImageMagick.
set -eu
script=$1
command -v convert >/dev/null 2>&1 || { echo "no ImageMagick: skipped"; exit 77; }
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
printf 'First paragraph of the script.\n\nSecond paragraph, after a blank line.\n' > "$dir/lf.txt"
printf 'First paragraph of the script.\r\n\r\nSecond paragraph, after a blank line.\r\n' > "$dir/crlf.txt"
"$script" "$dir/lf.txt" "$dir/lf.pgm" >/dev/null
"$script" "$dir/crlf.txt" "$dir/crlf.pgm" >/dev/null
if cmp -s "$dir/lf.pgm" "$dir/crlf.pgm"; then
  echo "CRLF and LF strips identical"
else
  echo "FAIL: CRLF strip differs: $(head -c 20 "$dir/lf.pgm" | sed -n 2p) vs $(head -c 20 "$dir/crlf.pgm" | sed -n 2p)"
  exit 1
fi
