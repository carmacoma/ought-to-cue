// Copied unchanged from carmacoma/decklinkExperiments loopback/cpp/pattern.h (Brendan's own code), so
// scroll-test output can be checked with that repo's loopback capture (decklink_capture.py).
// Frame-counter test pattern for 1920x1080 8-bit UYVY ('2vuy') frames.
//
// Byte-for-byte identical to counter_codec.py (the Python sender/capture
// analyser), so frames produced here can be checked by decklink_capture.py:
//   48 blocks of 40 px across rows 480..559: a 32-bit frame number followed by
//   its CRC-16 (XMODEM, same as Python's binascii.crc_hqx(data, 0)), MSB first,
//   XORed with 0xA5A5A5A5A5A5 so blank pictures are rejected.
//   Black block = bit 0 (Y=16), white block = bit 1 (Y=235).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pattern
{
constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
constexpr int kBits = 48;
constexpr int kBlockW = kWidth / kBits;  // 40
constexpr int kStripY0 = 480;
constexpr int kStripY1 = 560;  // exclusive
constexpr uint8_t kYBlack = 16, kYWhite = 235, kYGrey = 126, kChroma = 128;
// XORed into the 48-bit value so a blank (all-black or all-white) picture is not a valid frame.
constexpr uint64_t kMask = 0xA5A5A5A5A5A5ULL;

// CRC-16/XMODEM: poly 0x1021, init 0, no reflection (== binascii.crc_hqx(data, 0)).
inline uint16_t crc16(const uint8_t* data, size_t len)
{
	uint16_t crc = 0;
	for (size_t i = 0; i < len; ++i)
	{
		crc ^= static_cast<uint16_t>(data[i]) << 8;
		for (int b = 0; b < 8; ++b)
			crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
	}
	return crc;
}

// Mid-grey picture with neutral chroma. UYVY byte order: U Y V Y.
inline void initBase(uint8_t* frame, int rowBytes)
{
	for (int y = 0; y < kHeight; ++y)
	{
		uint8_t* row = frame + static_cast<size_t>(y) * rowBytes;
		for (int x = 0; x < kWidth * 2; x += 2)
		{
			row[x] = kChroma;
			row[x + 1] = kYGrey;
		}
	}
}

inline void setLuma(uint8_t* frame, int rowBytes, int y0, int y1, int x0, int width, uint8_t value)
{
	for (int y = y0; y < y1; ++y)
	{
		uint8_t* row = frame + static_cast<size_t>(y) * rowBytes;
		for (int x = x0; x < x0 + width; ++x)
			row[2 * x + 1] = value;
	}
}

inline void encode(uint8_t* frame, int rowBytes, uint32_t n)
{
	const uint8_t be[4] = {static_cast<uint8_t>(n >> 24), static_cast<uint8_t>(n >> 16),
	                       static_cast<uint8_t>(n >> 8), static_cast<uint8_t>(n)};
	const uint64_t value = ((static_cast<uint64_t>(n) << 16) | crc16(be, 4)) ^ kMask;
	for (int i = 0; i < kBits; ++i)
	{
		const bool bit = (value >> (kBits - 1 - i)) & 1;
		setLuma(frame, rowBytes, kStripY0, kStripY1, i * kBlockW, kBlockW, bit ? kYWhite : kYBlack);
	}
}

// Moving white bar so a frozen picture is obvious by eye.
inline void drawBar(uint8_t* frame, int rowBytes, uint32_t n)
{
	const int x = static_cast<int>((static_cast<uint64_t>(n) * 16) % (kWidth - 80));
	setLuma(frame, rowBytes, 700, 760, x, 80, kYWhite);
}

// Copy the base picture into dst and draw frame number n on it.
inline void fillFrame(uint8_t* dst, const uint8_t* base, int rowBytes, uint32_t n)
{
	std::memcpy(dst, base, static_cast<size_t>(rowBytes) * kHeight);
	encode(dst, rowBytes, n);
	drawBar(dst, rowBytes, n);
}
}  // namespace pattern
