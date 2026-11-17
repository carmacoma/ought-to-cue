// 8-bit binary PGM (P5) read/write: strips from files, frame dumps for tests and the headless sink.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace otc
{
struct PgmImage
{
	int width = 0, height = 0;
	std::vector<uint8_t> pixels;
};

bool readPgm(const std::string& path, PgmImage& out, std::string& error);  // P5, maxval 255 only
bool writePgm(const std::string& path, const PgmImage& img);
PgmImage lumaOf(const uint8_t* uyvyFrame, int width, int height, int rowBytes);
}  // namespace otc
