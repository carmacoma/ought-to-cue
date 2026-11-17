#include "core/video_mode.h"

namespace otc
{
const std::vector<VideoMode>& allModes()
{
	// name, width, height, interlaced, upperFieldFirst, frameDuration, timescale
	static const std::vector<VideoMode> modes = {
	    {"1080i50", 1920, 1080, true, true, 1000, 25000},
	    {"1080p25", 1920, 1080, false, false, 1000, 25000},
	    {"1080p50", 1920, 1080, false, false, 1000, 50000},
	    {"1080i5994", 1920, 1080, true, true, 1001, 30000},
	};
	return modes;
}

const VideoMode* findMode(std::string_view name)
{
	for (const auto& m : allModes())
		if (m.name == name)
			return &m;
	return nullptr;
}
}  // namespace otc
