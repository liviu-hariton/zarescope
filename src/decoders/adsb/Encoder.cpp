// SPDX-License-Identifier: GPL-3.0-or-later

#include "decoders/adsb/Encoder.h"

#include "decoders/adsb/Cpr.h"
#include "dsp/IqConvert.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zarescope::adsb {

namespace {

// Writes `count` bits of `value` starting at message bit `first` (1-based).
void setBits(RawMessage& message, int first, int count, uint32_t value) {
    for (int i = 0; i < count; ++i) {
        const int bit = first - 1 + i;
        const auto mask = static_cast<uint8_t>(0x80U >> (bit % 8));
        if (((value >> (count - 1 - i)) & 1U) != 0) {
            message.bytes[static_cast<size_t>(bit / 8)] |= mask;
        } else {
            message.bytes[static_cast<size_t>(bit / 8)] &= static_cast<uint8_t>(~mask);
        }
    }
}
void setMe(RawMessage& message, int first, int count, uint32_t value) {
    setBits(message, 32 + first, count, value);
}

RawMessage header(uint32_t icao) {
    RawMessage message;
    message.lengthBytes = kLongBytes;
    setBits(message, 1, 5, 17);  // DF17
    setBits(message, 6, 3, 5);   // capability: airborne
    setBits(message, 9, 24, icao);
    return message;
}

// Fills in the parity (the CRC of the first 88 bits).
void seal(RawMessage& message) {
    setBits(message, 89, 24, 0);
    setBits(message, 89, 24,
            crcRemainder(std::span<const uint8_t>(message.bytes.data(), kLongBytes)));
}

double mod(double x, double y) { return x - y * std::floor(x / y); }

}  // namespace

RawMessage encodeIdentification(uint32_t icao, std::string_view callsign, int category) {
    static constexpr std::string_view kCharset =
        "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######";
    RawMessage message = header(icao);
    setMe(message, 1, 5, 4);
    setMe(message, 6, 3, static_cast<uint32_t>(category));
    for (int i = 0; i < 8; ++i) {
        const char c =
            i < static_cast<int>(callsign.size()) ? callsign[static_cast<size_t>(i)] : ' ';
        const auto index = kCharset.find(c);
        setMe(message, 9 + i * 6, 6,
              static_cast<uint32_t>(index == std::string_view::npos ? 32 : index));
    }
    seal(message);
    return message;
}

void cprEncode(double latDeg, double lonDeg, bool odd, uint32_t& latCpr, uint32_t& lonCpr) {
    constexpr double kScale = 131072.0;
    const int i = odd ? 1 : 0;
    const double dLat = 360.0 / (60.0 - i);
    const double yz = std::floor(kScale * mod(latDeg, dLat) / dLat + 0.5);
    const double rLat = dLat * (yz / kScale + std::floor(latDeg / dLat));
    const double dLon = 360.0 / std::max(cprNl(rLat) - i, 1);
    const double xz = std::floor(kScale * mod(lonDeg, dLon) / dLon + 0.5);
    latCpr = static_cast<uint32_t>(yz) & 0x1FFFF;
    lonCpr = static_cast<uint32_t>(xz) & 0x1FFFF;
}

RawMessage encodeAirbornePosition(uint32_t icao, double latDeg, double lonDeg, int altitudeFt,
                                  bool odd) {
    RawMessage message = header(icao);
    setMe(message, 1, 5, 11);  // TC 11: barometric altitude
    const auto n = static_cast<uint32_t>(std::max(0, (altitudeFt + 1000) / 25));
    setMe(message, 9, 12, ((n >> 4) << 5) | (1U << 4) | (n & 0xF));  // Q bit set: 25-ft steps
    setMe(message, 22, 1, odd ? 1U : 0U);
    uint32_t latCpr = 0;
    uint32_t lonCpr = 0;
    cprEncode(latDeg, lonDeg, odd, latCpr, lonCpr);
    setMe(message, 23, 17, latCpr);
    setMe(message, 40, 17, lonCpr);
    seal(message);
    return message;
}

RawMessage encodeVelocity(uint32_t icao, double groundSpeedKt, double trackDeg,
                          int verticalRateFpm) {
    RawMessage message = header(icao);
    setMe(message, 1, 5, 19);
    setMe(message, 6, 3, 1);  // subtype 1: ground speed, subsonic
    const double track = trackDeg * std::numbers::pi / 180.0;
    const double east = groundSpeedKt * std::sin(track);
    const double north = groundSpeedKt * std::cos(track);
    setMe(message, 14, 1, east < 0.0 ? 1U : 0U);
    setMe(message, 15, 10, static_cast<uint32_t>(std::lround(std::fabs(east))) + 1);
    setMe(message, 25, 1, north < 0.0 ? 1U : 0U);
    setMe(message, 26, 10, static_cast<uint32_t>(std::lround(std::fabs(north))) + 1);
    setMe(message, 37, 1, verticalRateFpm < 0 ? 1U : 0U);
    setMe(message, 38, 9, static_cast<uint32_t>(std::abs(verticalRateFpm) / 64 + 1));
    seal(message);
    return message;
}

void modulate(const RawMessage& message, float amplitude, float phase, std::vector<uint8_t>& iq) {
    const uint8_t onI = dsp::floatToCu8(amplitude * std::cos(phase));
    const uint8_t onQ = dsp::floatToCu8(amplitude * std::sin(phase));
    const uint8_t off = dsp::floatToCu8(0.0F);
    auto sample = [&](bool on) {
        iq.push_back(on ? onI : off);
        iq.push_back(on ? onQ : off);
    };
    // Preamble: pulses in samples 0, 2, 7 and 9 of 16.
    for (int s = 0; s < 16; ++s) {
        sample(s == 0 || s == 2 || s == 7 || s == 9);
    }
    for (size_t bit = 0; bit < message.lengthBytes * 8; ++bit) {
        const bool one = ((message.bytes[bit / 8] >> (7 - bit % 8)) & 1U) != 0;
        sample(one);
        sample(!one);
    }
}

}  // namespace zarescope::adsb
