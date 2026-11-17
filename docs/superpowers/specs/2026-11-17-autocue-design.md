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
- Blackmagic DeckLink SDK (C++). On Linux it loads `libDeckLinkAPI.so` from
  Blackmagic Desktop Video at run time. The SDK itself comes from Blackmagic's
  website (registration needed); see "Left to the builder" on whether its
  headers can be committed.
- Linux playout machine with a DeckLink card and Desktop Video drivers.
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
                                  │  - compose frame, mirror, │
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
  unmirrored picture to SDI and HDMI where the card has both. Device and format chosen in settings.
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
the wheel has been turned.** A candidate is the Ulanzi D100H: an endless,
stepless dial with haptic feedback and three buttons, Bluetooth 5.0 (a
dual-mode version also has USB-C). Ulanzi's own app is Windows and macOS
only, so on Linux it is used in its standalone mode, where it sends ordinary
input events. Claude's interpretation of "speed follows rotation" for an
endless dial, to confirm on the device:

- Each step clockwise adds a fixed amount of forward speed, each step back
  takes it away; past zero the text runs backwards. The further from the
  zero point, the faster.
- One button stops the scroll and resets the zero point (pause); the other
  two are next and previous story. Which button does what is a setting.

The adapter reads the device's events directly from Linux's input system
(evdev), taking the device exclusively so its keystrokes never reach other
programs, and maps them to the actions above through a small config file
(which event is a clockwise step, which is each button). That makes it
work with any dial or shuttle that shows up as a keyboard, mouse or HID
device, not just the D100H. What events the D100H actually sends in
standalone mode isn't documented; the first job on real hardware is to
record them. Because the wheel sets speed rather than position, the path
wheel → server → engine needs no special latency handling.

### Control UI

- Script list from the scripts folder; load a script.
- Live preview of the output, built from the same tiles at the engine's
  reported position (approximate timing is fine; the output is the truth).
- Speed and position readout, story list to jump to.
- Settings: font, size, margins, colours, reading-line height, mirror per
  output, video device and format, scripts folder.
- Settings and the current script are held on the server, so every browser
  agrees and a reload of the UI loses nothing.

### Testing

- Converters: fixture files for each format, checked against expected
  internal form.
- Layout: render fixtures and check story offsets and tile count.
- Engine motion: a fake frame clock; check position per field for given
  speed sequences, including easing and slow speeds.
- Engine frames: headless sink renders against expected images (mirror,
  marker).
- Place-keeping: reload an edited script, check the anchor stays at the
  reading-line.
- All of the above runs in CI without a DeckLink card. The DeckLink sink is
  checked by hand on the playout machine.

## Calls made by Claude (to check)

- Repo split into `apps/control`, `server` (Node/TS), `engine` (C++), one repo.
- Default 1080i50; one unmirrored output.
- How "speed follows rotation" works on an endless dial, the button
  assignments, and reading the wheel through evdev with a mapping file.
- Build order: an early hardware test of the scroll on a real DeckLink card
  and monitor, with test patterns and a step-by-step for whoever runs it,
  before the motion code is built on. Then a minimum version (`.docx` and
  `.txt`, keyboard control, DeckLink output, place kept on reload), then the
  preview, the other formats and the jog wheel. (No first-use date or
  existing prompter was named, so this order is Claude's.)
- Playout hardware (DeckLink model, Linux distribution) not yet named; the
  build targets a current Ubuntu LTS and Desktop Video unless told otherwise.
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
- Whether the DeckLink SDK headers can be committed (check the SDK licence);
  if not, a documented path to point the build at a local SDK copy.
- What the engine does with its speed if the server disappears (the picture
  must stay up either way).
