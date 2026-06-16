#include "oceanwatchai/FeatureExtractor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
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
        .vessel_id = "FV-FEATURE-001",
        .timestamp = std::move(timestamp),
        .latitude = latitude,
        .longitude = longitude,
        .speed_knots = speed_knots,
        .course_deg = course_deg,
    };
}

oceanwatchai::VesselTrack make_track(std::initializer_list<oceanwatchai::AISPoint> points)
{
    oceanwatchai::VesselTrack track{"FV-FEATURE-001"};
    for (auto point : points) {
        track.add_point(std::move(point));
    }
    return track;
}

} // namespace

TEST_CASE("Feature extractor returns zero-valued features for an empty track")
{
    const oceanwatchai::FeatureExtractor extractor;
    const oceanwatchai::VesselTrack track{"FV-FEATURE-001"};

    const auto features = extractor.extract(track);

    CHECK(features.vessel_id == "FV-FEATURE-001");
    CHECK(features.total_distance_km == Catch::Approx(0.0));
    CHECK(features.duration_hours == Catch::Approx(0.0));
    CHECK(features.mean_speed_knots == Catch::Approx(0.0));
    CHECK(features.max_speed_knots == Catch::Approx(0.0));
    CHECK(features.fraction_low_speed == Catch::Approx(0.0));
    CHECK(features.fraction_high_turning == Catch::Approx(0.0));
    CHECK(features.mean_turning_angle == Catch::Approx(0.0));
    CHECK(features.max_time_gap_hours == Catch::Approx(0.0));
    CHECK(features.number_of_ais_gaps == 0);
    CHECK(features.loitering_score == Catch::Approx(0.0));
    CHECK(features.suspicious_manoeuvre_score == Catch::Approx(0.0));
}

TEST_CASE("Feature extractor computes distance, duration, and reported speed features")
{
    const oceanwatchai::FeatureExtractor extractor;
    const auto track = make_track({
        make_point("2026-06-16T05:00:00Z", 0.0, 0.0, 10.0, 90.0),
        make_point("2026-06-16T06:00:00Z", 0.0, 1.0, 20.0, 90.0),
    });

    const auto features = extractor.extract(track);

    CHECK(features.total_distance_km == Catch::Approx(111.195).epsilon(0.001));
    CHECK(features.duration_hours == Catch::Approx(1.0));
    CHECK(features.mean_speed_knots == Catch::Approx(15.0));
    CHECK(features.max_speed_knots == Catch::Approx(20.0));
    CHECK(features.fraction_low_speed == Catch::Approx(0.0));
    CHECK(features.fraction_high_turning == Catch::Approx(0.0));
    CHECK(features.mean_turning_angle == Catch::Approx(0.0));
    CHECK(features.max_time_gap_hours == Catch::Approx(1.0));
    CHECK(features.number_of_ais_gaps == 0);
    CHECK(features.loitering_score == Catch::Approx(0.0));
    CHECK(features.suspicious_manoeuvre_score == Catch::Approx(0.0));
}

TEST_CASE("Feature extractor computes low speed, turning, gaps, and scores")
{
    const oceanwatchai::FeatureExtractor extractor;
    const auto track = make_track({
        make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 2.0, 0.0),
        make_point("2026-06-16T06:00:00Z", 51.5, 1.4, 4.0, 90.0),
        make_point("2026-06-16T09:00:00Z", 51.5, 1.4, 10.0, 100.0),
    });

    const auto features = extractor.extract(track);

    CHECK(features.total_distance_km == Catch::Approx(0.0));
    CHECK(features.duration_hours == Catch::Approx(4.0));
    CHECK(features.mean_speed_knots == Catch::Approx(16.0 / 3.0));
    CHECK(features.max_speed_knots == Catch::Approx(10.0));
    CHECK(features.fraction_low_speed == Catch::Approx(2.0 / 3.0));
    CHECK(features.fraction_high_turning == Catch::Approx(0.5));
    CHECK(features.mean_turning_angle == Catch::Approx(50.0));
    CHECK(features.max_time_gap_hours == Catch::Approx(3.0));
    CHECK(features.number_of_ais_gaps == 1);
    CHECK(features.loitering_score == Catch::Approx(1.0 / 3.0));
    CHECK(features.suspicious_manoeuvre_score == Catch::Approx(0.5));
}

TEST_CASE("Feature extractor applies strict high-turning and AIS-gap thresholds")
{
    const oceanwatchai::FeatureExtractor extractor;
    const auto track = make_track({
        make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 3.0, 0.0),
        make_point("2026-06-16T07:00:00Z", 51.5, 1.4, 3.0, 45.0),
        make_point("2026-06-16T09:00:00Z", 51.5, 1.4, 3.0, 91.0),
    });

    const auto features = extractor.extract(track);

    CHECK(features.fraction_low_speed == Catch::Approx(1.0));
    CHECK(features.fraction_high_turning == Catch::Approx(0.5));
    CHECK(features.mean_turning_angle == Catch::Approx(45.5));
    CHECK(features.max_time_gap_hours == Catch::Approx(2.0));
    CHECK(features.number_of_ais_gaps == 0);
    CHECK(features.loitering_score == Catch::Approx(0.5));
    CHECK(features.suspicious_manoeuvre_score == Catch::Approx(0.35));
}

TEST_CASE("Feature extractor handles a single-point track")
{
    const oceanwatchai::FeatureExtractor extractor;
    const auto track = make_track({
        make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 3.0, 270.0),
    });

    const auto features = extractor.extract(track);

    CHECK(features.duration_hours == Catch::Approx(0.0));
    CHECK(features.mean_speed_knots == Catch::Approx(3.0));
    CHECK(features.max_speed_knots == Catch::Approx(3.0));
    CHECK(features.fraction_low_speed == Catch::Approx(1.0));
    CHECK(features.fraction_high_turning == Catch::Approx(0.0));
    CHECK(features.loitering_score == Catch::Approx(0.0));
    CHECK(features.suspicious_manoeuvre_score == Catch::Approx(0.0));
}

TEST_CASE("Feature extractor rejects unsorted tracks and invalid thresholds")
{
    const oceanwatchai::FeatureExtractor extractor;
    const auto unsorted_track = make_track({
        make_point("2026-06-16T06:00:00Z", 51.5, 1.4, 3.0, 0.0),
        make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 3.0, 0.0),
    });

    CHECK_THROWS_AS(extractor.extract(unsorted_track), std::invalid_argument);
    CHECK_THROWS_AS((oceanwatchai::FeatureExtractor{5.0, 1.0}), std::invalid_argument);
    CHECK_THROWS_AS((oceanwatchai::FeatureExtractor{1.0, 5.0, 181.0}), std::invalid_argument);
    CHECK_THROWS_AS((oceanwatchai::FeatureExtractor{1.0, 5.0, 45.0, -1.0}), std::invalid_argument);
}
