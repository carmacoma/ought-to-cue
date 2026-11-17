// DeckLink output: scheduled playback on one output, timed by the card's own clock.
// Ported from carmacoma/decklinkExperiments loopback/cpp/dl_output_spike.cpp (Brendan's own code).
// Built only when the SDK headers are given (-DOTC_DECKLINK_SDK).
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "core/scroll_source.h"
#include "core/video_mode.h"

namespace otc
{
struct DeckLinkStats
{
	uint64_t scheduled = 0, completed = 0, late = 0, dropped = 0, flushed = 0;
	uint32_t buffered = 0;
};

// Prints index, name, profile, duplex and which of our modes each device supports. 0 if any device was found.
int listDeckLinkDevices();

class DeckLinkSink
{
public:
	DeckLinkSink();
	~DeckLinkSink();  // stops if still running
	DeckLinkSink(const DeckLinkSink&) = delete;
	DeckLinkSink& operator=(const DeckLinkSink&) = delete;

	// Checks the device exists and supports the mode with 8-bit YUV.
	bool open(int deviceIndex, const VideoMode& mode, std::string& error);
	// Pre-rolls bufferFrames frames from src, starts playback, and keeps bufferFrames queued:
	// each completed frame is refilled with src.renderNextFrame. src must outlive stop().
	bool start(ScrollSource& src, int bufferFrames, std::string& error);
	// StopScheduledPlayback -> wait for ScheduledPlaybackHasStopped -> wait for flush completions ->
	// DisableVideoOutput (the spike's stop sequence).
	void stop();
	DeckLinkStats stats() const;
	bool playing() const;  // true once pre-roll finished and scheduled playback started
	std::string deviceName() const;

	struct Impl;  // public only so the SDK callback class in the .cpp can name it

private:
	std::unique_ptr<Impl> impl_;
};
}  // namespace otc
