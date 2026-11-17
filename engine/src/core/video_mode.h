// Output video modes the engine supports, and their timing.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace otc
{
struct VideoMode
{
	std::string name;  // "1080i50"
	int width = 1920, height = 1080;
	bool interlaced = false;
	bool upperFieldFirst = false;  // true for interlaced HD
	int64_t frameDuration = 0;     // in timescale units
	int64_t timescale = 0;

	int fieldsPerFrame() const { return interlaced ? 2 : 1; }
	// Time between motion steps: one field (interlaced) or one frame (progressive).
	double fieldSeconds() const
	{
		return static_cast<double>(frameDuration) / static_cast<double>(timescale) / fieldsPerFrame();
	}
};

const VideoMode* findMode(std::string_view name);  // nullptr if unknown
const std::vector<VideoMode>& allModes();
}  // namespace otc
