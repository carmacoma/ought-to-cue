#include "sinks/decklink/decklink_sink.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "DeckLinkAPI.h"

namespace otc
{
namespace
{
using Clock = std::chrono::steady_clock;

template <typename T>
struct Ref
{
	T* p = nullptr;
	Ref() = default;
	explicit Ref(T* x) : p(x) {}
	~Ref() { reset(); }
	Ref(const Ref&) = delete;
	Ref& operator=(const Ref&) = delete;
	Ref(Ref&& o) noexcept : p(o.p) { o.p = nullptr; }
	Ref& operator=(Ref&& o) noexcept
	{
		if (this != &o)
		{
			reset();
			p = o.p;
			o.p = nullptr;
		}
		return *this;
	}
	void reset(T* x = nullptr)
	{
		if (p)
			p->Release();
		p = x;
	}
	T** out()
	{
		reset();
		return &p;
	}
	T* operator->() const { return p; }
	T* get() const { return p; }
	explicit operator bool() const { return p != nullptr; }
};

template <typename U, typename T>
Ref<U> queryInterface(T* obj, REFIID iid)
{
	Ref<U> r;
	if (obj && obj->QueryInterface(iid, reinterpret_cast<void**>(r.out())) != S_OK)
		r.reset();
	return r;
}

// On Linux REFIID is a plain struct with no operator==.
bool sameIID(const REFIID& a, const REFIID& b) { return std::memcmp(&a, &b, sizeof(REFIID)) == 0; }

std::string fourcc(uint32_t v)
{
	char c[5] = {static_cast<char>(v >> 24), static_cast<char>(v >> 16), static_cast<char>(v >> 8), static_cast<char>(v), 0};
	for (int i = 0; i < 4; ++i)
		if (c[i] < 32 || c[i] > 126)
			c[i] = '?';
	return c;
}

std::string nameOf(IDeckLink* dl)
{
	const char* name = nullptr;
	std::string s = "(unnamed)";
	if (dl->GetDisplayName(&name) == S_OK && name)
	{
		s = name;
		std::free(const_cast<char*>(name));
	}
	return s;
}

std::vector<Ref<IDeckLink>> enumerateDevices()
{
	std::vector<Ref<IDeckLink>> devices;
	Ref<IDeckLinkIterator> it(CreateDeckLinkIteratorInstance());
	if (!it)
	{
		std::fprintf(stderr, "DeckLink driver not found (is Desktop Video installed and loaded?)\n");
		return devices;
	}
	IDeckLink* dl = nullptr;
	while (it->Next(&dl) == S_OK)
		devices.emplace_back(dl);
	return devices;
}

bool bmdModeFor(const std::string& name, BMDDisplayMode& out)
{
	if (name == "1080i50")
		out = bmdModeHD1080i50;
	else if (name == "1080p25")
		out = bmdModeHD1080p25;
	else if (name == "1080p50")
		out = bmdModeHD1080p50;
	else if (name == "1080i5994")
		out = bmdModeHD1080i5994;
	else
		return false;
	return true;
}

const char* duplexName(int64_t d)
{
	switch (d)
	{
		case bmdDuplexFull: return "full";
		case bmdDuplexHalf: return "half";
		case bmdDuplexInactive: return "inactive";
		default: return "unknown";
	}
}

bool supportsMode(IDeckLinkOutput* output, BMDDisplayMode mode)
{
	bool supported = false;
	return output &&
	       output->DoesSupportVideoMode(bmdVideoConnectionUnspecified, mode, bmdFormat8BitYUV, bmdNoVideoOutputConversion,
	                                    bmdSupportedVideoModeDefault, nullptr, &supported) == S_OK &&
	       supported;
}
}  // namespace

int listDeckLinkDevices()
{
	auto devices = enumerateDevices();
	if (devices.empty())
		return 1;
	std::printf("%-5s %-28s %-8s %-8s %s\n", "index", "name", "profile", "duplex", "modes (8-bit YUV output)");
	for (size_t i = 0; i < devices.size(); ++i)
	{
		IDeckLink* dl = devices[i].get();
		auto attrs = queryInterface<IDeckLinkProfileAttributes>(dl, IID_IDeckLinkProfileAttributes);
		auto output = queryInterface<IDeckLinkOutput>(dl, IID_IDeckLinkOutput);
		int64_t profile = 0, duplex = 0;
		if (attrs)
		{
			attrs->GetInt(BMDDeckLinkProfileID, &profile);
			attrs->GetInt(BMDDeckLinkDuplex, &duplex);
		}
		std::string modes;
		for (const auto& m : allModes())
		{
			BMDDisplayMode bm;
			if (bmdModeFor(m.name, bm) && supportsMode(output.get(), bm))
				modes += (modes.empty() ? "" : " ") + m.name;
		}
		std::printf("%-5zu %-28s %-8s %-8s %s\n", i, nameOf(dl).c_str(), fourcc(static_cast<uint32_t>(profile)).c_str(),
		            duplexName(duplex), output ? (modes.empty() ? "(none)" : modes.c_str()) : "(no output)");
	}
	return 0;
}

class OutputCallback;

struct DeckLinkSink::Impl
{
	Ref<IDeckLink> device;
	Ref<IDeckLinkOutput> output;
	OutputCallback* callback = nullptr;  // COM reference held while output is enabled
	BMDDisplayMode bmdMode = 0;
	VideoMode mode;
	ScrollSource* src = nullptr;
	int bufferFrames = 4;

