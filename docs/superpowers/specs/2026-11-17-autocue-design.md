# Ought to Cue: autocue design

Date: 2026-11-17. Status: for Brendan's approval.

## Intent

An autocue (teleprompter) that puts a presenter's script on a 1920x1080 HD
video signal, sent to SDI and/or HDMI through a Blackmagic DeckLink card.
An operator scrolls it with an external jog wheel. The operator's interface
is a web app, so it runs anywhere a browser does; a server on the playout
machine does the video output.

Why now, and what the existing setup lacks, hasn't been said yet. Context:
the owner also builds the Sky racing graphics POC (`carmacoma/2026-graphics-system-poc`),
so the users are likely a racing broadcast team, but nothing here depends on it.

## Goals

- Smooth scrolling, locked to the output frame rate. This is the quality the
  whole design is built around.
- Scripts loaded from `.txt`, `.rtf`, `.md` and `.docx` files.
- Controlled by an external jog wheel; the keyboard stands in until its
  protocol is known.
- A React 19 control UI in any browser on the network.
- Runs on Linux first.

## Non-goals (for now)

- Windows or macOS playout.
- Editing scripts inside the app (scripts are edited in their own apps and
  reloaded).
- Newsroom system integration (MOS, iNEWS, ENPS) and running orders.
- The jog wheel's own protocol: the adapter is written once the protocol is
  available. Until then only the actions it maps to are defined.
- Several scripts on several outputs at once.
- A mirrored output (the prompter monitor flips the picture).
- Any link to the graphics POC.

## Anti-goals (a failure even if it works)

- Visible judder or uneven motion in the scroll, at any speed, including very slow.
- The presenter losing their place when a script is reloaded or a setting changes.
- Anything that needs the internet while on air.
- An output that stops or glitches because the control UI disconnected or a
  browser tab closed.

## Constraints

- Output 1920x1080. Default format 1080i50; 1080p25, 1080p50 and 1080i59.94
  selectable. (Defaults are Claude's call: 50Hz region.)
- Blackmagic DeckLink SDK (C++), API 16.0. On Linux `DeckLinkAPIDispatch.cpp`
  loads `libDeckLinkAPI.so` from Desktop Video at run time (link `-ldl
  -lpthread`; sample flags `-Wno-multichar -fno-rtti`). The SDK is **not
  committed**: Brendan keeps it locally and supplies headers, manual text and
  samples on request (licensing and handling in `carmacoma/decklinkExperiments`,
  `sdk/README.md` and `docs/LICENSING.md`). The build takes the headers' path
  (`SDK=/path/to/include`), as the spike there does.
- The autocue runs on **its own dedicated Linux box with a DeckLink card of
  some kind, not the CasparCG box** (Brendan, 2026-11-17). The exact card isn't
  chosen, so the code uses only the SDK's standard interfaces, picks the
  device and connector from settings, and checks the card supports the chosen
  mode (Brendan: "we should have some flexibility"). HDMI is available only if
  the chosen card has it.
- Testing happens on the CasparCG box until the dedicated box exists
  (Brendan, 2026-11-17): a DeckLink 8K Pro on Ubuntu 24.04, Desktop Video
  16.5a1, where 1080i50 is upper field first; SDI only. A test takes one of
  its outputs with CasparCG stopped, as the loopback tests in
  `decklinkExperiments` do, so tests are run by Brendan at times that suit
  the graphics work. That repo's loopback capture (an SDI cable from an
  output back into an input) can also record the autocue's output for
  checking frame by frame.
