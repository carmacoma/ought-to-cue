#include "apps/scrolltest_decklink.h"

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>

#include "sinks/decklink/decklink_sink.h"
#include "sinks/output_report.h"

namespace otc
{
namespace
{
std::atomic<bool> gStop{false};
void onSignal(int) { gStop = true; }

// Puts the terminal in raw-ish mode (no line buffering, no echo; Ctrl-C still signals) and restores it.
class RawTerminal
{
public:
	RawTerminal()
	{
		if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &saved_) != 0)
			return;
		termios raw = saved_;
		raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
		raw.c_cc[VMIN] = 0;
		raw.c_cc[VTIME] = 0;
		active_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
	}
	~RawTerminal()
	{
		if (active_)
			tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
	}
	bool active() const { return active_; }

private:
	termios saved_{};
	bool active_ = false;
};

enum class Key
{
	None,
	Up,
	Down,
	PageUp,
	PageDown,
	Home,
	Stop,
	Interp,
	Marker,
	Counter,
	Quit,
	Other
};

// Reads one key press if one is waiting (within timeoutMs), decoding the usual escape sequences.
Key readKey(int timeoutMs)
{
	pollfd pfd{STDIN_FILENO, POLLIN, 0};
	if (poll(&pfd, 1, timeoutMs) <= 0)
		return Key::None;
	unsigned char buf[8] = {};
	const ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
	if (n <= 0)
		return Key::None;
	const std::string k(reinterpret_cast<char*>(buf), static_cast<size_t>(n));
	if (k == "\x1b[A" || k == "\x1bOA")
		return Key::Up;
	if (k == "\x1b[B" || k == "\x1bOB")
		return Key::Down;
	if (k == "\x1b[5~")
		return Key::PageUp;
	if (k == "\x1b[6~")
		return Key::PageDown;
	if (k == "\x1b[H" || k == "\x1bOH" || k == "\x1b[1~" || k == "\x1b[7~")
		return Key::Home;
	switch (buf[0])
	{
		case '0':
		case ' ': return Key::Stop;
		case 'i': return Key::Interp;
		case 'm': return Key::Marker;
		case 'c': return Key::Counter;
		case 'q': return Key::Quit;
		default: return Key::Other;
	}
}

void apply(Key key, ScrollSource& src)
{
	switch (key)
	{
		case Key::Up: src.setTargetSpeed(src.targetSpeed() + 10); break;
		case Key::Down: src.setTargetSpeed(src.targetSpeed() - 10); break;
		case Key::PageUp: src.setTargetSpeed(src.targetSpeed() + 100); break;
		case Key::PageDown: src.setTargetSpeed(src.targetSpeed() - 100); break;
		case Key::Stop: src.setTargetSpeed(0); break;
		case Key::Home:
			src.setTargetSpeed(0);
			src.jumpTo(-1e18);  // clamps to the top: first line at the reading line
			break;
		case Key::Interp:
			src.setInterp(src.interp() == Interp::Linear ? Interp::WholeLine : Interp::Linear);
			break;
		case Key::Marker: src.setShowMarker(!src.showMarker()); break;
		case Key::Counter: src.setCounter(!src.counter()); break;
		default: break;
	}
}

OutputCounts countsOf(const DeckLinkStats& st) { return {st.scheduled, st.completed, st.late, st.dropped, st.flushed}; }

void printStatus(const VideoMode& mode, int buffer, const ScrollSource& src, const DeckLinkStats& st, bool newline)
{
	std::printf("\r%s %s buffer=%d speed=%.0f (target %.0f) px/s pos=%.1f  frames=%llu late=%llu dropped=%llu "
	            "queued=%u%s\x1b[K%s",
	            mode.name.c_str(), src.interp() == Interp::Linear ? "linear" : "wholeline", buffer, src.speed(),
	            src.targetSpeed(), src.position(), static_cast<unsigned long long>(st.completed + st.late),
	            static_cast<unsigned long long>(st.late), static_cast<unsigned long long>(st.dropped), st.buffered,
	            src.counter() ? " counter" : "", newline ? "\n" : "");
	std::fflush(stdout);
}
}  // namespace

int runDeckLinkScrollTest(ScrollSource& src, const VideoMode& mode, int device, int bufferFrames, double seconds)
{
	DeckLinkSink sink;
	std::string error;
	if (!sink.open(device, mode, error))
	{
		std::fprintf(stderr, "%s\n", error.c_str());
		return 1;
	}
	std::printf("Device %d: %s, %s, buffer %d frames\n", device, sink.deviceName().c_str(), mode.name.c_str(),
	            bufferFrames);
	std::signal(SIGINT, onSignal);
	std::signal(SIGTERM, onSignal);
	if (!sink.start(src, bufferFrames, error))
	{
		std::fprintf(stderr, "%s\n", error.c_str());
		return 1;
	}

	RawTerminal term;
	if (term.active())
		std::printf("Keys: Up/Down speed +-10, PgUp/PgDn +-100, 0 or space stop, Home top, i interp, m marker, "
		            "c counter, q quit\n");
	else
		std::printf("stdin is not a terminal: no keyboard control (Ctrl-C to stop)\n");

	// In a terminal: a live status line. Redirected to a file: a report line every 10 s, in the format
	// summarize_run.sh (decklinkExperiments) reads, so loopback runs can be checked with it.
	const bool tty = isatty(STDOUT_FILENO);
	using Clock = std::chrono::steady_clock;
	const auto t0 = Clock::now();
	auto nextStatus = t0;
	bool announced = false;
	while (!gStop)
	{
		if (seconds > 0 && std::chrono::duration<double>(Clock::now() - t0).count() >= seconds)
			break;
		if (sink.failed())
		{
			std::printf("%s", tty ? "\r\x1b[K" : "");
			std::fprintf(stderr, "output stopped on a DeckLink error (see above)\n");
			break;
		}
		const Key key = term.active() ? readKey(50) : (poll(nullptr, 0, 50), Key::None);
		if (key == Key::Quit)
			break;
		apply(key, src);
		if (!announced && sink.playing())
		{
			announced = true;
			std::printf("%s%s\n", tty ? "\r\x1b[K" : "", startedLine(bufferFrames, localStamp()).c_str());
			std::fflush(stdout);
		}
		const bool due = Clock::now() >= nextStatus;
		if (tty && (due || key != Key::None))
			printStatus(mode, bufferFrames, src, sink.stats(), false);
		else if (!tty && due)
		{
			const DeckLinkStats st = sink.stats();
			std::printf("%s speed=%.0f pos=%.1f\n", periodicReportLine(countsOf(st), st.buffered, localStamp()).c_str(),
			            src.speed(), src.position());
			std::fflush(stdout);
		}
		if (due)
			nextStatus += std::chrono::seconds(tty ? 1 : 10);
	}
	sink.stop();
	if (tty)
		printStatus(mode, bufferFrames, src, sink.stats(), true);
	std::printf("%s\n", finalReportLine(countsOf(sink.stats()), localStamp()).c_str());
	if (sink.failed())
		return 1;
	return 0;
}
}  // namespace otc
