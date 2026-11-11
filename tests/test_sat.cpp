// SPDX-License-Identifier: GPL-3.0-or-later
//
// Satellites (M4): the catalog, TLE parsing, SGP4 against Vallado's
// verification values, and pass prediction (sanity rules plus a golden
// regression test for a fixed TLE and station).

#include "sat/Orbit.h"
#include "sat/SatCatalog.h"

#include <doctest/doctest.h>

#include <cmath>
#include <format>

using namespace zarescope::sat;
using namespace std::chrono;

namespace {

// Vallado's verification satellite 00005 (SGP4-VER.TLE, "TEME example").
constexpr std::string_view kLine1 =
    "1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753";
constexpr std::string_view kLine2 =
    "2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667";

// Replaces a TLE line's last character with its correct checksum.
std::string withChecksum(std::string line) {
    int sum = 0;
    for (size_t i = 0; i < 68; ++i) {
        if (line[i] >= '0' && line[i] <= '9') {
            sum += line[i] - '0';
        } else if (line[i] == '-') {
            sum += 1;
        }
    }
    line[68] = static_cast<char>('0' + sum % 10);
    return line;
}

// A sun-synchronous weather satellite like Meteor-M (830 km, 98.7°,
// 14.21 revolutions a day). Epoch 2026-10-01 00:00 UTC (day 274).
Tle meteorLikeTle() {
    const std::string line1 =
        withChecksum("1 99999U 26001A   26274.00000000  .00000050  00000-0  40000-4 0  9990");
    const std::string line2 =
        withChecksum("2 99999  98.7000 120.0000 0005000  90.0000 270.0000 14.21000000 10000");
    std::string error;
    auto tle = parseTle("TEST-SAT\n" + line1 + "\n" + line2 + "\n", error);
    INFO(error);
    REQUIRE(tle);
    return *tle;
}

const Station kBucharest{44.4268, 26.1025, 80.0};

}  // namespace

TEST_CASE("sat: the shipped catalog lists the Meteor satellites with their frequencies") {
    std::string error;
    const auto catalog = loadCatalog(ZARESCOPE_SAT_CATALOG_FILE, error);
    INFO(error);
    REQUIRE(catalog);
    const auto n24 = std::find_if(catalog->begin(), catalog->end(),
                                  [](const Satellite& s) { return s.noradId == 59051; });
    REQUIRE(n24 != catalog->end());
    CHECK(n24->name == "Meteor-M N2-4");
    REQUIRE_FALSE(n24->downlinks.empty());
    CHECK(n24->downlinks[0].frequencyHz == 137'900'000);
    CHECK(n24->downlinks[0].mode == "lrpt");
    CHECK_FALSE(
        parseCatalog(R"({"zarescopeCatalog":1,"satellites":[{"noradId":1,"name":"A","downlinks":[]},
                                                                    {"noradId":1,"name":"B","downlinks":[]}]})",
                     error));
}

TEST_CASE("sat: TLEs parse with or without a name, and bad ones are refused") {
    std::string error;
    auto tle = parseTle(std::string(kLine1) + "\n" + std::string(kLine2) + "\n", error);
    REQUIRE(tle);
    CHECK(tle->noradId == 5);
    CHECK(tle->name.empty());
    // Epoch 2000, day 179.78495062.
    const auto expected =
        sys_days{year{2000} / January / 1} +
        duration_cast<system_clock::duration>(duration<double, std::ratio<86400>>(178.78495062));
    CHECK(abs(tle->epoch - expected) < milliseconds{2});

    auto named =
        parseTle("VANGUARD 1\r\n" + std::string(kLine1) + "\r\n" + std::string(kLine2), error);
    REQUIRE(named);
    CHECK(named->name == "VANGUARD 1");

    std::string broken(kLine1);
    broken[20] = '9';  // checksum no longer matches
    CHECK_FALSE(parseTle(broken + "\n" + std::string(kLine2), error));
    CHECK(error.find("checksum") != std::string::npos);
    CHECK_FALSE(parseTle("just one line", error));
}

