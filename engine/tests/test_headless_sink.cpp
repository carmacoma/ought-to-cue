#include <filesystem>

#include "check.h"
#include "core/pgm.h"
#include "core/scroll_source.h"
#include "core/video_mode.h"
#include "sinks/headless_sink.h"

namespace fs = std::filesystem;

TEST(headless_dumps_requested_frames)
{
	fs::remove_all("dumps");
	otc::ScrollSource src(*otc::findMode("1080p50"), otc::makeBarsStrip(4000), otc::Look{}, otc::Interp::Linear);
	src.setTargetSpeed(100);
	CHECK(otc::runHeadless(src, {10, "dumps", 5}) == 0);
	CHECK(src.framesRendered() == 10);
	CHECK(fs::exists("dumps/frame_000000.pgm"));
	CHECK(fs::exists("dumps/frame_000005.pgm"));
	CHECK(!fs::exists("dumps/frame_000001.pgm"));
	otc::PgmImage img;
	std::string err;
	CHECK(otc::readPgm("dumps/frame_000005.pgm", img, err) && img.width == 1920 && img.height == 1080);
}

TEST(headless_without_dumps_writes_nothing)
{
	fs::remove_all("nodumps");
	otc::ScrollSource src(*otc::findMode("1080i50"), otc::makeGridStrip(2000), otc::Look{}, otc::Interp::Linear);
	CHECK(otc::runHeadless(src, {3, "nodumps", 0}) == 0);
	CHECK(src.framesRendered() == 3);
	CHECK(!fs::exists("nodumps"));
}

TEST_MAIN()
