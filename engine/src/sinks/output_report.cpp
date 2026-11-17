#include "sinks/output_report.h"

#include <cstdio>
#include <ctime>

namespace otc
{
namespace
{
std::string counts(const OutputCounts& c)
{
	char buf[160];
	std::snprintf(buf, sizeof buf, "scheduled=%llu ok=%llu late=%llu dropped=%llu flushed=%llu",
	              static_cast<unsigned long long>(c.scheduled), static_cast<unsigned long long>(c.completed),
	              static_cast<unsigned long long>(c.late), static_cast<unsigned long long>(c.dropped),
	              static_cast<unsigned long long>(c.flushed));
	return buf;
}
}  // namespace

std::string localStamp()
{
	char buf[32];
	const std::time_t t = std::time(nullptr);
	std::tm tmv{};
	localtime_r(&t, &tmv);
	std::strftime(buf, sizeof buf, "%F %T", &tmv);
	return buf;
}

std::string startedLine(int prerollFrames, const std::string& stamp)
{
	return stamp + " playback started after " + std::to_string(prerollFrames) + " pre-roll frames";
}

std::string periodicReportLine(const OutputCounts& c, uint32_t buffered, const std::string& stamp)
{
	return stamp + " " + counts(c) + " buffered=" + std::to_string(buffered);
}

std::string finalReportLine(const OutputCounts& c, const std::string& stamp)
{
	const uint64_t done = c.completed + c.late + c.dropped + c.flushed;
	const uint64_t outstanding = c.scheduled > done ? c.scheduled - done : 0;
	return "FINAL " + stamp + " " + counts(c) + " outstanding=" + std::to_string(outstanding);
}
}  // namespace otc
