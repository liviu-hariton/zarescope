// SPDX-License-Identifier: GPL-3.0-or-later

#include "decoders/adsb/ModeS.h"

#include <cmath>
#include <numbers>
#include <unordered_map>

namespace zarescope::adsb {

namespace {

constexpr uint32_t kGenerator = 0xFFF409;

// CRC table for one byte at a time.
constexpr std::array<uint32_t, 256> kCrcTable = [] {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i << 16;
        for (int bit = 0; bit < 8; ++bit) {
            c = (c & 0x800000) != 0 ? (c << 1) ^ kGenerator : c << 1;
        }
        table[i] = c & 0xFFFFFF;
    }
    return table;
}();

// Message bits `first`…`first + count − 1` (1-based, as in the standard).
uint32_t bits(std::span<const uint8_t> bytes, int first, int count) {
    uint32_t value = 0;
    for (int i = 0; i < count; ++i) {
        const int bit = first - 1 + i;
        value = (value << 1) | ((bytes[static_cast<size_t>(bit / 8)] >> (7 - bit % 8)) & 1U);
    }
    return value;
}

// ME bits (1-based within the 56-bit ME field).
uint32_t me(std::span<const uint8_t> bytes, int first, int count) {
    return bits(bytes, 32 + first, count);
}

// Syndrome → bit index, for single-bit errors in 112-bit messages.
const std::unordered_map<uint32_t, int>& syndromeTable() {
    static const auto table = [] {
        std::unordered_map<uint32_t, int> map;
        std::array<uint8_t, kLongBytes> message{};
        for (int bit = 0; bit < static_cast<int>(kLongBytes * 8); ++bit) {
            message.fill(0);
            message[static_cast<size_t>(bit / 8)] = static_cast<uint8_t>(0x80U >> (bit % 8));
            map.emplace(crcRemainder(message), bit);
        }
        return map;
    }();
    return table;
}

int grayToInt(uint32_t gray) {
    uint32_t n = gray;
    n ^= n >> 8;
    n ^= n >> 4;
    n ^= n >> 2;
    n ^= n >> 1;
    return static_cast<int>(n);
}

}  // namespace

uint32_t crcRemainder(std::span<const uint8_t> bytes) {
    if (bytes.size() < 4) {
        return 0xFFFFFFFF;
    }
    uint32_t crc = 0;
    for (size_t i = 0; i + 3 < bytes.size(); ++i) {
        crc = ((crc << 8) ^ kCrcTable[((crc >> 16) ^ bytes[i]) & 0xFF]) & 0xFFFFFF;
    }
    const size_t n = bytes.size();
    const uint32_t parity = (static_cast<uint32_t>(bytes[n - 3]) << 16) |
                            (static_cast<uint32_t>(bytes[n - 2]) << 8) | bytes[n - 1];
    return crc ^ parity;
}

bool correctSingleBit(std::span<uint8_t> bytes) {
    if (bytes.size() != kLongBytes) {
        return false;
    }
    const uint32_t syndrome = crcRemainder(bytes);
    if (syndrome == 0) {
        return false;
    }
    const auto& table = syndromeTable();
    const auto it = table.find(syndrome);
    if (it == table.end() || it->second < 5) {
        return false;  // never "fix" the downlink format itself
    }
    bytes[static_cast<size_t>(it->second / 8)] ^= static_cast<uint8_t>(0x80U >> (it->second % 8));
    return crcRemainder(bytes) == 0;
}

std::optional<int> decodeAltitude12(uint32_t field) {
    if (field == 0) {
        return std::nullopt;  // altitude not available
    }
    const bool q = ((field >> 4) & 1U) != 0;  // the 8th of the 12 bits
    if (q) {
        // Drop the Q bit: 11 bits of 25-ft steps from −1000 ft.
        const uint32_t n = ((field >> 5) << 4) | (field & 0xF);
        return static_cast<int>(n) * 25 - 1000;
    }
    // Gillham (Mode C) code. The 12 bits are C1 A1 C2 A2 C4 A4 B1 Q B2 D2 B4 D4.
    auto bit = [field](int index) { return (field >> (11 - index)) & 1U; };  // index 0 = C1
    const uint32_t c1 = bit(0), a1 = bit(1), c2 = bit(2), a2 = bit(3), c4 = bit(4), a4 = bit(5);
    const uint32_t b1 = bit(6), b2 = bit(8), d2 = bit(9), b4 = bit(10), d4 = bit(11);
    const uint32_t gray500 =
        (d2 << 7) | (d4 << 6) | (a1 << 5) | (a2 << 4) | (a4 << 3) | (b1 << 2) | (b2 << 1) | b4;
    const uint32_t gray100 = (c1 << 2) | (c2 << 1) | c4;
    const int n500 = grayToInt(gray500);
    int n100 = grayToInt(gray100);
    if (n100 == 0 || n100 == 5 || n100 == 6) {
        return std::nullopt;  // invalid code
    }
    if (n100 == 7) {
        n100 = 5;
    }
    if (n500 % 2 != 0) {
        n100 = 6 - n100;
    }
    return n500 * 500 + n100 * 100 - 1300;
}

