#include <cstdio>
#include <string>

#include "check.h"
#include "core/pgm.h"
#include "core/strip.h"

static void writeFile(const char* path, const std::string& data)
{
	FILE* f = std::fopen(path, "wb");
	std::fwrite(data.data(), 1, data.size(), f);
	std::fclose(f);
}

TEST(grey_maps_to_video_range)
{
	uint8_t g[4] = {0, 255, 128, 0};
	auto s = otc::stripFromGrey(2, 2, g);
	CHECK(s.uyvy.size() == 2 * 2 * 2);
	CHECK(s.row(0)[1] == 16);
	CHECK(s.row(0)[3] == 235);  // Y bytes
	CHECK(s.row(0)[0] == 128);
	CHECK(s.row(0)[2] == 128);  // U, V
	CHECK(s.row(1)[1] == 126);  // 16 + 128*219/255 = 125.9 -> 126
	CHECK(s.row(-1) == nullptr);
	CHECK(s.row(2) == nullptr);
}

TEST(patterns_have_requested_size_and_content)
{
	for (const auto& s : {otc::makeBarsStrip(4000), otc::makeGridStrip(4000), otc::makePseudoTextStrip(4000)})
	{
		CHECK(s.width == 1920 && s.height == 4000);
		int bright = 0;
		for (int y = 0; y < s.height; ++y)
			bright += s.row(y)[1 + 2 * 960] > 128;
		CHECK(bright > 0 && bright < s.height);  // neither blank nor solid
	}
}

TEST(pgm_round_trip_and_errors)
{
	otc::PgmImage img{1920, 3, std::vector<uint8_t>(1920 * 3, 200)};
	CHECK(otc::writePgm("t.pgm", img));
	otc::PgmImage back;
	std::string err;
	CHECK(otc::readPgm("t.pgm", back, err) && back.width == 1920 && back.height == 3 && back.pixels[5] == 200);

	writeFile("comment.pgm", std::string("P5\n# made by a test\n2 1\n255\n") + "\x10\x20");
	CHECK(otc::readPgm("comment.pgm", back, err) && back.width == 2 && back.pixels[1] == 0x20);

	writeFile("bad.pgm", "P2\n1 1\n255\n0\n");
	err.clear();
	CHECK(!otc::readPgm("bad.pgm", back, err) && !err.empty());
	writeFile("short.pgm", "P5\n1920 10\n255\n" + std::string(100, '\0'));
	err.clear();
	CHECK(!otc::readPgm("short.pgm", back, err) && !err.empty());
	writeFile("deep.pgm", "P5\n1 1\n65535\n\0\0");
	err.clear();
	CHECK(!otc::readPgm("deep.pgm", back, err) && !err.empty());
	err.clear();
	CHECK(!otc::readPgm("missing.pgm", back, err) && !err.empty());
}

TEST(luma_of_frame_extracts_y)
{
	std::vector<uint8_t> frame(4 * 2, 0);  // 2x2 UYVY with rowBytes 4
	frame[1] = 16;
	frame[3] = 235;
	frame[5] = 100;
	frame[7] = 50;
	auto img = otc::lumaOf(frame.data(), 2, 2, 4);
	CHECK(img.width == 2 && img.height == 2);
	CHECK(img.pixels[0] == 16 && img.pixels[1] == 235 && img.pixels[2] == 100 && img.pixels[3] == 50);
}

TEST_MAIN()
