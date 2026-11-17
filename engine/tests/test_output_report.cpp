#include <string>

#include "check.h"
#include "sinks/output_report.h"

// summarize_run.sh (carmacoma/decklinkExperiments) reads these lines: "<date> <time> playback started ...",
// report lines containing "late=N dropped=N", and "FINAL <date> <time> ... late=0 dropped=0 ... outstanding=0".
TEST(final_line_matches_summarize_run_format)
{
	otc::OutputCounts c{100, 95, 0, 0, 5};
	CHECK(otc::finalReportLine(c, "2026-11-18 10:00:00") ==
	      "FINAL 2026-11-18 10:00:00 scheduled=100 ok=95 late=0 dropped=0 flushed=5 outstanding=0");
	otc::OutputCounts open{100, 90, 1, 2, 3};
	CHECK(otc::finalReportLine(open, "2026-11-18 10:00:00") ==
	      "FINAL 2026-11-18 10:00:00 scheduled=100 ok=90 late=1 dropped=2 flushed=3 outstanding=4");
}

TEST(periodic_line_has_counts)
{
	otc::OutputCounts c{50, 40, 0, 0, 0};
	CHECK(otc::periodicReportLine(c, 6, "2026-11-18 10:00:10") ==
	      "2026-11-18 10:00:10 scheduled=50 ok=40 late=0 dropped=0 flushed=0 buffered=6");
}

TEST(started_line_is_timestamped)
{
	CHECK(otc::startedLine(4, "2026-11-18 10:00:00") == "2026-11-18 10:00:00 playback started after 4 pre-roll frames");
}

TEST_MAIN()
