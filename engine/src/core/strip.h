// The laid-out script as one tall image, already in the output's pixel format
// (8-bit YUV 4:2:2, byte order U Y V Y), so composing a frame is copying and blending rows.
#pragma once

#include <cstdint>
#include <vector>

namespace otc
{
struct Strip
{
	int width = 1920, height = 0;
	std::vector<uint8_t> uyvy;  // height rows of width*2 bytes

	const uint8_t* row(int y) const
	{
		if (y < 0 || y >= height)
			return nullptr;
		return uyvy.data() + static_cast<size_t>(y) * width * 2;
	}
};

// Greyscale (0 black .. 255 white) to video-range luma: Y = 16 + g*219/255 rounded, U = V = 128.
Strip stripFromGrey(int width, int height, const uint8_t* grey);

// Test patterns, all 1920 wide.
Strip makeBarsStrip(int height);        // horizontal white bars 1, 2, 4, 8 px thick, repeating every 64 px
Strip makeGridStrip(int height);        // 1 px white lines every 100 px both ways, 3 px every 500 px
Strip makePseudoTextStrip(int height);  // rows of word-like blocks: 80 px line pitch, 56 px cap height,
                                        // soft (anti-aliased) 2 px edges
}  // namespace otc
