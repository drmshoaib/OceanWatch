#include "oceanwatchai/GeospatialUtils.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace {

oceanwatchai::AISPoint make_point(
    std::string timestamp,
    double latitude,
    double longitude,
    double speed_knots,
    double course_deg)
{
    return oceanwatchai::AISPoint{
        .vessel_id = "FV-TEST-001",
        .timestamp = std::move(timestamp),
        .latitude = latitude,
        .longitude = longitude,
        .speed_knots = speed_knots,
        .course_deg = course_deg,
    };
}

} // namespace

TEST_CASE("Haversine distance handles zero and known one-degree distances")
{
    CHECK(oceanwatchai::haversine_distance_km(51.5, 1.4, 51.5, 1.4) == Catch::Approx(0.0));
    CHECK(oceanwatchai::haversine_distance_km(0.0, 0.0, 0.0, 1.0) == Catch::Approx(111.195).epsilon(0.001));
    CHECK(oceanwatchai::haversine_distance_km(0.0, 0.0, 1.0, 0.0) == Catch::Approx(111.195).epsilon(0.001));

    const auto from = make_point("2026-06-16T05:00:00Z", 0.0, 0.0, 0.0, 0.0);
    const auto to = make_point("2026-06-16T06:00:00Z", 0.0, 1.0, 0.0, 0.0);
    CHECK(oceanwatchai::haversine_distance_km(from, to) == Catch::Approx(111.195).epsilon(0.001));
}

TEST_CASE("Haversine distance validates coordinate ranges")
{
    CHECK_THROWS_AS(oceanwatchai::haversine_distance_km(91.0, 0.0, 0.0, 0.0), std::invalid_argument);
    CHECK_THROWS_AS(oceanwatchai::haversine_distance_km(0.0, 181.0, 0.0, 0.0), std::invalid_argument);
}

TEST_CASE("Angular difference handles wrap-around near zero and 360 degrees")
{
    CHECK(oceanwatchai::normalize_degrees(360.0) == Catch::Approx(0.0));
    CHECK(oceanwatchai::normalize_degrees(-10.0) == Catch::Approx(350.0));
    CHECK(oceanwatchai::normalize_degrees(725.0) == Catch::Approx(5.0));

    CHECK(oceanwatchai::angular_difference_deg(350.0, 10.0) == Catch::Approx(20.0));
    CHECK(oceanwatchai::angular_difference_deg(359.0, 1.0) == Catch::Approx(2.0));
    CHECK(oceanwatchai::angular_difference_deg(0.0, 360.0) == Catch::Approx(0.0));
    CHECK(oceanwatchai::angular_difference_deg(90.0, 270.0) == Catch::Approx(180.0));
    CHECK(oceanwatchai::bearing_difference_deg(10.0, 350.0) == Catch::Approx(20.0));
}

TEST_CASE("Time difference handles UTC ISO-8601 timestamps")
{
    CHECK(oceanwatchai::time_difference_hours("2026-06-16T05:00:00Z", "2026-06-16T06:00:00Z") ==
          Catch::Approx(1.0));
    CHECK(oceanwatchai::time_difference_hours("2026-06-16T23:30:00Z", "2026-06-17T01:00:00Z") ==
          Catch::Approx(1.5));
    CHECK(oceanwatchai::time_difference_hours("2024-02-28T00:00:00Z", "2024-03-01T00:00:00Z") ==
          Catch::Approx(48.0));
    CHECK(oceanwatchai::time_difference_hours("2026-06-16T06:00:00Z", "2026-06-16T05:00:00Z") ==
          Catch::Approx(-1.0));

    const auto from = make_point("2026-06-16T05:00:00Z", 0.0, 0.0, 0.0, 0.0);
    const auto to = make_point("2026-06-16T06:30:00Z", 0.0, 0.0, 0.0, 0.0);
    CHECK(oceanwatchai::time_difference_hours(from, to) == Catch::Approx(1.5));
}

TEST_CASE("Time difference rejects unsupported timestamp formats")
{
    CHECK_THROWS_AS(
        oceanwatchai::time_difference_hours("2026-06-16 05:00:00", "2026-06-16T06:00:00Z"),
        std::invalid_argument);
    CHECK_THROWS_AS(
        oceanwatchai::time_difference_hours("2026-02-29T05:00:00Z", "2026-03-01T05:00:00Z"),
        std::invalid_argument);
}

TEST_CASE("Segment speed and consistency checks compare implied and reported speed")
{
    const auto from = make_point("2026-06-16T05:00:00Z", 0.0, 0.0, 60.0, 90.0);
    const auto to_consistent = make_point("2026-06-16T06:00:00Z", 0.0, 1.0, 60.0, 90.0);
    const auto to_inconsistent = make_point("2026-06-16T06:00:00Z", 0.0, 1.0, 5.0, 90.0);

    CHECK(oceanwatchai::segment_speed_knots(from, to_consistent) == Catch::Approx(60.04).epsilon(0.001));
    CHECK(oceanwatchai::is_speed_consistent(from, to_consistent, 1.0));
    CHECK_FALSE(oceanwatchai::is_speed_consistent(from, to_inconsistent, 1.0));
}

TEST_CASE("Speed consistency returns false for non-positive elapsed time")
{
    const auto from = make_point("2026-06-16T05:00:00Z", 0.0, 0.0, 10.0, 90.0);
    const auto same_time = make_point("2026-06-16T05:00:00Z", 0.0, 0.1, 10.0, 90.0);

    CHECK_FALSE(oceanwatchai::is_speed_consistent(from, same_time));
    CHECK_THROWS_AS(oceanwatchai::segment_speed_knots(from, same_time), std::domain_error);
}

TEST_CASE("Approximate acceleration uses speed delta over elapsed hours")
{
    const auto from = make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 10.0, 90.0);
    const auto to = make_point("2026-06-16T07:00:00Z", 51.5, 1.5, 22.0, 90.0);
    const auto same_time = make_point("2026-06-16T05:00:00Z", 51.5, 1.5, 22.0, 90.0);

    CHECK(oceanwatchai::approximate_acceleration_knots_per_hour(from, to) == Catch::Approx(6.0));
    CHECK_THROWS_AS(oceanwatchai::approximate_acceleration_knots_per_hour(from, same_time), std::domain_error);
}

TEST_CASE("Turning angle uses minimal heading change")
{
    const auto north_west = make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 8.0, 350.0);
    const auto north_east = make_point("2026-06-16T05:05:00Z", 51.5, 1.5, 8.0, 10.0);
    const auto south = make_point("2026-06-16T05:10:00Z", 51.5, 1.6, 8.0, 180.0);

    CHECK(oceanwatchai::turning_angle_deg(north_west, north_east) == Catch::Approx(20.0));
    CHECK(oceanwatchai::turning_angle_deg(north_east, south) == Catch::Approx(170.0));
}
