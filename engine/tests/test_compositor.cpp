#include <chrono>
#include <cmath>
#include <vector>

#include "check.h"
#include "core/compositor.h"
#include "core/strip.h"

namespace
{
constexpr int kW = 1920, kH = 1080, kRowBytes = kW * 2;

uint8_t yOfRow(int r) { return static_cast<uint8_t>(16 + (r % 200)); }

// Strip whose row r has Y = 16 + (r % 200) everywhere, chroma 128.
otc::Strip rampStrip(int height)
{
	otc::Strip s;
	s.height = height;
	s.uyvy.resize(static_cast<size_t>(kW) * height * 2);
	for (int r = 0; r < height; ++r)
		for (int x = 0; x < kW; ++x)
		{
			s.uyvy[(static_cast<size_t>(r) * kW + x) * 2] = 128;
			s.uyvy[(static_cast<size_t>(r) * kW + x) * 2 + 1] = yOfRow(r);
		}
	return s;
}

std::vector<uint8_t> frame(uint8_t fill = 0) { return std::vector<uint8_t>(static_cast<size_t>(kRowBytes) * kH, fill); }
uint8_t yAt(const std::vector<uint8_t>& f, int x, int y) { return f[static_cast<size_t>(y) * kRowBytes + 2 * x + 1]; }
uint8_t uvAt(const std::vector<uint8_t>& f, int x, int y) { return f[static_cast<size_t>(y) * kRowBytes + 2 * x]; }
}  // namespace

TEST(whole_line_copies_rows)
{
	auto s = rampStrip(2000);
	auto f = frame();
	otc::composeRows(s, 10.0, -1, otc::Interp::WholeLine, f.data(), kRowBytes);
	CHECK(yAt(f, 0, 0) == yOfRow(10));
	CHECK(yAt(f, 100, 5) == yOfRow(15));
	CHECK(yAt(f, 1919, 1079) == yOfRow(1089));
	otc::composeRows(s, 10.6, -1, otc::Interp::WholeLine, f.data(), kRowBytes);  // rounds to 11
	CHECK(yAt(f, 0, 0) == yOfRow(11));
}

TEST(linear_blends_fraction)
{
	auto s = rampStrip(2000);
	auto f = frame();
	otc::composeRows(s, 10.25, -1, otc::Interp::Linear, f.data(), kRowBytes);
	const int expected = static_cast<int>(std::lround(0.75 * yOfRow(10) + 0.25 * yOfRow(11)));
	CHECK(std::abs(yAt(f, 0, 0) - expected) <= 1);
	CHECK(uvAt(f, 0, 0) == 128);
}

TEST(integer_position_is_identical_in_both_modes)
{
	auto s = rampStrip(2000);
	auto a = frame(), b = frame();
	otc::composeRows(s, 42.0, -1, otc::Interp::Linear, a.data(), kRowBytes);
	otc::composeRows(s, 42.0, -1, otc::Interp::WholeLine, b.data(), kRowBytes);
	CHECK(a == b);
}

TEST(parity_writes_only_its_rows)
{
	auto s = rampStrip(2000);
	auto f = frame(0x55);
	otc::composeRows(s, 0.0, 0, otc::Interp::WholeLine, f.data(), kRowBytes);
	bool oddUntouched = true, evenWritten = true;
	for (int y = 0; y < kH; ++y)
	{
		if (y % 2)
			oddUntouched = oddUntouched && yAt(f, 7, y) == 0x55 && uvAt(f, 7, y) == 0x55;
		else
			evenWritten = evenWritten && yAt(f, 7, y) == yOfRow(y);
	}
	CHECK(oddUntouched);
	CHECK(evenWritten);
}

TEST(outside_strip_is_black)
{
	auto s = rampStrip(100);
	auto f = frame(0x55);
	otc::composeRows(s, -50.0, -1, otc::Interp::Linear, f.data(), kRowBytes);
	CHECK(yAt(f, 0, 0) == 16 && uvAt(f, 0, 0) == 128);
	CHECK(yAt(f, 0, 49) == 16);
	CHECK(yAt(f, 0, 50) == yOfRow(0));
	CHECK(yAt(f, 0, 149) == yOfRow(99));
	CHECK(yAt(f, 0, 150) == 16 && uvAt(f, 0, 150) == 128);
	otc::composeRows(s, 90.0, -1, otc::Interp::WholeLine, f.data(), kRowBytes);
	CHECK(yAt(f, 0, 9) == yOfRow(99));
	CHECK(yAt(f, 0, 10) == 16 && yAt(f, 0, 1079) == 16);
}

TEST(empty_strip_renders_black)
{
	otc::Strip empty{1920, 0, {}};
	auto f = frame(0x55);
	otc::composeRows(empty, 0.0, -1, otc::Interp::Linear, f.data(), kRowBytes);
	bool black = true;
	for (int y = 0; y < kH; y += 7)
		black = black && yAt(f, 3, y) == 16 && uvAt(f, 3, y) == 128;
	CHECK(black);
}

TEST(marker_is_drawn_at_marker_y)
{
	auto f = frame(16);
	otc::Look look;  // markerY 360
	otc::drawMarker(look, f.data(), kRowBytes);
	CHECK(yAt(f, 2, 360) == 235);
	CHECK(yAt(f, 2, 300) == 16);
	CHECK(yAt(f, 60, 360) == 16);  // 40 px wide
	auto g = frame(16);
	look.showMarker = false;
	otc::drawMarker(look, g.data(), kRowBytes);
	CHECK(g == frame(16));
}

TEST(compose_is_fast_enough)
{
	auto s = rampStrip(4000);
	auto f = frame();
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < 100; ++i)
		otc::composeRows(s, 10.0 + i * 0.37, -1, otc::Interp::Linear, f.data(), kRowBytes);
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 100;
	std::printf("  compose: %.2f ms per frame\n", ms);
	CHECK(ms < 5.0);  // a field lasts 20 ms
}

TEST_MAIN()
