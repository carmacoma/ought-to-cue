#include "core/motion.h"

#include <algorithm>
#include <cmath>

namespace otc
{
// A critically damped step response reaches 95% at omega * t ~= 4.74.
Motion::Motion(double settleSeconds) : omega_(4.74 / settleSeconds) {}

void Motion::setTargetSpeed(double pxPerSecond) { target_ = pxPerSecond; }

void Motion::jumpTo(double y)
{
	pos_ = std::clamp(y, minY_, maxY_);
	speed_ = 0;
	accel_ = 0;
}

void Motion::setLimits(double minY, double maxY)
{
	minY_ = minY;
	maxY_ = std::max(minY, maxY);
	pos_ = std::clamp(pos_, minY_, maxY_);
}

void Motion::advance(double dtSeconds)
{
	if (dtSeconds <= 0)
		return;
	// 1 ms sub-steps keep the explicit integration stable and free of overshoot at 20-40 ms fields.
	const int steps = std::max(1, static_cast<int>(std::ceil(dtSeconds / 0.001)));
	const double h = dtSeconds / steps;
	for (int i = 0; i < steps; ++i)
	{
		accel_ += h * (omega_ * omega_ * (target_ - speed_) - 2 * omega_ * accel_);
		speed_ += h * accel_;
		// Snap once settled, so zero is exactly zero and a constant speed is exact.
		if (std::abs(target_ - speed_) < 1e-6 && std::abs(accel_) < 1e-6)
		{
			speed_ = target_;
			accel_ = 0;
		}
		pos_ += h * speed_;
		// At a limit, stop; and drop a target that still pushes outward, so one step back moves the text.
		if (pos_ <= minY_)
		{
			pos_ = minY_;
			if (speed_ < 0)
				speed_ = accel_ = 0;
			if (target_ < 0)
				target_ = 0;
		}
		else if (pos_ >= maxY_)
		{
			pos_ = maxY_;
			if (speed_ > 0)
				speed_ = accel_ = 0;
			if (target_ > 0)
				target_ = 0;
		}
	}
}
}  // namespace otc
