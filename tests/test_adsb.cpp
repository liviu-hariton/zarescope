// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADS-B (M3). The decoded values are checked against the worked examples of
// Junzi Sun, "The 1090 MHz Riddle" (2nd ed.), which pyModeS reproduces:
// M3's acceptance test on known messages.

#include "decoders/adsb/AircraftStore.h"
#include "decoders/adsb/Cpr.h"
#include "decoders/adsb/Demodulator.h"
#include "decoders/adsb/Encoder.h"
#include "decoders/adsb/ModeS.h"
#include "dsp/IqConvert.h"

#include <doctest/doctest.h>

#include <random>

using namespace zarescope::adsb;

namespace {

RawMessage hex(std::string_view text) {
    auto message = fromHex(text);
    REQUIRE(message);
    return *message;
}

std::span<const uint8_t> bytesOf(const RawMessage& m) { return {m.bytes.data(), m.lengthBytes}; }

}  // namespace

TEST_CASE("adsb: CRC-24 is zero for intact messages and catches damage") {
    for (const char* text : {"8D4840D6202CC371C32CE0576098", "8D40621D58C382D690C8AC2863A7",
                             "8D40621D58C386435CC412692AD6", "8D485020994409940838175B284F",
                             "8DA05F219B06B6AF189400CBC33F"}) {
        CAPTURE(text);
        CHECK(crcRemainder(bytesOf(hex(text))) == 0);
    }
    RawMessage damaged = hex("8D4840D6202CC371C32CE0576098");
    damaged.bytes[6] ^= 0x10;
    CHECK(crcRemainder(bytesOf(damaged)) != 0);
}

TEST_CASE("adsb: a single flipped bit is corrected") {
    const RawMessage original = hex("8D4840D6202CC371C32CE0576098");
    for (int bit : {5, 20, 40, 87, 100}) {
        CAPTURE(bit);
        RawMessage damaged = original;
        damaged.bytes[static_cast<size_t>(bit / 8)] ^= static_cast<uint8_t>(0x80U >> (bit % 8));
        CHECK(correctSingleBit(std::span<uint8_t>(damaged.bytes.data(), kLongBytes)));
        CHECK(damaged.bytes == original.bytes);
    }
}

TEST_CASE("adsb: identification gives the callsign (Riddle ex. KLM1023)") {
    const auto squitter = decodeExtendedSquitter(bytesOf(hex("8D4840D6202CC371C32CE0576098")));
    REQUIRE(squitter);
    CHECK(squitter->icao == 0x4840D6);
    CHECK(squitter->typeCode == 4);
    REQUIRE(squitter->identification);
    CHECK(squitter->identification->callsign == "KLM1023");
}

TEST_CASE("adsb: airborne position decodes globally and locally (Riddle ch. 5)") {
    const auto even = decodeExtendedSquitter(bytesOf(hex("8D40621D58C382D690C8AC2863A7")));
    const auto odd = decodeExtendedSquitter(bytesOf(hex("8D40621D58C386435CC412692AD6")));
    REQUIRE(even);
    REQUIRE(odd);
    REQUIRE(even->position);
    REQUIRE(odd->position);
    CHECK(even->icao == 0x40621D);
    CHECK(even->typeCode == 11);
    CHECK_FALSE(even->position->odd);
    CHECK(odd->position->odd);
    CHECK(even->position->altitudeFt == 38000);
    CHECK(even->position->latCpr == 93000);
    CHECK(even->position->lonCpr == 51372);
    CHECK(odd->position->latCpr == 74158);
    CHECK(odd->position->lonCpr == 50194);

    // The even frame is the newer one.
    const auto global = cprGlobal(93000, 51372, 74158, 50194, false);
    REQUIRE(global);
    CHECK(global->latDeg == doctest::Approx(52.25720).epsilon(1e-6));
    CHECK(global->lonDeg == doctest::Approx(3.91937).epsilon(1e-5));

    // Locally, against a reference near the aircraft.
    const LatLon local = cprLocal(93000, 51372, false, {52.258, 3.918});
    CHECK(local.latDeg == doctest::Approx(52.25720).epsilon(1e-6));
    CHECK(local.lonDeg == doctest::Approx(3.91937).epsilon(1e-5));
}

