#include "core/pgm.h"

#include <cctype>
#include <cstdio>
#include <memory>

namespace otc
{
namespace
{
struct FileCloser
{
	void operator()(FILE* f) const { std::fclose(f); }
};
using File = std::unique_ptr<FILE, FileCloser>;

// Reads the next whitespace-separated header token, skipping '#' comments.
bool headerToken(FILE* f, std::string& tok)
{
	tok.clear();
	int c;
	while ((c = std::fgetc(f)) != EOF)
	{
		if (c == '#')
		{
			while ((c = std::fgetc(f)) != EOF && c != '\n')
			{
			}
			continue;
		}
		if (!std::isspace(c))
			break;
	}
	while (c != EOF && !std::isspace(c))
	{
		tok.push_back(static_cast<char>(c));
		c = std::fgetc(f);
	}
	return !tok.empty();  // the single whitespace after the last token has been consumed
}

bool toInt(const std::string& s, int& v)
{
	if (s.empty() || s.size() > 9)
		return false;
	v = 0;
	for (char ch : s)
	{
		if (!std::isdigit(static_cast<unsigned char>(ch)))
			return false;
		v = v * 10 + (ch - '0');
	}
	return true;
}
}  // namespace

bool readPgm(const std::string& path, PgmImage& out, std::string& error)
{
	File f(std::fopen(path.c_str(), "rb"));
	if (!f)
	{
		error = "cannot open " + path;
		return false;
	}
	std::string magic, ws, hs, ms;
	int w = 0, h = 0, maxval = 0;
	if (!headerToken(f.get(), magic) || magic != "P5")
	{
		error = path + ": not a binary PGM (P5) file";
		return false;
	}
	if (!headerToken(f.get(), ws) || !headerToken(f.get(), hs) || !headerToken(f.get(), ms) || !toInt(ws, w) ||
	    !toInt(hs, h) || !toInt(ms, maxval) || w <= 0 || h <= 0)
	{
		error = path + ": bad PGM header";
		return false;
	}
	if (maxval != 255)
	{
		error = path + ": only 8-bit PGM (maxval 255) is supported";
		return false;
	}
	// Check the data is really there before allocating, so a corrupt header can't ask for gigabytes.
	const long dataStart = std::ftell(f.get());
	if (dataStart < 0 || std::fseek(f.get(), 0, SEEK_END) != 0)
	{
		error = path + ": cannot read file";
		return false;
	}
	const long fileEnd = std::ftell(f.get());
	const unsigned long long need = static_cast<unsigned long long>(w) * static_cast<unsigned long long>(h);
	if (fileEnd < dataStart || static_cast<unsigned long long>(fileEnd - dataStart) < need)
	{
		error = path + ": file is shorter than its header says (truncated?)";
		return false;
	}
	std::fseek(f.get(), dataStart, SEEK_SET);
	std::vector<uint8_t> px(static_cast<size_t>(need));
	if (std::fread(px.data(), 1, px.size(), f.get()) != px.size())
	{
		error = path + ": file is shorter than its header says (truncated?)";
		return false;
	}
	out.width = w;
	out.height = h;
	out.pixels = std::move(px);
	return true;
}

bool writePgm(const std::string& path, const PgmImage& img)
{
	File f(std::fopen(path.c_str(), "wb"));
	if (!f)
		return false;
	std::fprintf(f.get(), "P5\n%d %d\n255\n", img.width, img.height);
	return std::fwrite(img.pixels.data(), 1, img.pixels.size(), f.get()) == img.pixels.size();
}

PgmImage lumaOf(const uint8_t* uyvyFrame, int width, int height, int rowBytes)
{
	PgmImage img{width, height, std::vector<uint8_t>(static_cast<size_t>(width) * height)};
	for (int y = 0; y < height; ++y)
	{
		const uint8_t* row = uyvyFrame + static_cast<size_t>(y) * rowBytes;
		for (int x = 0; x < width; ++x)
			img.pixels[static_cast<size_t>(y) * width + x] = row[2 * x + 1];
	}
	return img;
}
}  // namespace otc