TEST_CASE("sat: SGP4 matches Vallado's verification values (M4 acceptance)") {
    // Vallado et al., "Revisiting Spacetrack Report #3" (AIAA 2006-6753),
    // tcppver.out for satellite 00005. Tolerance: 1 m in position, 1 mm/s
    // in velocity.
    std::string error;
    const auto tle = parseTle(std::string(kLine1) + "\n" + std::string(kLine2), error);
    REQUIRE(tle);
    const Propagator propagator(*tle);
    double r[3];
    double v[3];

    propagator.temeState(tle->epoch, r, v);
    CHECK(r[0] == doctest::Approx(7022.46529266).epsilon(1e-3 / 7022.0));
    CHECK(r[1] == doctest::Approx(-1400.08296755).epsilon(1e-3 / 1400.0));
    CHECK(std::fabs(r[2] - 0.03995155) < 1e-3);
    CHECK(v[0] == doctest::Approx(1.893841015).epsilon(1e-6));
    CHECK(v[1] == doctest::Approx(6.405893759).epsilon(1e-6));
    CHECK(v[2] == doctest::Approx(4.534807250).epsilon(1e-6));

    propagator.temeState(tle->epoch + minutes{360}, r, v);
    CHECK(r[0] == doctest::Approx(-7154.03120202).epsilon(1e-3 / 7154.0));
    CHECK(r[1] == doctest::Approx(-3783.17682504).epsilon(1e-3 / 3783.0));
    CHECK(r[2] == doctest::Approx(-3536.19412294).epsilon(1e-3 / 3536.0));
    CHECK(v[0] == doctest::Approx(4.741887409).epsilon(1e-6));
    CHECK(v[1] == doctest::Approx(-4.151817765).epsilon(1e-6));
    CHECK(v[2] == doctest::Approx(-2.093935425).epsilon(1e-6));
}

TEST_CASE("sat: predicted passes obey the physics") {
    const Tle tle = meteorLikeTle();
    const Propagator propagator(tle);
    const auto passes = predictPasses(propagator, kBucharest, tle.epoch, hours{48}, 0.0);
    // A sun-synchronous satellite at 830 km passes over mid-latitudes
    // roughly 4–7 times a day (any elevation).
    CHECK(passes.size() >= 8);
    CHECK(passes.size() <= 16);
    for (size_t i = 0; i < passes.size(); ++i) {
        const Pass& p = passes[i];
        CAPTURE(i);
        CHECK(p.aos < p.tca);
        CHECK(p.tca < p.los);
        const auto minutesUp = duration<double, std::ratio<60>>(p.los - p.aos).count();
        CHECK(minutesUp > 1.0);
        CHECK(minutesUp < 17.0);
        CHECK(p.maxElevationDeg > 0.0);
        CHECK(p.maxElevationDeg <= 90.0);
        // AOS and LOS lie on the horizon, to within the 1-s bisection.
        CHECK(std::fabs(propagator.look(kBucharest, p.aos).elevationDeg) < 0.15);
        CHECK(std::fabs(propagator.look(kBucharest, p.los).elevationDeg) < 0.15);
        if (i > 0) {
            CHECK(passes[i - 1].los < p.aos);
        }
    }
    // The elevation filter only removes passes.
    const auto high = predictPasses(propagator, kBucharest, tle.epoch, hours{48}, 20.0);
    CHECK(high.size() < passes.size());
    for (const Pass& p : high) {
        CHECK(p.maxElevationDeg >= 20.0);
    }
    // The polar track runs from horizon to horizon through the maximum.
    const auto track = passTrack(propagator, kBucharest, high.front(), 60);
    REQUIRE(track.size() == 61);
    CHECK(std::fabs(track.front().elevationDeg) < 0.15);
    const auto highest = std::max_element(
        track.begin(), track.end(),
        [](const LookAngle& a, const LookAngle& b) { return a.elevationDeg < b.elevationDeg; });
    CHECK(highest->elevationDeg == doctest::Approx(high.front().maxElevationDeg).epsilon(0.02));
}

TEST_CASE("sat: predicted passes are stable (golden)") {
    // Pinned values for this TLE and station. If they change, either the
    // propagator or the predictor changed: check why before updating them.
    const Tle tle = meteorLikeTle();
    const Propagator propagator(tle);
    const auto passes = predictPasses(propagator, kBucharest, tle.epoch, hours{48}, 20.0);
    REQUIRE_FALSE(passes.empty());
    std::string summary;
    for (const Pass& p : passes) {
        summary += std::format("{:%Y-%m-%dT%H:%M:%S} {:.1f} {:.0f}-{:.0f} {}\n",
                               floor<seconds>(p.aos), p.maxElevationDeg, p.aosAzimuthDeg,
                               p.losAzimuthDeg, p.northbound ? "N" : "S");
    }
    MESSAGE(summary);
    CHECK(summary == ZARESCOPE_GOLDEN_PASSES);
}

TEST_CASE("sat: an orbit that never sets yields no pass and doesn't hang") {
    // Geostationary-like: 1.0027 revolutions a day, equatorial. Seen from
    // the equator under it, it stays up forever.
    const std::string line1 =
        withChecksum("1 99998U 26001B   26274.00000000  .00000000  00000-0  00000-0 0  9990");
    const std::string line2 =
        withChecksum("2 99998   0.0100 100.0000 0001000  90.0000 270.0000  1.00270000 10000");
    std::string error;
    const auto tle = parseTle(line1 + "\n" + line2, error);
    INFO(error);
    REQUIRE(tle);
    const Propagator propagator(*tle);
    const auto start = steady_clock::now();
    const auto passes =
        predictPasses(propagator, Station{0.0, 0.0, 0.0}, tle->epoch, hours{48}, 0.0);
    CHECK(passes.empty());
    CHECK(steady_clock::now() - start < seconds{10});
}
