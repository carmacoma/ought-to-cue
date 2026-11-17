# Scroll test on the CasparCG box

This test answers three questions before anything else is built on the engine:

1. **Slow scrolling:** which method reads better at slow speeds: `linear` (blends between lines: smooth,
   slightly soft) or `wholeline` (sharp, moves in whole-line steps).
2. **Buffer:** the fewest frames queued on the card that never drops a frame. Fewer means less lag between
   pressing a key (later, turning the wheel) and the text responding: each frame is 40 ms.
3. **Frame accounting:** whether a 10-minute run is clean (no late, dropped, missing or repeated frames),
   checked with the loopback tools from `decklinkExperiments`.

It takes about 45 minutes. CasparCG has to be stopped for the whole test, because the scroll test uses one of
its outputs. Run each command on its own (copy one block, paste, wait for it to finish). After a `sudo` command,
don't paste anything else until it has asked for your password and finished.

## 1. Stop CasparCG

If CasparCG runs under the systemd units:

```bash
systemctl --user stop casparcg-client casparcg-server casparcg-scanner
```

If it still runs from the script instead:

```bash
~/casparcg/start-casparcg.sh stop
```

## 2. Get the code and build it

```bash
git clone -b claude/brave-heisenberg-6wl6wj https://github.com/carmacoma/ought-to-cue ~/ought-to-cue
```

Copy the SDK 16.0 headers into the repo's git-ignored `sdk/include` folder. Change the source path to wherever
your unpacked SDK is (the folder that holds `DeckLinkAPI.h`):

```bash
mkdir -p ~/ought-to-cue/sdk/include && cp ~/path/to/DeckLink-SDK/Linux/include/* ~/ought-to-cue/sdk/include/
```

If `cmake --version` says it isn't installed:

```bash
sudo apt install -y cmake
```

Build and run the automated tests:

```bash
cd ~/ought-to-cue && cmake -S engine -B build-dl -DOTC_DECKLINK_SDK=$PWD/sdk/include && cmake --build build-dl && ctest --test-dir build-dl
```

Expected: `100% tests passed`.

## 3. Find the output

```bash
~/ought-to-cue/build-dl/ought-scrolltest --list
```

In the current profile (`2dfd`), index 0 is SDI 2 and index 1 is SDI 4. Use the index whose output goes to the
monitor you will watch. The commands below use `--device 0`; change it if yours is different. A monitor that
shows 1080i50 the way the prompter monitor will is best: some monitors deinterlace badly, which looks like
judder that isn't in the signal.

## 4. Make a strip of real text

Any plain-text file works; a real script is best. One paragraph per block of text, blank lines between:

```bash
~/ought-to-cue/engine/tools/make_text_strip.sh ~/some-script.txt /tmp/script.pgm
```

It prints the strip's size, e.g. `/tmp/script.pgm: 1920x24600`. The point size is the optional third argument
(default 80).

## 5. The runs

Each run starts scrolling at the speed given. While it runs, these keys work in the terminal:

| Key | Does |
|---|---|
| Up / Down | speed +10 / -10 px per second |
| Page Up / Page Down | speed +100 / -100 |
| 0 or space | stop (eases to a halt) |
| Home | back to the top |
| i | switch between `linear` and `wholeline` |
| m | reading-line marker on/off |
| c | loopback frame counter on/off |
| q | quit |

The status line shows the mode, method, buffer, speed, position and the card's late and dropped counts. For
reference, 100 px per second is about one line of 80-point text every 0.9 seconds.

**A. Slow speeds, real text** (the main question). Watch for judder (uneven steps), shimmer (text sharpness
pulsing) and how easy it is to read. Press `i` to compare the two methods without restarting.

```bash
~/ought-to-cue/build-dl/ought-scrolltest --device 0 --strip /tmp/script.pgm --speed 10
```

Try 10, 20, 30 and 50 px/s with Up/Down, each with `linear` and with `wholeline`.

**B. Normal and fast speeds.** Same command, then Page Up to about 150, 300 and 600 px/s; Page Down past zero
for backwards. Check that speed changes ease in and out with no jerk.

**C. Bars pattern** (judder and field-order check). The bars must move steadily, with no flicker or
back-and-forth twitching. Twitching means a field-order problem.

```bash
~/ought-to-cue/build-dl/ought-scrolltest --device 0 --pattern bars --speed 50
```

**D. Buffer depth.** Run at 150 px/s for about a minute each with `--buffer 2`, `3`, `4` and `10`. Note late and
dropped from the status line, and how laggy Up/Down feel.

```bash
~/ought-to-cue/build-dl/ought-scrolltest --device 0 --strip /tmp/script.pgm --speed 150 --buffer 2
```

**E. Ten-minute loopback run.** Cable **SDI 2 -> SDI 3** (as in the earlier loopback tests). Start the capture
from your `decklinkExperiments` checkout:

```bash
cd ~/decklinkExperiments/loopback/python && nohup python3 decklink_capture.py --log /tmp/otc-capture.log > /tmp/otc-capture.out 2>&1 &
```

Then the scroll test, writing to a log file (with the output redirected it prints a report line every 10 s
instead of the status line, and the keys don't work):

```bash
~/ought-to-cue/build-dl/ought-scrolltest --device 0 --strip /tmp/script.pgm --speed 100 --counter --buffer 4 --seconds 600 > /tmp/otc-run.log 2>&1
```

When it returns (10 minutes), summarise:

```bash
~/decklinkExperiments/loopback/tools/summarize_run.sh --stop-capture /tmp/otc-capture.log /tmp/otc-run.log /tmp/otc-capture.out
```

Expected: `PASS`. (If D showed the buffer needs more than 4, use that value here.)

## 6. Restart CasparCG

```bash
systemctl --user start casparcg-client.service
```

or, if it runs from the script:

```bash
~/casparcg/start-casparcg.sh start
```

## 7. Send back

Paste these back to Claude:

- the output of `--list` (step 3);
- the table below, filled in;
- the last 30 lines of the summary from E.

| Run | Speed (px/s) | Method | Buffer | Smooth? Judder? Shimmer? Soft? | Late | Dropped |
|---|---|---|---|---|---|---|
| A | 10 | linear | 4 | | | |
| A | 10 | wholeline | 4 | | | |
| A | 30 | linear | 4 | | | |
| A | 30 | wholeline | 4 | | | |
| A | 50 | linear | 4 | | | |
| A | 50 | wholeline | 4 | | | |
| B | 150 / 300 / 600 / backwards | (better one from A) | 4 | | | |
| C | 50 (bars) | linear | 4 | | | |
| D | 150 | | 2 | lag feel: | | |
| D | 150 | | 3 | lag feel: | | |
| D | 150 | | 4 | lag feel: | | |
| D | 150 | | 10 | lag feel: | | |
| E | 100, 10 minutes | | | summary verdict: | | |

Anything else you noticed (font size, the marker, how the easing feels) is useful too.