TEST_CASE("adsb: velocity, ground speed and airspeed subtypes (Riddle ch. 6)") {
    const auto ground = decodeExtendedSquitter(bytesOf(hex("8D485020994409940838175B284F")));
    REQUIRE(ground);
    REQUIRE(ground->velocity);
    CHECK(*ground->velocity->groundSpeedKt == doctest::Approx(159.20).epsilon(1e-3));
    CHECK(*ground->velocity->trackDeg == doctest::Approx(182.88).epsilon(1e-3));
    CHECK(ground->velocity->verticalRateFpm == -832);

    const auto air = decodeExtendedSquitter(bytesOf(hex("8DA05F219B06B6AF189400CBC33F")));
    REQUIRE(air);
    REQUIRE(air->velocity);
    CHECK(*air->velocity->headingDeg == doctest::Approx(243.98).epsilon(1e-3));
    CHECK(*air->velocity->airspeedKt == doctest::Approx(375.0));
    CHECK(air->velocity->trueAirspeed);
    CHECK(air->velocity->verticalRateFpm == -2304);
}

TEST_CASE("adsb: altitude codes, 25-ft and Gillham") {
    // Q bit set: 25-ft steps; n = (38000 + 1000) / 25 = 1560.
    CHECK(decodeAltitude12(0xC38) == 38000);
    CHECK_FALSE(decodeAltitude12(0));  // not available

    // Gillham (Q = 0). Bits: C1 A1 C2 A2 C4 A4 B1 Q B2 D2 B4 D4. Worked out
    // from the rules: altitude + 1300 = 500·n500 + 100·n100, both counts
    // Gray-coded, n100 mirrored (6 − n100) when n500 is odd.
    // 1000 ft: n500 = 4 → Gray 110 (B1 B2); n100 = 3 → Gray 010 (C2).
    CHECK(decodeAltitude12(0b001000101000) == 1000);
    // 1400 ft: n500 = 5 → Gray 111 (B1 B2 B4); n100 = 2, mirrored to 4 → Gray 110 (C1 C2).
    CHECK(decodeAltitude12(0b101000101010) == 1400);
    // All C bits zero is not a valid 100-ft count.
    CHECK_FALSE(decodeAltitude12(0b000000101000));
}

TEST_CASE("adsb: NL zones and distances") {
    CHECK(cprNl(0.0) == 59);
    CHECK(cprNl(52.2572) == 36);
    CHECK(cprNl(87.0) == 2);
    CHECK(cprNl(88.0) == 1);
    CHECK(distanceKm({52.0, 4.0}, {52.0, 4.0}) == doctest::Approx(0.0));
    CHECK(distanceKm({44.4268, 26.1025}, {46.7712, 23.6236}) ==
          doctest::Approx(324.0).epsilon(0.01));
    CHECK(bearingDeg({0.0, 0.0}, {1.0, 0.0}) == doctest::Approx(0.0));
    CHECK(bearingDeg({0.0, 0.0}, {0.0, 1.0}) == doctest::Approx(90.0));
}

TEST_CASE("adsb: encode → decode round-trips identification, position and velocity") {
    const auto id = decodeExtendedSquitter(bytesOf(encodeIdentification(0xABCDEF, "ROT123")));
    REQUIRE(id);
    CHECK(id->icao == 0xABCDEF);
    CHECK(id->identification->callsign == "ROT123");

    for (bool oddFrame : {false, true}) {
        const auto pos = decodeExtendedSquitter(
            bytesOf(encodeAirbornePosition(0x4B1234, 44.43, 26.10, 35000, oddFrame)));
        REQUIRE(pos);
        CHECK(pos->position->altitudeFt == 35000);
        CHECK(pos->position->odd == oddFrame);
        const LatLon local =
            cprLocal(pos->position->latCpr, pos->position->lonCpr, oddFrame, {44.4, 26.1});
        CHECK(local.latDeg == doctest::Approx(44.43).epsilon(1e-4));
        CHECK(local.lonDeg == doctest::Approx(26.10).epsilon(1e-4));
    }
    const auto vel = decodeExtendedSquitter(bytesOf(encodeVelocity(0x4B1234, 450.0, 270.0, -1024)));
    REQUIRE(vel);
    CHECK(*vel->velocity->groundSpeedKt == doctest::Approx(450.0).epsilon(0.005));
    CHECK(*vel->velocity->trackDeg == doctest::Approx(270.0).epsilon(0.005));
    CHECK(vel->velocity->verticalRateFpm == -1024);
}

