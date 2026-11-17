// Headless sink: renders frames with no output device, optionally dumping some as PGM (luma).
// For CI, tests, and machines without a DeckLink card.
#pragma once

#include <string>

#include "core/scroll_source.h"

namespace otc
{
struct HeadlessOptions
{
	int frames = 100;
	std::string dumpDir;
	int dumpEvery = 0;  // 0 = no dumps; otherwise frames 0, K, 2K, ... as frame_NNNNNN.pgm
};

int runHeadless(ScrollSource& src, const HeadlessOptions& opt);  // 0 on success
}  // namespace otc
