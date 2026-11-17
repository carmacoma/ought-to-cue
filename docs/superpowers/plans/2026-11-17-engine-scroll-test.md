# Engine and Scroll Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the C++ engine's core (scroll motion, frame composition, sinks) and a `ought-scrolltest` program that scrolls test patterns or a real text strip out of a DeckLink card, so Brendan can judge smoothness on the CasparCG box before anything else is built on it.

**Architecture:** A dependency-free core library (`engine/src/core`) holds the motion model, the strip (pre-converted UYVY rows), the compositor and `ScrollSource`, which renders frame after frame advancing motion once per field. Sinks drive it: a headless sink (CI, image dumps) and a DeckLink sink ported from `decklinkExperiments/loopback/cpp/dl_output_spike.cpp`, where the card's frame completions are the clock. The DeckLink parts build only when the SDK headers path is given.

**Tech Stack:** C++17, CMake ≥ 3.24, g++ 13 (Ubuntu 24.04), Blackmagic DeckLink SDK 16.0 headers (not committed), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-17-autocue-design.md` (this plan is stage 1 of its build order: "an early hardware test of the scroll on a real DeckLink card and monitor, with test patterns and a step-by-step for whoever runs it"). Plans for the minimum version and later stages follow once this has been tested on the card. Not in this stage: the window sink, the server, the control UI, script conversion, tiles from Chromium, the jog wheel.

## Global Constraints

- Output 1920x1080, 8-bit YUV 4:2:2 (`'2vuy'`, byte order U Y V Y), as in the spike.
- Modes: `1080i50` (default), `1080p25`, `1080p50`, `1080i5994`. Interlaced modes are upper field first.
- Motion is integrated per output field (50 steps a second in 1080i50), timed by frames rendered (the card's clock), never the system clock.
- Speed changes ease in; no jerks. Even motion at very slow speeds is the requirement; this stage offers two methods (`linear`, `wholeline`) so the hardware test can choose.
- Never mirrored.
- White text on black. Reading-line marker at a set height (default y = 360).
- The DeckLink SDK is never committed. The build takes `-DOTC_DECKLINK_SDK=/path/to/include` (the folder holding `DeckLinkAPI.h` and `DeckLinkAPIDispatch.cpp`); `sdk/` is git-ignored. Linux flags from the samples: `-Wno-multichar -fno-rtti`, link `-ldl -lpthread`.
- Everything except the DeckLink sink builds and tests in CI without the SDK or a card.
- No third-party C++ libraries in this stage (tests use a small in-repo `check.h`).
- Commits end with the session's attribution lines; work on branch `claude/brave-heisenberg-6wl6wj`.

## Review Focus

1. **Control lag.** The card buffer delays every speed change by its depth (10 frames = 400 ms, which an operator feels). A command must take effect on the very next frame rendered, and the buffer depth is a flag (default 4) so the test can find the smallest safe value. Test in Task 5.
2. **Running off either end of the script.** Scrolling past the top or the end stops cleanly at the limit; rows outside the strip are black, never garbage or a crash. Tests in Tasks 2 and 4.
3. **Speed exactly zero / very slow.** At speed 0 the picture is identical frame to frame (no drift, no shimmer from interpolation); at 2 px/s position still advances evenly. Tests in Tasks 2 and 5.
4. **A strip shorter than the screen, or empty.** Renders black plus the marker, no crash. Test in Task 4.
5. **A bad strip file.** Wrong format, wrong width or a truncated PGM gives a clear error and a non-zero exit, not a crash or a garbled picture. Test in Task 3.

---

## File Structure

```
.gitignore                     sdk/, build/
README.md                      what this is, how to build and run the scroll test
CLAUDE.md                      project instructions for agents (mirrors the graphics POC's style)
docs/PROJECT-LOG.md            standing decisions (short)
docs/SCROLL-TEST.md            Brendan's step-by-step for the hardware test
.github/workflows/ci.yml       build engine without SDK, run ctest
engine/CMakeLists.txt
engine/src/core/video_mode.{h,cpp}     modes and their timing
engine/src/core/motion.{h,cpp}         speed easing, position, limits
engine/src/core/strip.{h,cpp}          UYVY strip, grey->UYVY, test patterns
engine/src/core/pgm.{h,cpp}            read/write 8-bit PGM (P5)
engine/src/core/compositor.{h,cpp}     field/frame composition, marker
engine/src/core/scroll_source.{h,cpp}  renders successive frames, thread-safe commands
engine/src/core/counter_pattern.h      copy of decklinkExperiments loopback/cpp/pattern.h (Brendan's code)
engine/src/sinks/headless_sink.{h,cpp}
engine/src/sinks/decklink/decklink_sink.{h,cpp}   (built only with the SDK)
engine/src/apps/scrolltest_main.cpp
engine/tools/make_text_strip.sh        ImageMagick: text file -> 1920-wide PGM strip
engine/tests/check.h
engine/tests/test_*.cpp
```

---

### Task 1: Repo skeleton, build, video modes, CI

**Files:**
- Create: `.gitignore`, `README.md`, `CLAUDE.md`, `docs/PROJECT-LOG.md`, `.github/workflows/ci.yml`, `engine/CMakeLists.txt`, `engine/tests/check.h`, `engine/src/core/video_mode.h`, `engine/src/core/video_mode.cpp`
- Test: `engine/tests/test_video_mode.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace otc {
  struct VideoMode {
    std::string name;        // "1080i50"
    int width = 1920, height = 1080;
    bool interlaced;
    bool upperFieldFirst;    // true for interlaced HD
    int64_t frameDuration;   // in timescale units
    int64_t timescale;       // 1080i50: 1000/25000; 1080p50: 1000/50000; 1080p25: 1000/25000; 1080i5994: 1001/30000
    int fieldsPerFrame() const;          // 2 or 1
    double fieldSeconds() const;         // frameDuration / timescale / fieldsPerFrame
  };
  const VideoMode* findMode(std::string_view name);   // nullptr if unknown
  const std::vector<VideoMode>& allModes();
  }
  ```
  `check.h`: `CHECK(cond)`, `CHECK_NEAR(a, b, tol)`, `TEST(name)` registering into a static list, and `int runAllTests()`; each test file has its own `main` calling it. CMake: `add_library(otc_core ...)`, one executable per `tests/test_*.cpp` registered with `add_test`; option `OTC_DECKLINK_SDK` (path, empty by default) gates `decklink_sink` and the DeckLink code in `ought-scrolltest`. Compile flags `-std=c++17 -O2 -Wall -Wextra -Werror` for core.

- [ ] **Step 1: Write the failing test** (`test_video_mode.cpp`)

```cpp
TEST(modes_have_expected_timing) {
  auto* i50 = otc::findMode("1080i50");
  CHECK(i50 && i50->interlaced && i50->upperFieldFirst);
  CHECK(i50->fieldsPerFrame() == 2);
  CHECK_NEAR(i50->fieldSeconds(), 0.020, 1e-9);
  auto* p50 = otc::findMode("1080p50");
  CHECK(p50 && !p50->interlaced && p50->fieldsPerFrame() == 1);
  CHECK_NEAR(p50->fieldSeconds(), 0.020, 1e-9);
  CHECK_NEAR(otc::findMode("1080p25")->fieldSeconds(), 0.040, 1e-9);
  CHECK_NEAR(otc::findMode("1080i5994")->fieldSeconds(), 1001.0 / 60000.0, 1e-12);
  CHECK(otc::findMode("720p50") == nullptr);
  CHECK(otc::allModes().size() == 4);
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake -S engine -B build && cmake --build build`
Expected: build error, `video_mode.h` missing.

- [ ] **Step 3: Implement `check.h`, `video_mode.{h,cpp}` and `engine/CMakeLists.txt`** with the interfaces above.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake -S engine -B build && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 5: Add the repo files**
  - `.gitignore`: `build/`, `sdk/` (except `sdk/README.md`), `*.pgm` under `engine/out/`.
  - `README.md`: one paragraph on the project (from the spec's Intent), build commands with and without the SDK, pointer to `docs/SCROLL-TEST.md`.
  - `CLAUDE.md`: shape of the system (spec's Parts), where the spec and plans live, the rules: SDK never committed; card clock is master; motion per field; npm safety rule for later JS work (`.npmrc` blocks install scripts, `npm ci`, new packages only via a vetted path, nothing published in the last 7 days); keep `docs/PROJECT-LOG.md` current.
  - `docs/PROJECT-LOG.md`: decisions so far (approach 2, Linux first, dedicated box, test on the CasparCG box, ShuttlePRO recommended, 1080i50 default), each dated 2026-11-17.
  - `.github/workflows/ci.yml`: on push and pull_request, `ubuntu-24.04`, configure without SDK, build, `ctest --output-on-failure`.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "Engine skeleton: CMake, test harness, video modes, CI"
```

---

### Task 2: Motion

**Files:**
- Create: `engine/src/core/motion.h`, `engine/src/core/motion.cpp`
- Test: `engine/tests/test_motion.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace otc {
  class Motion {
   public:
    explicit Motion(double settleSeconds = 0.4);   // time to reach ~95% of a speed change
    void setTargetSpeed(double pxPerSecond);       // negative = backwards
    void jumpTo(double y);                         // instant, speed and acceleration -> 0, target kept
    void setLimits(double minY, double maxY);
    void advance(double dtSeconds);
    double position() const;
    double speed() const;
    double targetSpeed() const;
  };
  }
  ```
  Units: position is the strip row shown at the top of the frame (may be negative: blank above the script); speed in px per second.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(constant_speed_moves_exactly) {      // after easing has settled
  otc::Motion m(0.4); m.setLimits(-1e9, 1e9); m.setTargetSpeed(100);
  for (int i = 0; i < 100; ++i) m.advance(0.02);     // 2 s
  double p0 = m.position();
  for (int i = 0; i < 50; ++i) m.advance(0.02);      // 1 s more
  CHECK_NEAR(m.position() - p0, 100.0, 0.5);
}
TEST(speed_change_eases_without_overshoot) {
  otc::Motion m(0.4); m.setLimits(-1e9, 1e9); m.setTargetSpeed(200);
  double prev = 0; bool monotone = true; double maxSpeed = 0; double prevPos = m.position();
  for (int i = 0; i < 100; ++i) {
    m.advance(0.02);
    monotone &= m.speed() >= prev - 1e-9; prev = m.speed();
    maxSpeed = std::max(maxSpeed, m.speed());
    CHECK(m.position() >= prevPos); prevPos = m.position();
    if (i == 0) CHECK(m.speed() < 200 * 0.1);        // no jump on the first field
    if (i == 24) CHECK(m.speed() > 200 * 0.95);      // ~95% at 0.4 s, safely past it by 0.5 s
  }
  CHECK(monotone); CHECK(maxSpeed <= 200 + 1e-6);
}
TEST(zero_speed_does_not_drift) {
  otc::Motion m; m.setLimits(-1e9, 1e9); m.jumpTo(123.25);
  for (int i = 0; i < 1000; ++i) m.advance(0.02);
  CHECK(m.position() == 123.25);
}
TEST(slow_speed_advances_evenly) {         // 2 px/s once settled: each 20 ms field moves 0.04 px
  otc::Motion m; m.setLimits(-1e9, 1e9); m.setTargetSpeed(2);
  for (int i = 0; i < 100; ++i) m.advance(0.02);
  for (int i = 0; i < 50; ++i) { double p = m.position(); m.advance(0.02); CHECK_NEAR(m.position() - p, 0.04, 1e-4); }
}
TEST(stops_at_limits_and_can_reverse) {
  otc::Motion m; m.setLimits(0, 500); m.jumpTo(490); m.setTargetSpeed(300);
  for (int i = 0; i < 100; ++i) m.advance(0.02);
  CHECK(m.position() == 500); CHECK(m.speed() == 0);
  m.setTargetSpeed(-300);
  for (int i = 0; i < 25; ++i) m.advance(0.02);
  CHECK(m.position() < 500);
  m.jumpTo(-50); CHECK(m.position() == 0);             // jumpTo clamps too
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R motion --output-on-failure`
Expected: build error, `motion.h` missing.

- [ ] **Step 3: Implement `Motion`**

Speed follows the target as a critically damped spring, integrated in 1 ms sub-steps so 20 ms fields never overshoot; position integrates speed. With ω = 4.74 / settleSeconds:

```cpp
// per sub-step h (h = dt / ceil(dt / 0.001)):
accel += h * (omega * omega * (target - speed) - 2 * omega * accel);
speed += h * accel;
// snap when settled, so zero really is zero and constant speed is exact:
if (std::abs(target - speed) < 1e-6 && std::abs(accel) < 1e-6) { speed = target; accel = 0; }
pos += h * speed;
if (pos <= minY || pos >= maxY) { pos = clamp(pos); if moving further out: speed = accel = 0; }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add engine && git commit -m "Engine: eased scroll motion with limits"
```

---

### Task 3: Strip, PGM and test patterns

**Files:**
- Create: `engine/src/core/strip.h`, `engine/src/core/strip.cpp`, `engine/src/core/pgm.h`, `engine/src/core/pgm.cpp`
- Test: `engine/tests/test_strip.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace otc {
  struct Strip {                       // the laid-out script, ready to copy into frames
    int width = 1920, height = 0;
    std::vector<uint8_t> uyvy;         // height rows of width*2 bytes
    const uint8_t* row(int y) const;   // nullptr if y outside [0, height)
  };
  Strip stripFromGrey(int width, int height, const uint8_t* grey);  // Y = 16 + g*219/255 rounded, U = V = 128
  Strip makeBarsStrip(int height);       // horizontal white bars 1, 2, 4, 8 px thick, repeating every 64 px
  Strip makeGridStrip(int height);       // 1 px white lines every 100 px both ways, 3 px every 500 px
  Strip makePseudoTextStrip(int height); // rows of word-like blocks: 80 px line pitch, 56 px cap height, soft (anti-aliased) 2 px edges
  struct PgmImage { int width = 0, height = 0; std::vector<uint8_t> pixels; };
  bool readPgm(const std::string& path, PgmImage& out, std::string& error);   // P5, maxval 255 only
  bool writePgm(const std::string& path, const PgmImage& img);
  PgmImage lumaOf(const uint8_t* uyvyFrame, int width, int height, int rowBytes);  // for dumps and tests
  }
  ```

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(grey_maps_to_video_range) {
  uint8_t g[4] = {0, 255, 128, 0};
  auto s = otc::stripFromGrey(2, 2, g);
  CHECK(s.uyvy.size() == 2 * 2 * 2);
  CHECK(s.row(0)[1] == 16);  CHECK(s.row(0)[3] == 235);   // Y bytes
  CHECK(s.row(0)[0] == 128); CHECK(s.row(0)[2] == 128);   // U, V
  CHECK(s.row(1)[1] == 126);
  CHECK(s.row(-1) == nullptr); CHECK(s.row(2) == nullptr);
}
TEST(patterns_have_requested_size_and_content) {
  for (auto s : {otc::makeBarsStrip(4000), otc::makeGridStrip(4000), otc::makePseudoTextStrip(4000)}) {
    CHECK(s.width == 1920 && s.height == 4000);
    int bright = 0; for (int y = 0; y < s.height; ++y) bright += s.row(y)[1 + 2 * 960] > 128;
    CHECK(bright > 0 && bright < s.height);              // neither blank nor solid
  }
}
TEST(pgm_round_trip_and_errors) {
  otc::PgmImage img{1920, 3, std::vector<uint8_t>(1920 * 3, 200)};
  CHECK(otc::writePgm("t.pgm", img));
  otc::PgmImage back; std::string err;
  CHECK(otc::readPgm("t.pgm", back, err) && back.height == 3 && back.pixels[5] == 200);
  // write "P2\n1 1\n255\n0\n" to bad.pgm, and a P5 header promising 1920x10 with 100 bytes of data to short.pgm
  CHECK(!otc::readPgm("bad.pgm", back, err) && !err.empty());
  CHECK(!otc::readPgm("short.pgm", back, err) && !err.empty());
  CHECK(!otc::readPgm("missing.pgm", back, err) && !err.empty());
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R strip --output-on-failure`
Expected: build error, `strip.h` missing.

- [ ] **Step 3: Implement `strip.{h,cpp}` and `pgm.{h,cpp}`** (PGM reader accepts comments in the header; rejects anything but P5/maxval 255; checks the data length).

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add engine && git commit -m "Engine: UYVY strip, PGM I/O, test patterns"
```

---

### Task 4: Compositor

**Files:**
- Create: `engine/src/core/compositor.h`, `engine/src/core/compositor.cpp`
- Test: `engine/tests/test_compositor.cpp`

**Interfaces:**
- Consumes: `Strip` (Task 3).
- Produces:
  ```cpp
  namespace otc {
  enum class Interp { Linear, WholeLine };
  struct Look { int markerY = 360; bool showMarker = true; };
  // Writes the rows of one field (rowParity 0 = even rows, 1 = odd rows) or all rows (rowParity -1)
  // of a 1920x1080 UYVY frame. Output row y shows strip row (topY + y): Linear blends the two
  // nearest strip rows by the fraction; WholeLine uses floor(topY + 0.5). Rows outside the strip are black (Y 16, UV 128).
  void composeRows(const Strip& strip, double topY, int rowParity, Interp interp,
                   uint8_t* frame, int rowBytes, int height = 1080);
  // Reading-line marker: a white right-pointing triangle at the left edge, 40 px wide, 48 px tall, centred on look.markerY.
  void drawMarker(const Look& look, uint8_t* frame, int rowBytes);
  }
  ```

- [ ] **Step 1: Write the failing tests**

```cpp
// helper: Strip where row r has Y = 16 + (r % 200)
TEST(whole_line_copies_rows) { /* topY = 10.0: output row 0 has Y of strip row 10; row 5 of row 15 */ }
TEST(linear_blends_fraction) { /* topY = 10.25: output row 0 Y == round(0.75*Y(10) + 0.25*Y(11)) */ }
TEST(integer_position_is_identical_in_both_modes) { /* topY = 42.0: Linear frame == WholeLine frame byte for byte */ }
TEST(parity_writes_only_its_rows) {
  /* fill frame with 0x55; composeRows(..., rowParity 0, ...); every odd row still 0x55, every even row changed */
}
TEST(outside_strip_is_black) {
  /* strip height 100, topY = -50: rows 0..49 are Y16/UV128; topY = 90: rows 10.. are black */
}
TEST(empty_strip_renders_black) { /* Strip{1920, 0, {}}: whole frame Y16/UV128, no crash */ }
TEST(marker_is_drawn_at_marker_y) { /* drawMarker: Y at (x=2, y=360) == 235; at (x=2, y=300) unchanged; showMarker=false changes nothing */ }
TEST(compose_is_fast_enough) {
  /* composeRows of a full progressive frame, Linear, 100 times; average < 5 ms (a field lasts 20 ms) */
}
```
Write each with explicit `CHECK`s on the byte values named in its comment.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R compositor --output-on-failure`
Expected: build error, `compositor.h` missing.

- [ ] **Step 3: Implement `composeRows` and `drawMarker`** (Linear blends each byte of the two UYVY rows with a 0..256 integer weight; this is valid because the conversion is linear per byte).

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add engine && git commit -m "Engine: field compositor and reading-line marker"
```

---

### Task 5: ScrollSource

**Files:**
- Create: `engine/src/core/scroll_source.h`, `engine/src/core/scroll_source.cpp`, `engine/src/core/counter_pattern.h` (copy of `decklinkExperiments/loopback/cpp/pattern.h` unchanged, with a first-line comment naming where it came from)
- Test: `engine/tests/test_scroll_source.cpp`

**Interfaces:**
- Consumes: `VideoMode` (Task 1), `Motion` (Task 2), `Strip` (Task 3), `composeRows`, `drawMarker`, `Interp`, `Look` (Task 4).
- Produces:
  ```cpp
  namespace otc {
  class ScrollSource {
   public:
    ScrollSource(const VideoMode& mode, Strip strip, Look look, Interp interp);
    // Renders the next frame. Interlaced: the first field's rows (upper = even when upperFieldFirst)
    // at the current position, advance(fieldSeconds), the other field's rows, advance(fieldSeconds).
    // Progressive: all rows, advance(fieldSeconds). Then the marker; then, if counter is on,
    // counter_pattern encode(frame, rowBytes, frameIndex). Thread-safe against the commands below.
    void renderNextFrame(uint8_t* frame, int rowBytes);
    void setTargetSpeed(double pxPerSecond);
    void jumpTo(double y);
    void setInterp(Interp i);
    void setShowMarker(bool on);
    void setCounter(bool on);
    uint64_t framesRendered() const;
    double position() const;   // top row of the next frame
    double speed() const;
  };
  }
  ```
  Limits: `[-look.markerY, strip.height - look.markerY]`, so the first line can sit at the reading line and the last line can reach it. Initial position `-look.markerY`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(fields_sample_positions_20ms_apart) {
  /* 1080i50, strip row r has Y = 16 + (r % 200), WholeLine, marker off. Settle at 50 px/s (1 px per field),
     render: even rows show topY p, odd rows show topY p + 1 (i.e. output row 1 shows strip row p+2) */
}
TEST(progressive_advances_once_per_frame) { /* 1080p25 at settled 50 px/s: position advances 2 px per frame */ }
TEST(command_applies_on_next_frame) {
  /* render 3 frames at speed 0; setTargetSpeed(500); render 1 frame; speed() > 0 and position() > start  */
}
TEST(zero_speed_frames_are_identical) { /* Linear, jumpTo(10.37), two renders produce byte-identical frames */ }
TEST(counter_written_when_on) { /* setCounter(true): rows 480..559 differ between frame 0 and frame 1; off: identical at speed 0 */ }
TEST(starts_with_first_line_at_reading_line) { /* position() == -360 initially; jumpTo(-1000) clamps to -360 */ }
```
Write each with explicit `CHECK`s.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R scroll_source --output-on-failure`
Expected: build error.

- [ ] **Step 3: Implement `ScrollSource`** (one `std::mutex` guarding motion and settings; `renderNextFrame` holds it only while reading/advancing motion and copying settings, not while composing).

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add engine && git commit -m "Engine: ScrollSource renders frames with per-field motion"
```

---

### Task 6: Headless sink and `ought-scrolltest` (no card)

**Files:**
- Create: `engine/src/sinks/headless_sink.h`, `engine/src/sinks/headless_sink.cpp`, `engine/src/apps/scrolltest_main.cpp`, `engine/tools/make_text_strip.sh`
- Modify: `engine/CMakeLists.txt` (executable `ought-scrolltest`; ctest smoke test)
- Test: `engine/tests/test_headless_sink.cpp`

**Interfaces:**
- Consumes: `ScrollSource` (Task 5), `findMode`, `readPgm`, `stripFromGrey`, `make*Strip`, `writePgm`, `lumaOf`.
- Produces:
  ```cpp
  namespace otc {
  struct HeadlessOptions { int frames = 100; std::string dumpDir; int dumpEvery = 0; };  // dumpEvery 0 = no dumps
  int runHeadless(ScrollSource& src, const HeadlessOptions& opt);   // returns 0; dumps frame_NNNNNN.pgm (luma)
  }
  ```
  CLI (`ought-scrolltest`):
  ```
  --sink headless|decklink   (default decklink if built with the SDK, else headless)
  --list                     list DeckLink devices (decklink builds only)
  --device N                 DeckLink device index (default 0)
  --mode 1080i50|1080p25|1080p50|1080i5994   (default 1080i50)
  --pattern text|bars|grid   (default text: makePseudoTextStrip(40000))
  --strip FILE.pgm           use a 1920-wide PGM strip instead of a pattern
  --speed PX                 starting target speed in px/s (default 0)
  --interp linear|wholeline  (default linear)
  --buffer N                 frames kept queued on the card (default 4)
  --counter                  overlay the loopback frame counter (rows 480-559)
  --seconds S                stop after S seconds (default: run until q / Ctrl-C)
  --frames N --dump DIR --dump-every K    (headless only)
  ```
  Unknown option, unknown mode, or a strip that isn't 1920 wide: message on stderr, exit 2.
  `make_text_strip.sh INPUT.txt OUTPUT.pgm [POINTSIZE=80]`: `convert -size 1920x -background black -fill white -font DejaVu-Sans -pointsize "$size" -interline-spacing 20 caption:@"$in" -bordercolor black -border 120x0 -resize 1920x -depth 8 pgm:"$out"` (120 px side margins; result exactly 1920 wide).

- [ ] **Step 1: Write the failing test** (`test_headless_sink.cpp`)

```cpp
TEST(headless_dumps_requested_frames) {
  /* 1080p50 ScrollSource on makeBarsStrip(4000), speed 100; runHeadless({10, "dumps", 5});
     files dumps/frame_000000.pgm and frame_000005.pgm exist, 1920x1080; no frame_000001.pgm; framesRendered()==10 */
}
```
And in CMake, smoke tests: `ought-scrolltest --sink headless --frames 20` exits 0; `ought-scrolltest --mode 720p` exits 2 (`WILL_FAIL` or check the exit code with a `cmake -E` wrapper).

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R headless --output-on-failure`
Expected: build error.

- [ ] **Step 3: Implement `runHeadless`, the CLI (DeckLink options parsed but rejected with exit 2 in builds without the SDK) and `make_text_strip.sh`**

- [ ] **Step 4: Run tests and try it**

Run: `cmake --build build && ctest --test-dir build --output-on-failure && ./build/ought-scrolltest --sink headless --pattern text --speed 120 --frames 50 --dump /tmp/otc --dump-every 10`
Expected: all tests pass; 5 PGM files; viewing two of them shows the text moved up by 48 px (120 px/s × 0.4 s).

- [ ] **Step 5: Commit**

```bash
git add engine && git commit -m "Engine: headless sink and ought-scrolltest CLI"
```

---

### Task 7: DeckLink sink and keyboard control

**Needs the SDK 16.0 `include/` folder.** If it isn't in `sdk/include`, ask Brendan to upload the include zip (see `decklinkExperiments/sdk/README.md` for how to unpack it safely). Without it this task can be written but not compiled; say so plainly rather than claiming it builds.

**Files:**
- Create: `engine/src/sinks/decklink/decklink_sink.h`, `engine/src/sinks/decklink/decklink_sink.cpp`
- Modify: `engine/src/apps/scrolltest_main.cpp`, `engine/CMakeLists.txt`

**Interfaces:**
- Consumes: `ScrollSource`, `VideoMode`.
- Produces:
  ```cpp
  namespace otc {
  struct DeckLinkStats { uint64_t scheduled, completed, late, dropped, flushed; uint32_t buffered; };
  int listDeckLinkDevices();                        // prints index, name, profile/duplex, which of our modes are supported
  class DeckLinkSink {
   public:
    bool open(int deviceIndex, const VideoMode& mode, std::string& error);  // checks DoesSupportVideoMode for bmdFormat8BitYUV
    bool start(ScrollSource& src, int bufferFrames, std::string& error);   // pre-roll bufferFrames, StartScheduledPlayback
    void stop();                                    // the spike's stop sequence
    DeckLinkStats stats() const;
  };
  }
  ```
  Port from `decklinkExperiments/loopback/cpp/dl_output_spike.cpp` (Brendan's own code): `Ref<T>`, `queryInterface`, `sameIID`, device enumeration, the `IDeckLinkVideoOutputCallback`, pre-roll and reschedule-on-completion loop, completion-result counting, and the stop sequence (`StopScheduledPlayback` → wait `ScheduledPlaybackHasStopped` → wait for flush completions → `DisableVideoOutput`). Replace `pattern::fillFrame` with `src.renderNextFrame(bytes, rowBytes)`. Mode mapping: `1080i50`→`bmdModeHD1080i50`, `1080p25`→`bmdModeHD1080p25`, `1080p50`→`bmdModeHD1080p50`, `1080i5994`→`bmdModeHD1080i5994`; schedule times from `VideoMode::frameDuration/timescale`, never the system clock.
  Keyboard (terminal in raw mode, restored on exit and on Ctrl-C): `↑`/`↓` speed ±10 px/s, `PgUp`/`PgDn` ±100 px/s, `0` or space stop, `Home` back to top, `i` toggle interp, `m` toggle marker, `c` toggle counter, `q` quit. A status line once a second: mode, interp, buffer, speed, position, late, dropped.

- [ ] **Step 1: Build with the SDK**

Run: `cmake -S engine -B build-dl -DOTC_DECKLINK_SDK=$PWD/sdk/include && cmake --build build-dl`
Expected: builds with no warnings; `./build-dl/ought-scrolltest --list` prints "DeckLink driver not found" here (no card) and exits non-zero.

- [ ] **Step 2: Check the build without the SDK still passes CI**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 3: Commit**

```bash
git add engine && git commit -m "Engine: DeckLink sink and keyboard control for the scroll test"
```

---

### Task 8: The hardware test procedure

**Files:**
- Create: `docs/SCROLL-TEST.md`
- Modify: `docs/PROJECT-LOG.md`, `README.md`

- [ ] **Step 1: Write `docs/SCROLL-TEST.md`** for Brendan on the CasparCG box: single copy-pasteable commands (no lines pasted after a `sudo`), in this order:
  1. Stop CasparCG (its usual stop command from `decklinkExperiments/casparcg-ops`).
  2. Clone the branch, put the SDK headers in `sdk/include`, build: `cmake -S engine -B build-dl -DOTC_DECKLINK_SDK=$PWD/sdk/include && cmake --build build-dl`.
  3. `./build-dl/ought-scrolltest --list`; note which index drives the output cabled to the monitor.
  4. Make a real-text strip: `engine/tools/make_text_strip.sh some-script.txt /tmp/script.pgm`.
  5. The runs, each about two minutes, with what to look at and a results table to fill in (speed, mode, interp, buffer → smooth / judders / blurs / shimmer; late; dropped):
     - slow 10 and 30 px/s, `--interp linear` then `wholeline`, real text (the main question: which reads better slowly);
     - normal 100–200 px/s, both methods;
     - fast 600 px/s and reverse;
     - `--pattern bars` at 50 px/s (judder and field-order check: bars must move steadily, no back-and-forth flicker);
     - `--buffer 2`, `3`, `4`, `10` at 150 px/s: lowest value with zero late/dropped frames, and whether the lag after an arrow key feels acceptable;
     - one 10-minute run at 1080i50 with `--counter` while `decklink_capture.py` from `decklinkExperiments` records the loopback (SDI out → SDI in), then `summarize_run.sh` for zero missing or repeated frames.
  6. Restart CasparCG.
  Ends with: paste the filled-in table and the `--list` output back to Claude.

- [ ] **Step 2: Log it**: `docs/PROJECT-LOG.md` gets "Stage 1 built: engine core and scroll test; waiting on the hardware test to choose the slow-motion method and buffer depth". README links the procedure.

- [ ] **Step 3: Verify the commands in the doc that can run here**

Run: `cmake -S engine -B build && cmake --build build && ctest --test-dir build --output-on-failure && engine/tools/make_text_strip.sh README.md /tmp/readme.pgm && ./build/ought-scrolltest --sink headless --strip /tmp/readme.pgm --frames 10`
Expected: tests pass; the strip is 1920 wide; the run exits 0.

- [ ] **Step 4: Commit and push**

```bash
git add -A && git commit -m "Scroll test procedure for the CasparCG box"
git -c push.negotiate=true push
```
