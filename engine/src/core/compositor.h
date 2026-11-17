// Composes output frames (1920 wide, 8-bit UYVY) from the strip at a scroll position.
#pragma once

#include <cstdint>

#include "core/strip.h"

namespace otc
{
// How a fractional scroll position is drawn. Linear blends the two nearest strip rows (smooth,
// slightly soft between rows); WholeLine shows the nearest row (sharp, moves in whole-line steps).
// The hardware scroll test decides which reads better at slow speeds.
enum class Interp
{
	Linear,
	WholeLine
};

struct Look
{
	int markerY = 360;  // reading line, in output rows
	bool showMarker = true;
};

// Writes the rows of one field (rowParity 0 = even rows, 1 = odd rows) or all rows (rowParity -1).
// Output row y shows strip row (topY + y). Rows outside the strip are black (Y 16, UV 128).
void composeRows(const Strip& strip, double topY, int rowParity, Interp interp, uint8_t* frame, int rowBytes,
                 int height = 1080);

// Reading-line marker: a white right-pointing triangle at the left edge, 40 px wide, 48 px tall,
// centred on look.markerY. Does nothing when look.showMarker is false.
void drawMarker(const Look& look, uint8_t* frame, int rowBytes);
}  // namespace otc
