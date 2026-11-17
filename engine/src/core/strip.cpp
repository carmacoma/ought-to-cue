#include "core/strip.h"

#include <algorithm>

namespace otc
{
namespace
{
constexpr int kWidth = 1920;

uint8_t lumaFromGrey(uint8_t g) { return static_cast<uint8_t>(16 + (g * 219 + 127) / 255); }
}  // namespace

Strip stripFromGrey(int width, int height, const uint8_t* grey)
{
	Strip s;
	s.width = width;
	s.height = std::max(0, height);
	s.uyvy.resize(static_cast<size_t>(s.width) * s.height * 2);
	for (size_t i = 0, n = static_cast<size_t>(s.width) * s.height; i < n; ++i)
	{
		s.uyvy[2 * i] = 128;  // U or V
		s.uyvy[2 * i + 1] = lumaFromGrey(grey[i]);
	}
	return s;
}

Strip makeBarsStrip(int height)
{
	std::vector<uint8_t> g(static_cast<size_t>(kWidth) * std::max(0, height), 0);
	for (int y = 0; y < height; ++y)
	{
		const int p = y % 64;
		const bool on = p == 4 || (p >= 12 && p < 14) || (p >= 24 && p < 28) || (p >= 40 && p < 48);
		if (on)
			std::fill_n(g.begin() + static_cast<size_t>(y) * kWidth, kWidth, 255);
	}
	return stripFromGrey(kWidth, height, g.data());
}

Strip makeGridStrip(int height)
{
	auto onLine = [](int v) { return v % 100 == 0 || (v % 500) < 3 || (v % 500) > 497; };
	std::vector<uint8_t> g(static_cast<size_t>(kWidth) * std::max(0, height), 0);
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < kWidth; ++x)
			if (onLine(y) || onLine(x))
				g[static_cast<size_t>(y) * kWidth + x] = 255;
	return stripFromGrey(kWidth, height, g.data());
}

Strip makePseudoTextStrip(int height)
{
	constexpr int kPitch = 80, kCap = 56, kMargin = 120, kGap = 24, kEdge = 2;
	std::vector<uint8_t> g(static_cast<size_t>(kWidth) * std::max(0, height), 0);
	uint32_t rng = 12345;
	auto next = [&rng] {
		rng = rng * 1664525u + 1013904223u;
		return rng >> 8;
	};
	// Coverage ramps over kEdge px at each edge, so edges are soft like anti-aliased text.
	auto ramp = [](int d) { return std::min(1.0, (d + 0.5) / kEdge); };
	for (int line = 0; line * kPitch < height; ++line)
	{
		if (line % 7 == 6)
			continue;  // paragraph gap
		const int top = line * kPitch + (kPitch - kCap) / 2;
		const int lineEnd = kWidth - kMargin - static_cast<int>(next() % 400);
		for (int x0 = kMargin; x0 < lineEnd;)
		{
			const int w = 40 + static_cast<int>(next() % 220);
			const int x1 = std::min(x0 + w, lineEnd);
			for (int y = top; y < top + kCap && y < height; ++y)
			{
				const double cy = ramp(std::min(y - top, top + kCap - 1 - y));
				for (int x = x0; x < x1; ++x)
				{
					const double c = cy * ramp(std::min(x - x0, x1 - 1 - x));
					g[static_cast<size_t>(y) * kWidth + x] = static_cast<uint8_t>(255 * c + 0.5);
				}
			}
			x0 = x1 + kGap;
		}
	}
	return stripFromGrey(kWidth, height, g.data());
}
}  // namespace otc
