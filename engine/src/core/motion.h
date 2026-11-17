// Scroll motion: an eased speed and the position it moves.
//
// Position is the strip row shown at the top of the frame (negative = blank above the script).
// Speed is in px per second; negative scrolls backwards. Speed follows the target as a critically
// damped spring, so changes ease in and out with no overshoot and no jerk.
#pragma once

#include <limits>

namespace otc
{
class Motion
{
public:
	explicit Motion(double settleSeconds = 0.4);  // time to reach ~95% of a speed change

	void setTargetSpeed(double pxPerSecond);
	void jumpTo(double y);  // instant; speed and acceleration -> 0, target kept; clamped to the limits
	void setLimits(double minY, double maxY);
	void advance(double dtSeconds);

	double position() const { return pos_; }
	double speed() const { return speed_; }
	double targetSpeed() const { return target_; }

private:
	double omega_;
	double target_ = 0, speed_ = 0, accel_ = 0, pos_ = 0;
	double minY_ = -std::numeric_limits<double>::infinity();
	double maxY_ = std::numeric_limits<double>::infinity();
};
}  // namespace otc
