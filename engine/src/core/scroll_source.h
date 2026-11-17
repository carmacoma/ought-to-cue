// Renders successive output frames of the scrolling strip.
//
// Time is counted in frames rendered: each call to renderNextFrame is one frame on the card, so
// motion follows the card's clock, never the system clock. Interlaced modes take one motion step
// per field. The command methods may be called from any thread; they take effect on the next frame.
#pragma once

#include <cstdint>
#include <mutex>

#include "core/compositor.h"
#include "core/motion.h"
#include "core/strip.h"
#include "core/video_mode.h"

namespace otc
{
class ScrollSource
{
public:
	ScrollSource(const VideoMode& mode, Strip strip, Look look, Interp interp);

	// Interlaced: the first field's rows (even rows when upperFieldFirst) at the current position,
	// advance one field, the other field's rows, advance one field. Progressive: all rows, advance one
	// frame. Then the marker, then (if on) the loopback frame counter on rows 480-559.
	void renderNextFrame(uint8_t* frame, int rowBytes);

	void setTargetSpeed(double pxPerSecond);
	void jumpTo(double y);
	void setInterp(Interp i);
	void setShowMarker(bool on);
	void setCounter(bool on);

	uint64_t framesRendered() const;
	double position() const;  // top row of the next frame
	double speed() const;
	double targetSpeed() const;
	Interp interp() const;
	bool showMarker() const;
	bool counter() const;

private:
	const VideoMode mode_;
	const Strip strip_;
	mutable std::mutex mu_;  // guards everything below
	Look look_;
	Interp interp_;
	Motion motion_;
	bool counter_ = false;
	uint64_t frames_ = 0;
};
}  // namespace otc
