// The scroll test on a DeckLink card: output, keyboard control in the terminal, a status line.
#pragma once

#include "core/scroll_source.h"
#include "core/video_mode.h"

namespace otc
{
int listDeckLinkDevices();
// Runs until q, Ctrl-C, or `seconds` (0 = no limit). Returns 0 on a clean run, 1 on device errors.
int runDeckLinkScrollTest(ScrollSource& src, const VideoMode& mode, int device, int bufferFrames, double seconds);
}  // namespace otc
