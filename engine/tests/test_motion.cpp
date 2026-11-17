#include <algorithm>

#include "check.h"
#include "core/motion.h"

TEST(constant_speed_moves_exactly)
{
	otc::Motion m(0.4);
	m.setLimits(-1e9, 1e9);
	m.setTargetSpeed(100);
	for (int i = 0; i < 100; ++i)
		m.advance(0.02);  // 2 s
	const double p0 = m.position();
	for (int i = 0; i < 50; ++i)
		m.advance(0.02);  // 1 s more
	CHECK_NEAR(m.position() - p0, 100.0, 0.5);
}

TEST(speed_change_eases_without_overshoot)
{
	otc::Motion m(0.4);
	m.setLimits(-1e9, 1e9);
	m.setTargetSpeed(200);
	double prev = 0, maxSpeed = 0, prevPos = m.position();
	bool monotone = true;
	for (int i = 0; i < 100; ++i)
	{
		m.advance(0.02);
		monotone = monotone && m.speed() >= prev - 1e-9;
		prev = m.speed();
		maxSpeed = std::max(maxSpeed, m.speed());
		CHECK(m.position() >= prevPos);
		prevPos = m.position();
		if (i == 0)
			CHECK(m.speed() < 200 * 0.1);  // no jump on the first field
		if (i == 24)
			CHECK(m.speed() > 200 * 0.95);  // ~95% at 0.4 s, safely past it by 0.5 s
	}
	CHECK(monotone);
	CHECK(maxSpeed <= 200 + 1e-6);
	CHECK(m.targetSpeed() == 200);
}

TEST(zero_speed_does_not_drift)
{
	otc::Motion m;
	m.setLimits(-1e9, 1e9);
	m.jumpTo(123.25);
	for (int i = 0; i < 1000; ++i)
		m.advance(0.02);
	CHECK(m.position() == 123.25);
}

TEST(slow_speed_advances_evenly)
{
	// 2 px/s once settled: each 20 ms field moves 0.04 px
	otc::Motion m;
	m.setLimits(-1e9, 1e9);
	m.setTargetSpeed(2);
	for (int i = 0; i < 100; ++i)
		m.advance(0.02);
	for (int i = 0; i < 50; ++i)
	{
		const double p = m.position();
		m.advance(0.02);
		CHECK_NEAR(m.position() - p, 0.04, 1e-4);
	}
}

TEST(stops_at_limits_and_can_reverse)
{
	otc::Motion m;
	m.setLimits(0, 500);
	m.jumpTo(490);
	m.setTargetSpeed(300);
	for (int i = 0; i < 100; ++i)
		m.advance(0.02);
	CHECK(m.position() == 500);
	CHECK(m.speed() == 0);
	m.setTargetSpeed(-300);
	for (int i = 0; i < 25; ++i)
		m.advance(0.02);
	CHECK(m.position() < 500);
	m.jumpTo(-50);
	CHECK(m.position() == 0);  // jumpTo clamps too
}

TEST_MAIN()
