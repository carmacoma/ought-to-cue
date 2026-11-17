#include "core/scroll_source.h"

#include "core/counter_pattern.h"

namespace otc
{
ScrollSource::ScrollSource(const VideoMode& mode, Strip strip, Look look, Interp interp)
    : mode_(mode), strip_(std::move(strip)), look_(look), interp_(interp)
{
	// The first line can sit at the reading line, and the last line can reach it.
	motion_.setLimits(-look_.markerY, strip_.height - look_.markerY);
	motion_.jumpTo(-look_.markerY);
}

void ScrollSource::renderNextFrame(uint8_t* frame, int rowBytes)
{
	double first = 0, second = 0;
	Look look;
	Interp interp;
	bool counter;
	uint64_t index;
	{
		std::lock_guard<std::mutex> lock(mu_);
		first = motion_.position();
		motion_.advance(mode_.fieldSeconds());
		if (mode_.interlaced)
		{
			second = motion_.position();
			motion_.advance(mode_.fieldSeconds());
		}
		look = look_;
		interp = interp_;
		counter = counter_;
		index = frames_++;
	}
	if (mode_.interlaced)
	{
		const int firstParity = mode_.upperFieldFirst ? 0 : 1;
		composeRows(strip_, first, firstParity, interp, frame, rowBytes, mode_.height);
		composeRows(strip_, second, 1 - firstParity, interp, frame, rowBytes, mode_.height);
	}
	else
	{
		composeRows(strip_, first, -1, interp, frame, rowBytes, mode_.height);
	}
	drawMarker(look, frame, rowBytes);
	if (counter)
		pattern::encode(frame, rowBytes, static_cast<uint32_t>(index));
}

void ScrollSource::setTargetSpeed(double pxPerSecond)
{
	std::lock_guard<std::mutex> lock(mu_);
	motion_.setTargetSpeed(pxPerSecond);
}
void ScrollSource::jumpTo(double y)
{
	std::lock_guard<std::mutex> lock(mu_);
	motion_.jumpTo(y);
}
void ScrollSource::setInterp(Interp i)
{
	std::lock_guard<std::mutex> lock(mu_);
	interp_ = i;
}
void ScrollSource::setShowMarker(bool on)
{
	std::lock_guard<std::mutex> lock(mu_);
	look_.showMarker = on;
}
void ScrollSource::setCounter(bool on)
{
	std::lock_guard<std::mutex> lock(mu_);
	counter_ = on;
}
uint64_t ScrollSource::framesRendered() const
{
	std::lock_guard<std::mutex> lock(mu_);
	return frames_;
}
double ScrollSource::position() const
{
	std::lock_guard<std::mutex> lock(mu_);
	return motion_.position();
}
double ScrollSource::speed() const
{
	std::lock_guard<std::mutex> lock(mu_);
	return motion_.speed();
}
double ScrollSource::targetSpeed() const
{
	std::lock_guard<std::mutex> lock(mu_);
	return motion_.targetSpeed();
}
Interp ScrollSource::interp() const
{
	std::lock_guard<std::mutex> lock(mu_);
	return interp_;
}
bool ScrollSource::showMarker() const
{
	std::lock_guard<std::mutex> lock(mu_);
	return look_.showMarker;
}
bool ScrollSource::counter() const
{
	std::lock_guard<std::mutex> lock(mu_);
	return counter_;
}
}  // namespace otc