	std::vector<IDeckLinkMutableVideoFrame*> pool;  // owns one reference each
	std::mutex mtx;
	std::condition_variable cv;
	std::vector<IDeckLinkVideoFrame*> freeFrames;
	bool playbackStopped = false;
	std::atomic<bool> stopping{false};
	std::atomic<bool> playing{false};
	std::atomic<bool> enabled{false};
	std::thread worker;
	std::atomic<uint64_t> scheduled{0}, completed{0}, late{0}, dropped{0}, flushed{0};
	uint64_t next = 0;

	bool renderAndSchedule(IDeckLinkVideoFrame* frame)
	{
		auto buf = queryInterface<IDeckLinkVideoBuffer>(frame, IID_IDeckLinkVideoBuffer);
		if (!buf || buf->StartAccess(bmdBufferAccessWrite) != S_OK)
		{
			std::fprintf(stderr, "could not access frame buffer\n");
			return false;
		}
		void* bytes = nullptr;
		buf->GetBytes(&bytes);
		src->renderNextFrame(static_cast<uint8_t*>(bytes), static_cast<int>(frame->GetRowBytes()));
		buf->EndAccess(bmdBufferAccessWrite);
		// Times come from the frame count and the mode's frame duration: the card's clock, not the system's.
		const HRESULT hr = output->ScheduleVideoFrame(frame, static_cast<BMDTimeValue>(next) * mode.frameDuration,
		                                              mode.frameDuration, mode.timescale);
		if (hr != S_OK)
		{
			std::fprintf(stderr, "ScheduleVideoFrame failed: 0x%08x\n", static_cast<unsigned>(hr));
			return false;
		}
		++next;
		++scheduled;
		return true;
	}

	void workerLoop()
	{
		int prerolled = 0;
		while (!stopping)
		{
			{
				std::unique_lock<std::mutex> lk(mtx);
				cv.wait_for(lk, std::chrono::milliseconds(20), [&] { return stopping.load() || !freeFrames.empty(); });
			}
			while (!stopping)
			{
				uint32_t buffered = 0;
				if (output->GetBufferedVideoFrameCount(&buffered) != S_OK || buffered >= static_cast<uint32_t>(bufferFrames))
					break;
				IDeckLinkVideoFrame* f = nullptr;
				{
					std::lock_guard<std::mutex> lk(mtx);
					if (freeFrames.empty())
						break;
					f = freeFrames.back();
					freeFrames.pop_back();
				}
				if (!renderAndSchedule(f))
				{
					stopping = true;
					break;
				}
				if (!playing && ++prerolled >= bufferFrames)
				{
					if (output->StartScheduledPlayback(0, mode.timescale, 1.0) != S_OK)
					{
						std::fprintf(stderr, "StartScheduledPlayback failed\n");
						stopping = true;
						break;
					}
					playing = true;
				}
			}
		}
	}

