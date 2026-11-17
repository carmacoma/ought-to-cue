# Ought to Cue

Autocue (teleprompter) for Brendan (GitHub carmacoma): scripts from text documents, scrolled by a jog wheel,
output 1920x1080 on a Blackmagic DeckLink card from a dedicated Linux box. Read `README.md` for how to build,
`docs/PROJECT-LOG.md` for standing decisions, and the design in `docs/superpowers/specs/`. Plans for each
build stage are in `docs/superpowers/plans/`. Keep `docs/PROJECT-LOG.md` up to date: decisions every session
needs go there.

## Shape of the system

- `engine/` (C++17, CMake): everything that happens per frame. Scroll motion, frame composition (field by
  field), the reading-line marker, and sinks (DeckLink, headless). It knows nothing about files or formats.
  Runs as its own process so the picture stays up whatever the server does.
- `server/` (Node, TypeScript; not built yet): script files and conversion (.txt, .rtf, .md, .docx), layout
  in headless Chromium into image tiles, state and settings, the WebSocket/HTTP API, jog wheel adapters.
- `apps/control` (React 19; not built yet): the operator's UI in any browser.

## Rules

- The DeckLink SDK is never committed (licence). Headers go in `sdk/include` (git-ignored); the build takes
  `-DOTC_DECKLINK_SDK=<path>`. Everything except the DeckLink sink must build and test without it (CI has none).
- The card's clock is the master: motion advances per output field (50 steps a second in 1080i50), counted
  in frames rendered, never from the system clock.
- Smooth scrolling is the requirement everything else serves. Speed changes ease in; no jerks.
- One unmirrored output; prompter monitors flip the picture themselves.
- JavaScript packages, once there are any (Brendan's rule from the graphics POC, against hijacked npm
  releases): `.npmrc` blocks install scripts, CI uses `npm ci`, and a new package or version is taken only once
  it has been published at least 7 days. Never a bare `npm update` or `npm install <package>`.
- Work test-first; CI (`.github/workflows/ci.yml`) builds the engine without the SDK and runs `ctest`.
- Check what other threads have done before starting: `git log`, open PRs, `docs/PROJECT-LOG.md`.