TEST_CASE("adsb: the demodulator finds messages in noisy IQ, across block edges") {
    const std::vector<RawMessage> sent{
        encodeIdentification(0x4B1234, "TAROM12"),
        encodeAirbornePosition(0x4B1234, 44.5, 26.0, 33000, false),
        encodeVelocity(0x4B1234, 420.0, 90.0, 0),
        hex("8D40621D58C386435CC412692AD6"),
    };
    std::vector<uint8_t> iq;
    std::mt19937 rng(9);
    std::normal_distribution<float> noise(0.0F, 0.02F);
    auto silence = [&](size_t samples) {
        for (size_t i = 0; i < samples; ++i) {
            iq.push_back(zarescope::dsp::floatToCu8(noise(rng)));
            iq.push_back(zarescope::dsp::floatToCu8(noise(rng)));
        }
    };
    silence(1000);
    for (size_t k = 0; k < sent.size(); ++k) {
        modulate(sent[k], 0.4F, 0.7F * static_cast<float>(k), iq);
        silence(700 + 137 * k);
    }
    // Add the noise on top of the pulses too.
    for (auto& byte : iq) {
        byte = zarescope::dsp::floatToCu8(zarescope::dsp::cu8ToFloat(byte) + noise(rng));
    }

    // Feed in awkward block sizes so messages straddle block edges.
    ModeSDemodulator demod;
    std::vector<RawMessage> received;
    for (size_t pos = 0; pos < iq.size();) {
        const size_t take = std::min<size_t>(2 * 333, iq.size() - pos);
        demod.process(std::span(iq).subspan(pos, take), received);
        pos += take;
    }
    REQUIRE(received.size() == sent.size());
    for (size_t k = 0; k < sent.size(); ++k) {
        CAPTURE(k);
        CHECK(toHex(received[k]) == toHex(sent[k]));
        CHECK(received[k].signalDb > -12.0F);  // amplitude 0.4 ≈ −8 dBFS
    }
}

TEST_CASE("adsb: the demodulator corrects one damaged bit") {
    RawMessage message = encodeIdentification(0x4B1234, "TAROM12");
    const RawMessage original = message;
    message.bytes[7] ^= 0x04;  // flip one bit before modulating
    std::vector<uint8_t> iq(400, 128);
    modulate(message, 0.5F, 0.0F, iq);
    iq.insert(iq.end(), 600, 128);
    ModeSDemodulator demod;
    std::vector<RawMessage> received;
    demod.process(iq, received);
    REQUIRE(received.size() == 1);
    CHECK(received[0].corrected);
    CHECK(toHex(received[0]) == toHex(original));
}

TEST_CASE("adsb: the store pairs CPR frames, tracks and expires aircraft") {
    AircraftStore store;
    const auto t0 = Clock::now();
    store.update(encodeIdentification(0x4B1234, "TAROM12"), t0);
    store.update(encodeAirbornePosition(0x4B1234, 44.50, 26.00, 33000, false),
                 t0 + std::chrono::seconds(1));
    CHECK_FALSE(store.aircraft().at(0x4B1234).position);  // one frame, no station: not yet
    store.update(encodeAirbornePosition(0x4B1234, 44.51, 26.02, 33000, true),
                 t0 + std::chrono::seconds(2));
    const Aircraft& plane = store.aircraft().at(0x4B1234);
    REQUIRE(plane.position);
    CHECK(plane.position->latDeg == doctest::Approx(44.51).epsilon(1e-4));
    CHECK(plane.position->lonDeg == doctest::Approx(26.02).epsilon(1e-4));
    CHECK(plane.callsign == "TAROM12");
    CHECK(plane.altitudeFt == 33000);
    // Next frames decode locally and extend the trail.
    store.update(encodeAirbornePosition(0x4B1234, 44.52, 26.04, 33100, false),
                 t0 + std::chrono::seconds(3));
    CHECK(store.aircraft().at(0x4B1234).trail.size() == 2);
    store.update(encodeVelocity(0x4B1234, 430.0, 45.0, 512), t0 + std::chrono::seconds(3));
    CHECK(*store.aircraft().at(0x4B1234).groundSpeedKt == doctest::Approx(430.0).epsilon(0.005));
    CHECK(store.messages() == 5);

    // With a station, one frame is enough for a newcomer.
    store.setStation(LatLon{44.4, 26.1});
    store.update(encodeAirbornePosition(0x4B9999, 44.60, 26.30, 20000, true),
                 t0 + std::chrono::seconds(4));
    REQUIRE(store.aircraft().at(0x4B9999).position);
    CHECK(store.aircraft().at(0x4B9999).position->latDeg == doctest::Approx(44.60).epsilon(1e-4));

    store.expire(t0 + std::chrono::seconds(50));
    CHECK(store.aircraft().size() == 2);
    store.expire(t0 + std::chrono::seconds(70));
    CHECK(store.aircraft().empty());
}
