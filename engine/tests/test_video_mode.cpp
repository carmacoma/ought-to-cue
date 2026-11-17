#include "check.h"
#include "core/video_mode.h"

TEST(modes_have_expected_timing)
{
	auto* i50 = otc::findMode("1080i50");
	CHECK(i50 && i50->interlaced && i50->upperFieldFirst);
	CHECK(i50->fieldsPerFrame() == 2);
	CHECK_NEAR(i50->fieldSeconds(), 0.020, 1e-9);
	auto* p50 = otc::findMode("1080p50");
	CHECK(p50 && !p50->interlaced && p50->fieldsPerFrame() == 1);
	CHECK_NEAR(p50->fieldSeconds(), 0.020, 1e-9);
	CHECK_NEAR(otc::findMode("1080p25")->fieldSeconds(), 0.040, 1e-9);
	CHECK_NEAR(otc::findMode("1080i5994")->fieldSeconds(), 1001.0 / 60000.0, 1e-12);
	CHECK(otc::findMode("720p50") == nullptr);
	CHECK(otc::allModes().size() == 4);
}

TEST_MAIN()