	uint64_t outstanding() const
	{
		const uint64_t done = completed + late + dropped + flushed;
		return scheduled > done ? scheduled - done : 0;
	}
};

class OutputCallback : public IDeckLinkVideoOutputCallback
{
public:
	explicit OutputCallback(DeckLinkSink::Impl* s) : s_(s) {}
	HRESULT QueryInterface(REFIID iid, LPVOID* ppv) override
	{
		if (!ppv)
			return E_INVALIDARG;
		if (sameIID(iid, IID_IUnknown) || sameIID(iid, IID_IDeckLinkVideoOutputCallback))
		{
			*ppv = static_cast<IDeckLinkVideoOutputCallback*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	ULONG AddRef() override { return ++refs_; }
	ULONG Release() override
	{
		const ULONG n = --refs_;
		if (n == 0)
			delete this;
		return n;
	}
	HRESULT ScheduledFrameCompleted(IDeckLinkVideoFrame* frame, BMDOutputFrameCompletionResult result) override
	{
		switch (result)
		{
			case bmdOutputFrameCompleted: ++s_->completed; break;
			case bmdOutputFrameDisplayedLate: ++s_->late; break;
			case bmdOutputFrameDropped: ++s_->dropped; break;
			case bmdOutputFrameFlushed: ++s_->flushed; break;
		}
		{
			std::lock_guard<std::mutex> lk(s_->mtx);
			s_->freeFrames.push_back(frame);  // one of our pool frames; the pool owns the reference
		}
		s_->cv.notify_one();
		return S_OK;
	}
	HRESULT ScheduledPlaybackHasStopped() override
	{
		{
			std::lock_guard<std::mutex> lk(s_->mtx);
			s_->playbackStopped = true;
		}
		s_->cv.notify_all();
		return S_OK;
	}

private:
	DeckLinkSink::Impl* s_;
	std::atomic<ULONG> refs_{1};
};

DeckLinkSink::DeckLinkSink() : impl_(std::make_unique<Impl>()) {}
DeckLinkSink::~DeckLinkSink() { stop(); }

bool DeckLinkSink::open(int deviceIndex, const VideoMode& mode, std::string& error)
{
	Impl& s = *impl_;
	if (!bmdModeFor(mode.name, s.bmdMode))
	{
		error = "no DeckLink mode for " + mode.name;
		return false;
	}
	auto devices = enumerateDevices();
	if (devices.empty())
	{
		error = "no DeckLink devices (is Desktop Video installed and loaded?)";
		return false;
	}
	if (deviceIndex < 0 || deviceIndex >= static_cast<int>(devices.size()))
	{
		error = "no DeckLink device " + std::to_string(deviceIndex) + " (use --list)";
		return false;
	}
	s.device = std::move(devices[static_cast<size_t>(deviceIndex)]);
	s.output = queryInterface<IDeckLinkOutput>(s.device.get(), IID_IDeckLinkOutput);
	if (!s.output)
	{
		error = "device " + std::to_string(deviceIndex) + " has no output";
		return false;
	}
	if (!supportsMode(s.output.get(), s.bmdMode))
	{
		error = "mode " + mode.name + " with 8-bit YUV is not supported on this output (check the profile with --list)";
		return false;
	}
	Ref<IDeckLinkDisplayMode> dm;
	if (s.output->GetDisplayMode(s.bmdMode, dm.out()) != S_OK)
	{
		error = "GetDisplayMode failed";
		return false;
	}
	BMDTimeValue duration = 0;
	BMDTimeScale scale = 0;
	dm->GetFrameRate(&duration, &scale);
	if (dm->GetWidth() != mode.width || dm->GetHeight() != mode.height || duration * mode.timescale != mode.frameDuration * scale)
	{
		error = "card reports different geometry or timing for " + mode.name;
		return false;
	}
	s.mode = mode;
	return true;
}

bool DeckLinkSink::start(ScrollSource& src, int bufferFrames, std::string& error)
{
	Impl& s = *impl_;
	if (!s.output)
	{
		error = "not open";
		return false;
	}
	int32_t rowBytes = 0;
	if (s.output->RowBytesForPixelFormat(bmdFormat8BitYUV, s.mode.width, &rowBytes) != S_OK || rowBytes != s.mode.width * 2)
	{
		error = "unexpected row bytes " + std::to_string(rowBytes);
		return false;
	}
	s.src = &src;
	s.bufferFrames = std::max(1, bufferFrames);
	s.callback = new OutputCallback(&s);
	s.output->SetScheduledFrameCompletionCallback(s.callback);
	if (s.output->EnableVideoOutput(s.bmdMode, bmdVideoOutputFlagDefault) != S_OK)
	{
		error = "EnableVideoOutput failed (is the connector in use, e.g. by CasparCG, or set as an input?)";
		s.output->SetScheduledFrameCompletionCallback(nullptr);
		s.callback->Release();
		s.callback = nullptr;
		return false;
	}
	s.enabled = true;
	const int poolSize = s.bufferFrames + 6;
	for (int i = 0; i < poolSize; ++i)
	{
		IDeckLinkMutableVideoFrame* mf = nullptr;
		if (s.output->CreateVideoFrame(s.mode.width, s.mode.height, rowBytes, bmdFormat8BitYUV, bmdFrameFlagDefault, &mf) != S_OK)
		{
			error = "CreateVideoFrame failed";
			stop();
			return false;
		}
		s.pool.push_back(mf);
		s.freeFrames.push_back(mf);
	}
	s.worker = std::thread([&s] { s.workerLoop(); });
	return true;
}

void DeckLinkSink::stop()
{
	Impl& s = *impl_;
	s.stopping = true;
	s.cv.notify_all();
	if (s.worker.joinable())
		s.worker.join();
	if (s.playing)
	{
		s.output->StopScheduledPlayback(0, nullptr, 0);
		std::unique_lock<std::mutex> lk(s.mtx);
		if (!s.cv.wait_for(lk, std::chrono::seconds(5), [&] { return s.playbackStopped; }))
			std::fprintf(stderr, "timed out waiting for ScheduledPlaybackHasStopped\n");
		s.playing = false;
	}
	// Flush completions can arrive after the stop callback: wait until every frame is accounted for.
	const auto deadline = Clock::now() + std::chrono::seconds(3);
	while (s.outstanding() != 0 && Clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	if (s.enabled)
	{
		s.output->DisableVideoOutput();
		s.output->SetScheduledFrameCompletionCallback(nullptr);
		s.enabled = false;
	}
	if (s.callback)
	{
		s.callback->Release();
		s.callback = nullptr;
	}
	for (auto* f : s.pool)
		f->Release();
	s.pool.clear();
	s.freeFrames.clear();
}

DeckLinkStats DeckLinkSink::stats() const
{
	const Impl& s = *impl_;
	DeckLinkStats st;
	st.scheduled = s.scheduled;
	st.completed = s.completed;
	st.late = s.late;
	st.dropped = s.dropped;
	st.flushed = s.flushed;
	if (s.output)
		s.output->GetBufferedVideoFrameCount(&st.buffered);
	return st;
}

bool DeckLinkSink::playing() const { return impl_->playing; }

std::string DeckLinkSink::deviceName() const { return impl_->device ? nameOf(impl_->device.get()) : std::string(); }
}  // namespace otc
