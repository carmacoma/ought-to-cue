// ought-scrolltest: scrolls a test pattern or a text strip, to judge scroll smoothness on a real
// DeckLink card (or headless, dumping frames, where there is no card). See docs/SCROLL-TEST.md.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "core/pgm.h"
#include "core/scroll_source.h"
#include "core/strip.h"
#include "core/video_mode.h"
#include "sinks/headless_sink.h"

#ifdef OTC_HAVE_DECKLINK
#include "apps/scrolltest_decklink.h"
#endif

namespace
{
constexpr int kPatternHeight = 40000;

struct Options
{
#ifdef OTC_HAVE_DECKLINK
	std::string sink = "decklink";
#else
	std::string sink = "headless";
#endif
	bool list = false;
	int device = 0;
	std::string mode = "1080i50";
	std::string pattern = "text";
	std::string stripPath;
	double speed = 0;
	std::string interp = "linear";
	int buffer = 4;
	bool counter = false;
	double seconds = 0;
	int frames = 100;
	std::string dumpDir;
	int dumpEvery = 0;
};

void usage(FILE* out)
{
	std::fprintf(out,
	             "usage: ought-scrolltest [options]\n"
	             "  --sink headless|decklink   output (default: decklink if built with the SDK, else headless)\n"
	             "  --list                     list DeckLink devices\n"
	             "  --device N                 DeckLink device index (default 0)\n"
	             "  --mode 1080i50|1080p25|1080p50|1080i5994   (default 1080i50)\n"
	             "  --pattern text|bars|grid   test pattern (default text)\n"
	             "  --strip FILE.pgm           a 1920-wide 8-bit PGM strip instead of a pattern\n"
	             "  --speed PX                 starting speed in px/s (default 0)\n"
	             "  --interp linear|wholeline  slow-motion method (default linear)\n"
	             "  --buffer N                 frames kept queued on the card (default 4)\n"
	             "  --counter                  overlay the loopback frame counter (rows 480-559)\n"
	             "  --seconds S                stop after S seconds (default: until q / Ctrl-C)\n"
	             "  --frames N --dump DIR --dump-every K   headless only\n");
}

bool toInt(const char* s, int& v)
{
	char* end = nullptr;
	const long x = std::strtol(s, &end, 10);
	if (!end || *end || end == s)
		return false;
	v = static_cast<int>(x);
	return true;
}

bool toDouble(const char* s, double& v)
{
	char* end = nullptr;
	v = std::strtod(s, &end);
	return end && !*end && end != s;
}

// Returns 0 when parsed, 2 on bad usage, 1 for --help.
int parse(int argc, char** argv, Options& o)
{
	for (int i = 1; i < argc; ++i)
	{
		const std::string a = argv[i];
		auto value = [&](const char*& out) {
			if (i + 1 >= argc)
				return false;
			out = argv[++i];
			return true;
		};
		const char* v = nullptr;
		bool ok = true;
		if (a == "--help" || a == "-h")
		{
			usage(stdout);
			return 1;
		}
		else if (a == "--list")
			o.list = true;
		else if (a == "--counter")
			o.counter = true;
		else if (a == "--sink")
			ok = value(v) && (o.sink = v, o.sink == "headless" || o.sink == "decklink");
		else if (a == "--device")
			ok = value(v) && toInt(v, o.device) && o.device >= 0;
		else if (a == "--mode")
			ok = value(v) && (o.mode = v, true);
		else if (a == "--pattern")
			ok = value(v) && (o.pattern = v, o.pattern == "text" || o.pattern == "bars" || o.pattern == "grid");
		else if (a == "--strip")
			ok = value(v) && (o.stripPath = v, true);
		else if (a == "--speed")
			ok = value(v) && toDouble(v, o.speed);
		else if (a == "--interp")
			ok = value(v) && (o.interp = v, o.interp == "linear" || o.interp == "wholeline");
		else if (a == "--buffer")
			ok = value(v) && toInt(v, o.buffer) && o.buffer >= 1 && o.buffer <= 60;
		else if (a == "--seconds")
			ok = value(v) && toDouble(v, o.seconds) && o.seconds >= 0;
		else if (a == "--frames")
			ok = value(v) && toInt(v, o.frames) && o.frames >= 0;
		else if (a == "--dump")
			ok = value(v) && (o.dumpDir = v, true);
		else if (a == "--dump-every")
			ok = value(v) && toInt(v, o.dumpEvery) && o.dumpEvery >= 0;
		else
		{
			std::fprintf(stderr, "unknown option: %s\n", a.c_str());
			usage(stderr);
			return 2;
		}
		if (!ok)
		{
			std::fprintf(stderr, "bad or missing value for %s\n", a.c_str());
			return 2;
		}
	}
	return 0;
}

bool loadStrip(const Options& o, otc::Strip& strip)
{
	if (o.stripPath.empty())
	{
		strip = o.pattern == "bars"   ? otc::makeBarsStrip(kPatternHeight)
		        : o.pattern == "grid" ? otc::makeGridStrip(kPatternHeight)
		                              : otc::makePseudoTextStrip(kPatternHeight);
		return true;
	}
	otc::PgmImage img;
	std::string err;
	if (!otc::readPgm(o.stripPath, img, err))
	{
		std::fprintf(stderr, "%s\n", err.c_str());
		return false;
	}
	if (img.width != 1920)
	{
		std::fprintf(stderr, "%s: strip must be 1920 px wide (it is %d)\n", o.stripPath.c_str(), img.width);
		return false;
	}
	strip = otc::stripFromGrey(img.width, img.height, img.pixels.data());
	return true;
}
}  // namespace

int main(int argc, char** argv)
{
	Options o;
	const int parsed = parse(argc, argv, o);
	if (parsed == 1)
		return 0;
	if (parsed != 0)
		return 2;

#ifdef OTC_HAVE_DECKLINK
	if (o.list)
		return otc::listDeckLinkDevices() == 0 ? 0 : 1;
#else
	if (o.list || o.sink == "decklink")
	{
		std::fprintf(stderr, "this build has no DeckLink output (configure with -DOTC_DECKLINK_SDK=<path>)\n");
		return 2;
	}
#endif

	const otc::VideoMode* mode = otc::findMode(o.mode);
	if (!mode)
	{
		std::fprintf(stderr, "unknown mode: %s (use 1080i50, 1080p25, 1080p50 or 1080i5994)\n", o.mode.c_str());
		return 2;
	}
	otc::Strip strip;
	if (!loadStrip(o, strip))
		return 2;

	otc::ScrollSource src(*mode, std::move(strip), otc::Look{},
	                      o.interp == "wholeline" ? otc::Interp::WholeLine : otc::Interp::Linear);
	src.setTargetSpeed(o.speed);
	src.setCounter(o.counter);

	if (o.sink == "headless")
		return otc::runHeadless(src, {o.frames, o.dumpDir, o.dumpEvery});

#ifdef OTC_HAVE_DECKLINK
	return otc::runDeckLinkScrollTest(src, *mode, o.device, o.buffer, o.seconds);
#else
	return 2;
#endif
}
