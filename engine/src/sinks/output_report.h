// Log lines for an output run, in the format carmacoma/decklinkExperiments' summarize_run.sh reads,
// so a scroll-test loopback run can be checked with the same tool as the spike.
#pragma once

#include <cstdint>
#include <string>

namespace otc
{
struct OutputCounts
{
	uint64_t scheduled = 0, completed = 0, late = 0, dropped = 0, flushed = 0;
};

std::string localStamp();  // "YYYY-MM-DD HH:MM:SS", local time
std::string startedLine(int prerollFrames, const std::string& stamp);
std::string periodicReportLine(const OutputCounts& c, uint32_t buffered, const std::string& stamp);
std::string finalReportLine(const OutputCounts& c, const std::string& stamp);
}  // namespace otc