- Prior work to build on: `decklinkExperiments/loopback/cpp/dl_output_spike.cpp`
  (Brendan's own code, no SDK sample code copied) already does scheduled
  playback on this card and ran six hours with no late or dropped frames.
  Lessons recorded there that this design follows: the card's clock is the
  master (it ran about 11 ppm slow against the host), so schedule from frame
  completions, never from the system clock; keep a buffer of about 10 frames;
  follow its stop sequence.
- Development and CI happen on machines with no DeckLink card, so everything
  except the card driver must run and be testable without one.

## Approaches considered

1. **The browser draws every frame.** A hidden Chromium renders the React
   prompter page continuously; a native module copies frames to the card.
   One renderer, but the browser's timing isn't the card's, so frames are
   repeated or dropped: judder.
2. **The browser lays out, the server moves (chosen).** Headless Chromium
   renders the formatted script once into a tall strip of image tiles; the
   native engine moves through the strip every field/frame on the card's
   clock. Smooth motion, web layout; more native code.
3. **CasparCG as the server.** HTML producer plus DeckLink consumer. Least
   code, proven, but the same browser-timing judder as 1, a big dependency
   and no macOS.

Brendan picked 2 (2026-11-17) because smooth scrolling is what a prompter
can't get wrong.

## Design

### Parts

```
 browser (any machine)            playout machine (Linux)
 ┌──────────────────┐  WebSocket  ┌───────────────────────────┐
 │ control UI       │◄──────────► │ server (Node, TypeScript) │
 │ React 19         │  + HTTP     │  - script files, parsing  │
 └──────────────────┘             │  - layout (headless       │
                                  │    Chromium) → tiles      │
        jog wheel ── adapter ───► │  - state, settings        │
                                  └────────────┬──────────────┘
                                     local IPC │ tiles + commands
                                  ┌────────────▼──────────────┐
                                  │ engine (C++)              │
                                  │  - scroll motion per field│
                                  │  - compose frame,         │
                                  │    reading-line marker    │
                                  │  - sinks: DeckLink /      │
                                  │    window / headless      │
                                  └───────────────────────────┘
```

Three parts in one repo, Claude's call on the split:

- `apps/control`: React 19 control UI (Vite).
- `server`: Node and TypeScript. Owns script files, conversion, layout,
  state and settings, the WebSocket and HTTP API, and the control-input
  adapters. It is the only thing the UI talks to.
- `engine`: C++ (CMake). Owns everything that happens per frame. It knows
  nothing about files or formats: it gets tiles, story positions and
  commands, and puts out frames.

The engine runs as its own process so a crash or restart of the server, or a
long layout job, never stops the video. If the server goes away the engine
keeps outputting its last picture (and keeps scrolling at the current speed;
whether it should instead stop is left to the builder, with "keep the
picture up" as the hard rule).

### Scripts

- The server watches a scripts folder (path in settings). The UI lists its
  files; the operator picks one to load.
- Every format is converted to one simple internal form: paragraphs of text
  runs, each run with bold, italic, underline and colour, plus story breaks.
  Fonts and sizes in the source are ignored: the prompter has one font and
  size.
- Story breaks: Markdown headings and Word heading styles. A `.txt` or
  `.rtf` with no headings is one story (how `.rtf` headings are detected is
  the builder's call).
- `.txt` is plain. `.md` keeps bold and italic. `.rtf` and `.docx` keep bold,
  italic, underline and text colour.
- When the loaded file changes on disk, the server reloads it and the
  presenter's place is kept (see "Keeping the place").

### Layout into tiles

- The server renders the internal form as an HTML page in headless Chromium
  (Playwright), at the output's text width, in the chosen font, size,
  margins and colours (white on black by default).
- It captures the page as a vertical strip of tiles (e.g. 1920 wide,
  a few thousand pixels tall each), and records where each story and each
  paragraph starts (y offset) from the DOM.
- It lays out again only when the script or a look setting changes (font,
  size, margins, colours). Layout runs while the old tiles stay on air, and
  the engine swaps to the new strip in one frame.

### Keeping the place

Position is held as a text anchor (paragraph and offset within it) as well
as a pixel offset. After a re-layout the engine is given the new pixel
offset of the same anchor, so the line at the reading-line stays at the
reading-line. If the anchor's paragraph was deleted, the nearest surviving
paragraph before it is used.

### Motion

- Scroll speed is a signed value (lines per second, negative is backwards)
  set by the control actions. The engine integrates it per output field
  (50 motion steps a second in 1080i50, not 25), timed by the card's frame
  clock, never the system clock.
- Speed changes ease in over a short time so the text never jerks.
- Even motion at very slow speeds is a hard requirement; how (sub-pixel
  vertical resampling, field-based stepping) is the builder's call, checked
  by eye on a real monitor.

### The output frame

Composed by the engine for each field/frame from the tiles at the current
position:

- Text strip, black background.
- Reading-line marker (an arrow or band at a set height, a setting).
- Never mirrored. There is one regular signal; professional prompter
  monitors flip the picture themselves (Brendan, 2026-11-17).

### Outputs (sinks)

- **DeckLink:** scheduled playback on one DeckLink output, the same
  unmirrored picture to SDI and HDMI where the card has both. Built from the
  scheduled-output spike in `decklinkExperiments`; frame-completion results
  (late, dropped) are counted and shown in the control UI. In 1080i50 the
  two fields of a frame are drawn from scroll positions 20 ms apart, upper
  field first. Device and
  format chosen in settings.
- **Window:** a desktop window, for developing on a machine without a card.
- **Headless:** no display; frames can be written to image files for tests
  and checked against expected images.

### Control

Every control source maps to the same actions:

- set speed / change speed by a step
- nudge forward or back a short distance
- next story / previous story / back to top
- pause / resume

Sources:

- The control UI's buttons and the keyboard (in the UI's browser).
- The jog wheel, through an adapter in the server.

The jog wheel (Brendan, 2026-11-17): **the speed of the text follows how far
the wheel has been turned.** Two candidates so far, which work differently, so
the adapter supports both kinds of control:

- **Contour ShuttlePRO v2** (USB, wired): a spring-loaded outer shuttle ring
  (about 7 positions each way, springs back to centre when let go), an endless
  inner jog wheel with detents, and 15 buttons. Long supported on Linux.
- **Ulanzi D100H** (Bluetooth 5.0; a dual-mode version also has USB-C): one
  endless, stepless dial with haptic feedback and three buttons. Ulanzi's app
  is Windows and macOS only, so on Linux it runs in standalone mode, sending
  ordinary input events; which ones is undocumented.

How each maps to the actions (Claude's call, to confirm on the devices):

- **Spring-return ring (ShuttlePRO):** the ring's position sets the speed
  directly, through a configurable curve (fine control near centre, faster at
  the ends); centre, including letting go, is stop. Backwards past centre runs
  the text backwards. The inner jog wheel nudges; buttons do next and previous
  story, top, pause and speed presets.
- **Endless dial (D100H):** each step clockwise adds a fixed amount of speed,
  each step back takes it away; past zero the text runs backwards. One button
  pauses and resets zero; the other two are next and previous story.
- Which control and button does what is a setting.

Reading the devices: the adapter reads Linux's input system (evdev) and takes
the device exclusively, so its keystrokes never reach other programs, and
falls back to raw HID reports (hidraw) where evdev loses information. Known
case: the ShuttlePRO reports ring and jog positions as absolute values inside
"relative" events, the jog counter wraps at 0-255, and the kernel can drop
the return-to-centre event because it looks like "no movement". A missed
return-to-centre would leave the text scrolling after the operator let go,
so stopping on release must be reliable: if evdev can't guarantee it, read
hidraw. A small mapping file per device model says which event is which.
Recording each device's real events with `evtest` on the playout box is the
first hardware job for the wheel.

Because the wheel sets speed rather than position, the path
wheel → server → engine needs no special latency handling.

Claude's recommendation for the on-air device: the ShuttlePRO v2. It is wired
(no Bluetooth dropouts on air), works on Linux without the vendor's app, and
its spring-return ring means letting go stops the text.

### Control UI

- Script list from the scripts folder; load a script.
- Live preview of the output, built from the same tiles at the engine's
  reported position (approximate timing is fine; the output is the truth).
- Speed and position readout, story list to jump to.
- Settings: font, size, margins, colours, reading-line height,
  jog wheel mapping, video device and format, scripts folder.
- Settings and the current script are held on the server, so every browser
  agrees and a reload of the UI loses nothing.

### Testing

- Converters: fixture files for each format, checked against expected
  internal form.
- Layout: render fixtures and check story offsets and tile count.
- Engine motion: a fake frame clock; check position per field for given
  speed sequences, including easing and slow speeds.
- Engine frames: headless sink renders against expected images (text,
  marker).
- Place-keeping: reload an edited script, check the anchor stays at the
  reading-line.
- All of the above runs in CI without a DeckLink card or the SDK headers:
  the engine builds without the DeckLink sink when no SDK path is given. The
  DeckLink sink is built and checked by hand on the playout machine.

## Calls made by Claude (to check)

- Repo split into `apps/control`, `server` (Node/TS), `engine` (C++), one repo.
- Default 1080i50; one unmirrored output.
- How "speed follows rotation" works on each kind of wheel (spring-return
  ring: position is speed; endless dial: steps add speed), the button
  assignments, and reading wheels through evdev/hidraw with a mapping file
  per model. Recommending the ShuttlePRO v2 for air.
- Build order: an early hardware test of the scroll on a real DeckLink card
  and monitor, with test patterns and a step-by-step for whoever runs it,
  before the motion code is built on. Then a minimum version (`.docx` and
  `.txt`, keyboard control, DeckLink output, place kept on reload), then the
  preview, the other formats and the jog wheel. (No first-use date or
  existing prompter was named, so this order is Claude's.)
- Target Ubuntu 24.04, Desktop Video 16.5, SDK 16.0, as on the 8K Pro box;
  the SDK stays out of the repo, as in `decklinkExperiments`.
- White on black; one font and size; headings are story breaks.
- Keep the place across reloads; no in-app editing.
- Server holds settings and state; the engine is a separate process.
- Packages: carry over the graphics POC's safety rule (`.npmrc` blocks install
  scripts, `npm ci`, new packages only once published at least 7 days).

## Left to the builder

- Converter libraries for each format, as long as bold, italic, underline,
  colour and headings survive.
- Tile size, IPC mechanism between server and engine (e.g. Unix socket for
  commands, shared memory for tiles), and the window toolkit.
- The method for smooth slow motion.
- What the engine does with its speed if the server disappears (the picture
  must stay up either way).
