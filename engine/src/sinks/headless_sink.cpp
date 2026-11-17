#include "sinks/headless_sink.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#include "core/pgm.h"

namespace otc
{
int runHeadless(ScrollSource& src, const HeadlessOptions& opt)
{
	constexpr int kWidth = 1920, kHeight = 1080, kRowBytes = kWidth * 2;
	std::vector<uint8_t> frame(static_cast<size_t>(kRowBytes) * kHeight);
	const bool dumping = opt.dumpEvery > 0 && !opt.dumpDir.empty();
	if (dumping)
	{
		std::error_code ec;
		std::filesystem::create_directories(opt.dumpDir, ec);
		if (ec)
		{
			std::fprintf(stderr, "cannot create %s: %s\n", opt.dumpDir.c_str(), ec.message().c_str());
			return 1;
		}
	}
	for (int i = 0; i < opt.frames; ++i)
	{
		src.renderNextFrame(frame.data(), kRowBytes);
		if (dumping && i % opt.dumpEvery == 0)
		{
			char name[32];
			std::snprintf(name, sizeof name, "frame_%06d.pgm", i);
			const std::string path = (std::filesystem::path(opt.dumpDir) / name).string();
			if (!writePgm(path, lumaOf(frame.data(), kWidth, kHeight, kRowBytes)))
			{
				std::fprintf(stderr, "cannot write %s\n", path.c_str());
				return 1;
			}
		}
	}
	return 0;
}
}  // namespace otc
