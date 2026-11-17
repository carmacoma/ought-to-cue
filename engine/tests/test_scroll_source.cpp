#include <cmath>
#include <vector>

#include "check.h"
#include "core/scroll_source.h"
#include "core/video_mode.h"

namespace
{
constexpr int kW = 1920, kH = 1080, kRowBytes = kW * 2;

uint8_t yOfRow(int r) { return static_cast<uint8_t>(16 + (r % 200)); }

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

std::vector<uint8_t> frame() { return std::vector<uint8_t>(static_cast<size_t>(kRowBytes) * kH, 0); }
uint8_t yAt(const std::vector<uint8_t>& f, int x, int y) { return f[static_cast<size_t>(y) * kRowBytes + 2 * x + 1]; }
otc::Look noMarker()
{
	otc::Look l;
	l.showMarker = false;
	return l;
}
int nearest(double v) { return static_cast<int>(std::floor(v + 0.5)); }
}  // namespace

TEST(fields_sample_positions_20ms_apart)
{
	otc::ScrollSource src(*otc::findMode("1080i50"), rampStrip(20000), noMarker(), otc::Interp::WholeLine);
	auto f = frame();
	src.setTargetSpeed(50);  // 1 px per field once settled
	for (int i = 0; i < 100; ++i)
		src.renderNextFrame(f.data(), kRowBytes);
	const double p = src.position();
	src.renderNextFrame(f.data(), kRowBytes);
	// Upper field (even rows) at p, lower field (odd rows) one field later at p + 1.
	CHECK(yAt(f, 9, 400) == yOfRow(nearest(p) + 400));
	CHECK(yAt(f, 9, 401) == yOfRow(nearest(p + 1.0) + 401));
	CHECK_NEAR(src.position() - p, 2.0, 1e-6);  // two fields per frame
}

TEST(progressive_advances_once_per_frame)
{
	otc::ScrollSource src(*otc::findMode("1080p25"), rampStrip(20000), noMarker(), otc::Interp::WholeLine);
	auto f = frame();
	src.setTargetSpeed(50);
	for (int i = 0; i < 100; ++i)
		src.renderNextFrame(f.data(), kRowBytes);
	const double p = src.position();
	src.renderNextFrame(f.data(), kRowBytes);
	CHECK(yAt(f, 9, 401) == yOfRow(nearest(p) + 401));  // all rows from one position
	CHECK_NEAR(src.position() - p, 2.0, 1e-6);           // 50 px/s x 40 ms
}

TEST(command_applies_on_next_frame)
{
	otc::ScrollSource src(*otc::findMode("1080i50"), rampStrip(20000), noMarker(), otc::Interp::Linear);
	auto f = frame();
	for (int i = 0; i < 3; ++i)
		src.renderNextFrame(f.data(), kRowBytes);
	const double start = src.position();
	src.setTargetSpeed(500);
	src.renderNextFrame(f.data(), kRowBytes);
	CHECK(src.speed() > 0);
	CHECK(src.position() > start);
	CHECK(src.framesRendered() == 4);
}

TEST(zero_speed_frames_are_identical)
{
	otc::ScrollSource src(*otc::findMode("1080i50"), rampStrip(20000), otc::Look{}, otc::Interp::Linear);
	src.jumpTo(10.37);
	auto a = frame(), b = frame();
	src.renderNextFrame(a.data(), kRowBytes);
	src.renderNextFrame(b.data(), kRowBytes);
	CHECK(a == b);
}

TEST(counter_written_when_on)
{
	otc::ScrollSource src(*otc::findMode("1080i50"), otc::makePseudoTextStrip(4000), otc::Look{}, otc::Interp::Linear);
	auto a = frame(), b = frame();
	src.renderNextFrame(a.data(), kRowBytes);
	src.renderNextFrame(b.data(), kRowBytes);
	CHECK(a == b);  // off: speed 0 gives identical frames
	src.setCounter(true);
	src.renderNextFrame(a.data(), kRowBytes);
	src.renderNextFrame(b.data(), kRowBytes);
	bool differs = false, outsideSame = true;
	for (int y = 0; y < kH; ++y)
		for (int x = 0; x < kW; x += 5)
		{
			const bool same = yAt(a, x, y) == yAt(b, x, y);
			if (y >= 480 && y < 560)
				differs = differs || !same;
			else
				outsideSame = outsideSame && same;
		}
	CHECK(differs);
	CHECK(outsideSame);
}

TEST(starts_with_first_line_at_reading_line)
{
	otc::ScrollSource src(*otc::findMode("1080i50"), rampStrip(5000), otc::Look{}, otc::Interp::Linear);
	CHECK(src.position() == -360);
	src.jumpTo(-1000);
	CHECK(src.position() == -360);
	src.jumpTo(1e9);
	CHECK(src.position() == 5000 - 360);  // last line can reach the reading line
}

TEST_MAIN()
