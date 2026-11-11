// SPDX-License-Identifier: GPL-3.0-or-later
//
// Mode S messages: CRC-24, single-bit error correction, and the ADS-B
// fields of extended squitters (DF17 from transponders, DF18 from other
// ADS-B transmitters).
//
// Bit numbering follows the standard: bit 1 is the first bit sent. Inside
// the 56-bit ME field of an extended squitter, "ME bit k" (1-based) is
// message bit 32 + k.
//
// References:
//   Junzi Sun, "The 1090 MHz Riddle", 2nd ed., TU Delft OPEN Publishing
//   (open access, https://mode-s.org/1090mhz/): message layouts, CPR,
//   altitude and velocity encodings, and the worked examples used as test
//   vectors.
//   ICAO Annex 10 Vol. IV and RTCA DO-260B for the definitions.
//   dump1090 / readsb were read for the error-correction approach; no code
//   was copied.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace zarescope::adsb {

inline constexpr size_t kLongBytes = 14;  // 112 bits: DF 16–24
inline constexpr size_t kShortBytes = 7;  // 56 bits: DF 0–15

// One received Mode S message, as bytes. Short messages use the first 7.
struct RawMessage {
    std::array<uint8_t, kLongBytes> bytes{};
    size_t lengthBytes = 0;
    float signalDb = 0.0F;   // received signal strength, dBFS
    bool corrected = false;  // one bit was fixed by the CRC
};

// Downlink format (bits 1–5) and the message length it implies.
inline int downlinkFormat(std::span<const uint8_t> bytes) { return bytes[0] >> 3; }
inline size_t messageBytes(int df) { return df >= 16 ? kLongBytes : kShortBytes; }

// The CRC-24 (generator 0xFFF409) of all but the last 24 bits, XORed with
// those last 24 bits. Zero for an intact DF17/DF18/DF11 (with interrogator 0).
uint32_t crcRemainder(std::span<const uint8_t> bytes);

// Fixes a single flipped bit in a 112-bit message whose remainder isn't
// zero. Returns true (and flips it back) when the remainder matches exactly
// one bit. Only meaningful for DF17/DF18, whose parity isn't overlaid.
bool correctSingleBit(std::span<uint8_t> bytes);

// --- Decoded content --------------------------------------------------------

struct Identification {
    int category = 0;      // emitter category (A1 = light … see DO-260B)
    std::string callsign;  // trailing spaces removed
};

// The raw CPR fields of an airborne position; turned into lat/lon by Cpr.h.
struct AirbornePosition {
    std::optional<int> altitudeFt;  // barometric (TC 9–18) or GNSS (TC 20–22)
    bool gnssAltitude = false;
    bool odd = false;     // CPR format: false = even frame
    uint32_t latCpr = 0;  // 17 bits
    uint32_t lonCpr = 0;  // 17 bits
};

struct Velocity {
    // Ground speed and track (subtypes 1–2), or airspeed and heading (3–4).
    std::optional<double> groundSpeedKt;
    std::optional<double> trackDeg;
    std::optional<double> airspeedKt;
    std::optional<double> headingDeg;
    bool trueAirspeed = false;
    std::optional<int> verticalRateFpm;
};

struct ExtendedSquitter {
    uint32_t icao = 0;  // 24-bit address
    int typeCode = 0;
    std::optional<Identification> identification;
    std::optional<AirbornePosition> position;
    std::optional<Velocity> velocity;
};

// Decodes the ADS-B content of a DF17/DF18 message with a valid CRC.
// Returns std::nullopt for other formats.
std::optional<ExtendedSquitter> decodeExtendedSquitter(std::span<const uint8_t> bytes);

// The 12-bit altitude field of an airborne position (ME bits 9–20) in feet:
// 25-ft steps when the Q bit is set, Gillham (Mode C) 100-ft code otherwise.
std::optional<int> decodeAltitude12(uint32_t field);

// The ICAO address of DF11 (all-call reply) and DF17/18.
inline uint32_t icaoAddress(std::span<const uint8_t> bytes) {
    return (static_cast<uint32_t>(bytes[1]) << 16) | (static_cast<uint32_t>(bytes[2]) << 8) |
           bytes[3];
}

// Parses hex text ("8D4840D6202CC371C32CE0576098") into bytes. For tests,
// tools and logs.
std::optional<RawMessage> fromHex(std::string_view hex);
std::string toHex(const RawMessage& message);

}  // namespace zarescope::adsb
