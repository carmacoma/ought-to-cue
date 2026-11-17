#include "core/compositor.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace otc
{
namespace
{
constexpr uint8_t kBlackY = 16, kNeutralUV = 128, kWhiteY = 235;

constexpr int kMaxWidth = 8192;

// One black row, built once (thread-safe static initialisation), wide enough for any strip.
const uint8_t* blackRow()
{
	static const std::vector<uint8_t> row = [] {
		std::vector<uint8_t> r(static_cast<size_t>(kMaxWidth) * 2);
		for (size_t i = 0; i < r.size(); i += 2)
		{
			r[i] = kNeutralUV;
			r[i + 1] = kBlackY;
		}
		return r;
	}();
	return row.data();
}
}  // namespace

void composeRows(const Strip& strip, double topY, int rowParity, Interp interp, uint8_t* frame, int rowBytes,
                 int height)
{
	const int width = strip.width <= kMaxWidth ? strip.width : kMaxWidth;
	const size_t bytes = static_cast<size_t>(width) * 2;
	const uint8_t* black = blackRow();
	auto rowOrBlack = [&](int r) {
		const uint8_t* p = strip.row(r);
		return p ? p : black;
	};

	const int start = rowParity == 1 ? 1 : 0;
	const int step = rowParity < 0 ? 1 : 2;

	if (interp == Interp::WholeLine)
	{
		const int base = static_cast<int>(std::floor(topY + 0.5));
		for (int y = start; y < height; y += step)
			std::memcpy(frame + static_cast<size_t>(y) * rowBytes, rowOrBlack(base + y), bytes);
		return;
	}

	const double fl = std::floor(topY);
	const int base = static_cast<int>(fl);
	const int w = static_cast<int>(std::lround((topY - fl) * 256));  // weight of the lower row, 0..256
	for (int y = start; y < height; y += step)
	{
		uint8_t* out = frame + static_cast<size_t>(y) * rowBytes;
		const uint8_t* a = rowOrBlack(base + y);
		if (w == 0)
		{
			std::memcpy(out, a, bytes);
			continue;
		}
		const uint8_t* b = rowOrBlack(base + y + 1);
		if (w == 256)
		{
			std::memcpy(out, b, bytes);
			continue;
		}
		// Blending UYVY bytes directly is exact: each byte is a linear quantity.
		for (size_t i = 0; i < bytes; ++i)
			out[i] = static_cast<uint8_t>((a[i] * (256 - w) + b[i] * w + 128) >> 8);
	}
}

void drawMarker(const Look& look, uint8_t* frame, int rowBytes)
{
	if (!look.showMarker)
		return;
	constexpr int kMaxW = 40, kHalfH = 24;
	for (int dy = -kHalfH; dy <= kHalfH; ++dy)
	{
		const int y = look.markerY + dy;
		if (y < 0 || y >= 1080)
			continue;
		const int w = static_cast<int>(std::lround(kMaxW * (1.0 - std::abs(dy) / static_cast<double>(kHalfH))));
		uint8_t* row = frame + static_cast<size_t>(y) * rowBytes;
		for (int x = 0; x < w; ++x)
		{
			row[2 * x] = kNeutralUV;
			row[2 * x + 1] = kWhiteY;
		}
	}
}
}  // namespace otc
