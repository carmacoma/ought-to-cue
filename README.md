# Ought to Cue

An autocue (teleprompter) that puts a presenter's script on a 1920x1080 HD video signal, sent to SDI
and/or HDMI through a Blackmagic DeckLink card, scrolled by an operator with a jog wheel. The operator
works in a web app; a server on a Linux playout machine drives the card. Design:
[`docs/superpowers/specs/2026-11-17-autocue-design.md`](docs/superpowers/specs/2026-11-17-autocue-design.md).

**Status:** stage 1, the C++ engine and a scroll test for judging smoothness on a real card. The
hardware test procedure is [`docs/SCROLL-TEST.md`](docs/SCROLL-TEST.md).

## Build

Needs CMake 3.24+ and g++ (Ubuntu 24.04).

```bash
cmake -S engine -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

That builds everything except DeckLink output. With the SDK headers in `sdk/include` (see `sdk/README.md`):

```bash
cmake -S engine -B build-dl -DOTC_DECKLINK_SDK=$PWD/sdk/include && cmake --build build-dl
```

## Scroll test without a card

```bash
./build/ought-scrolltest --sink headless --pattern text --speed 120 --frames 50 --dump /tmp/otc --dump-every 10
```

writes every tenth frame as a PGM image. `engine/tools/make_text_strip.sh script.txt strip.pgm` turns a text file
into a strip for `--strip`.
