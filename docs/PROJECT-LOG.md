# Project log

Standing decisions and conventions. Short; newest last within each section.

## Decisions (2026-11-17, Brendan)

- Approach: headless Chromium lays the script out into image tiles; the C++ engine moves through them every
  field on the card's clock (smooth scrolling over everything).
- Control UI: a React 19 web app; a local server on the playout box drives the card.
- Linux first. On air: a dedicated box with a DeckLink card (model not chosen). Tests: on the CasparCG box
  (DeckLink 8K Pro, Ubuntu 24.04, Desktop Video 16.5, SDK 16.0) with CasparCG stopped.
- One regular, unmirrored output; the prompter monitor flips the picture.
- Scripts from .txt, .rtf, .md, .docx; headings are story breaks; no editing in the app; reloads keep the place.
- Jog wheel: speed follows rotation. ShuttlePRO v2 recommended (wired, spring-return ring); Ulanzi D100H also
  supported. Read through Linux evdev/hidraw.
- 1080i50 by default; 1080p25, 1080p50, 1080i59.94 selectable.
- Build order: stage 1 engine and hardware scroll test; then a minimum version; then preview, other formats,
  jog wheel.

## Status

- 2026-11-17: stage 1 built (engine core, `ought-scrolltest`, headless and DeckLink sinks). Waiting on the hardware
  test (`docs/SCROLL-TEST.md`) to choose the slow-motion method (`linear` or `wholeline`) and the buffer depth.

## Conventions

- The scroll test's log lines match `decklinkExperiments/loopback/tools/summarize_run.sh` ("playback started",
  report lines with `late=N dropped=N`, `FINAL ... outstanding=N`), so loopback runs are checked with that tool.
- `engine/tools/make_text_strip.sh` renders each paragraph separately: Ubuntu's ImageMagick policy blocks
  `caption:@file` and caps image size, and `caption` fails on some multi-paragraph text.
