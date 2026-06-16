#include "oceanwatchai/AisCsvLoader.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <utility>

namespace {

std::istringstream make_stream(std::string csv)
{
    return std::istringstream{std::move(csv)};
}

} // namespace

TEST_CASE("AIS CSV loader groups valid rows into vessel tracks")
{
    auto input = make_stream(
        "vessel_id,timestamp,latitude,longitude,speed_knots,course_deg\n"
        "FV-OCEAN-001,2026-06-16T05:00:00Z,51.5120,1.4220,8.4,145.0\n"
        "FV-OCEAN-001,2026-06-16T05:05:00Z,51.5065,1.4382,8.1,147.5\n");

    const auto result = oceanwatchai::load_ais_csv(input);

    REQUIRE(result.warnings.empty());
    REQUIRE(result.tracks.size() == 1);
    REQUIRE(result.point_count() == 2);

    const auto* track = result.find_track("FV-OCEAN-001");
    REQUIRE(track != nullptr);
    REQUIRE(track->size() == 2);

    const auto& first_point = track->points().front();
    CHECK(first_point.vessel_id == "FV-OCEAN-001");
    CHECK(first_point.timestamp == "2026-06-16T05:00:00Z");
    CHECK(first_point.latitude == Catch::Approx(51.5120));
    CHECK(first_point.longitude == Catch::Approx(1.4220));
    CHECK(first_point.speed_knots == Catch::Approx(8.4));
    CHECK(first_point.course_deg == Catch::Approx(145.0));
}

TEST_CASE("AIS CSV loader skips malformed rows and returns warnings")
{
    auto input = make_stream(
        "vessel_id,timestamp,latitude,longitude,speed_knots,course_deg\n"
        "FV-OCEAN-001,2026-06-16T05:00:00Z,51.5120,1.4220,8.4,145.0\n"
        "FV-OCEAN-001,2026-06-16T05:05:00Z,not-a-latitude,1.4382,8.1,147.5\n"
        "FV-OCEAN-001,2026-06-16T05:10:00Z,51.5009,1.4541,-2.0,149.2\n"
        "FV-OCEAN-001,2026-06-16T05:15:00Z,51.4900,1.4700,7.3,151.0\n");

    const auto result = oceanwatchai::load_ais_csv(input);

    REQUIRE(result.warnings.size() == 2);
    CHECK(result.warnings[0].line_number == 3);
    CHECK(result.warnings[1].line_number == 4);
    CHECK(result.point_count() == 2);

    const auto* track = result.find_track("FV-OCEAN-001");
    REQUIRE(track != nullptr);
    REQUIRE(track->size() == 2);
    CHECK(track->points()[0].timestamp == "2026-06-16T05:00:00Z");
    CHECK(track->points()[1].timestamp == "2026-06-16T05:15:00Z");
}

TEST_CASE("AIS CSV loader groups multiple vessels")
{
    auto input = make_stream(
        "vessel_id,timestamp,latitude,longitude,speed_knots,course_deg\n"
        "FV-OCEAN-002,2026-06-16T05:05:00Z,51.6211,1.3895,1.8,96.0\n"
        "FV-OCEAN-001,2026-06-16T05:00:00Z,51.5120,1.4220,8.4,145.0\n"
        "FV-OCEAN-002,2026-06-16T05:00:00Z,51.6200,1.3840,2.1,92.5\n");

    const auto result = oceanwatchai::load_ais_csv(input);

    REQUIRE(result.warnings.empty());
    REQUIRE(result.tracks.size() == 2);
    CHECK(result.point_count() == 3);

    const auto* first_track = result.find_track("FV-OCEAN-001");
    const auto* second_track = result.find_track("FV-OCEAN-002");
    REQUIRE(first_track != nullptr);
    REQUIRE(second_track != nullptr);
    CHECK(first_track->size() == 1);
    CHECK(second_track->size() == 2);
}

TEST_CASE("AIS CSV loader sorts each vessel track by timestamp")
{
    auto input = make_stream(
        "vessel_id,timestamp,latitude,longitude,speed_knots,course_deg\n"
        "FV-OCEAN-001,2026-06-16T05:10:00Z,51.5009,1.4541,7.9,149.2\n"
        "FV-OCEAN-001,2026-06-16T05:00:00Z,51.5120,1.4220,8.4,145.0\n"
        "FV-OCEAN-001,2026-06-16T05:05:00Z,51.5065,1.4382,8.1,147.5\n");

    const auto result = oceanwatchai::load_ais_csv(input);

    REQUIRE(result.warnings.empty());
    const auto* track = result.find_track("FV-OCEAN-001");
    REQUIRE(track != nullptr);
    REQUIRE(track->size() == 3);

    CHECK(track->points()[0].timestamp == "2026-06-16T05:00:00Z");
    CHECK(track->points()[1].timestamp == "2026-06-16T05:05:00Z");
    CHECK(track->points()[2].timestamp == "2026-06-16T05:10:00Z");
}

TEST_CASE("AIS CSV loader rejects files missing required columns")
{
    auto input = make_stream(
        "vessel_id,timestamp,latitude,longitude,speed_knots\n"
        "FV-OCEAN-001,2026-06-16T05:00:00Z,51.5120,1.4220,8.4\n");

    CHECK_THROWS_AS(oceanwatchai::load_ais_csv(input), oceanwatchai::CsvLoadError);
}