std::optional<ExtendedSquitter> decodeExtendedSquitter(std::span<const uint8_t> bytes) {
    if (bytes.size() < kLongBytes) {
        return std::nullopt;
    }
    const int df = downlinkFormat(bytes);
    if (df != 17 && df != 18) {
        return std::nullopt;
    }
    ExtendedSquitter squitter;
    squitter.icao = icaoAddress(bytes);
    squitter.typeCode = static_cast<int>(me(bytes, 1, 5));
    const int tc = squitter.typeCode;

    if (tc >= 1 && tc <= 4) {
        // Identification: 8 characters of 6 bits from the AIS charset.
        static constexpr std::string_view kCharset =
            "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######";
        Identification id;
        id.category = static_cast<int>(me(bytes, 6, 3));
        for (int i = 0; i < 8; ++i) {
            const char c = kCharset[me(bytes, 9 + i * 6, 6)];
            id.callsign += c == '#' ? ' ' : c;
        }
        while (!id.callsign.empty() && id.callsign.back() == ' ') {
            id.callsign.pop_back();
        }
        squitter.identification = id;
    } else if ((tc >= 9 && tc <= 18) || (tc >= 20 && tc <= 22)) {
        AirbornePosition position;
        const uint32_t altitudeField = me(bytes, 9, 12);
        position.gnssAltitude = tc >= 20;
        if (position.gnssAltitude) {
            // GNSS height in metres, converted to feet.
            if (altitudeField != 0) {
                position.altitudeFt = static_cast<int>(std::lround(altitudeField * 3.28084));
            }
        } else {
            position.altitudeFt = decodeAltitude12(altitudeField);
        }
        position.odd = me(bytes, 22, 1) != 0;
        position.latCpr = me(bytes, 23, 17);
        position.lonCpr = me(bytes, 40, 17);
        squitter.position = position;
    } else if (tc == 19) {
        Velocity velocity;
        const uint32_t subtype = me(bytes, 6, 3);
        const int factor = (subtype == 2 || subtype == 4) ? 4 : 1;  // supersonic
        if (subtype == 1 || subtype == 2) {
            const uint32_t vew = me(bytes, 15, 10);
            const uint32_t vns = me(bytes, 26, 10);
            if (vew != 0 && vns != 0) {
                const double east =
                    (me(bytes, 14, 1) != 0 ? -1.0 : 1.0) * static_cast<double>((vew - 1) * factor);
                const double north =
                    (me(bytes, 25, 1) != 0 ? -1.0 : 1.0) * static_cast<double>((vns - 1) * factor);
                velocity.groundSpeedKt = std::hypot(east, north);
                double track = std::atan2(east, north) * 180.0 / std::numbers::pi;
                velocity.trackDeg = track < 0.0 ? track + 360.0 : track;
            }
        } else if (subtype == 3 || subtype == 4) {
            if (me(bytes, 14, 1) != 0) {
                velocity.headingDeg = static_cast<double>(me(bytes, 15, 10)) * 360.0 / 1024.0;
            }
            velocity.trueAirspeed = me(bytes, 25, 1) != 0;
            const uint32_t speed = me(bytes, 26, 10);
            if (speed != 0) {
                velocity.airspeedKt = static_cast<double>((speed - 1) * factor);
            }
        }
        const uint32_t rate = me(bytes, 38, 9);
        if (rate != 0) {
            velocity.verticalRateFpm =
                (me(bytes, 37, 1) != 0 ? -1 : 1) * static_cast<int>((rate - 1) * 64);
        }
        squitter.velocity = velocity;
    }
    return squitter;
}

std::optional<RawMessage> fromHex(std::string_view hex) {
    if (hex.size() != kLongBytes * 2 && hex.size() != kShortBytes * 2) {
        return std::nullopt;
    }
    RawMessage message;
    message.lengthBytes = hex.size() / 2;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    for (size_t i = 0; i < message.lengthBytes; ++i) {
        const int high = nibble(hex[2 * i]);
        const int low = nibble(hex[2 * i + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        message.bytes[i] = static_cast<uint8_t>(high * 16 + low);
    }
    return message;
}

std::string toHex(const RawMessage& message) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string text;
    for (size_t i = 0; i < message.lengthBytes; ++i) {
        text += kDigits[message.bytes[i] >> 4];
        text += kDigits[message.bytes[i] & 0xF];
    }
    return text;
}

}  // namespace zarescope::adsb
