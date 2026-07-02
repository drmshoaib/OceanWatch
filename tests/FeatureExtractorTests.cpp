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

void check_features_equal(const oceanwatchai::TrackFeatures& actual, const oceanwatchai::TrackFeatures& expected)
{
    CHECK(actual.vessel_id == expected.vessel_id);
    CHECK(actual.total_distance_km == Catch::Approx(expected.total_distance_km));
    CHECK(actual.duration_hours == Catch::Approx(expected.duration_hours));
    CHECK(actual.mean_speed_knots == Catch::Approx(expected.mean_speed_knots));
    CHECK(actual.max_speed_knots == Catch::Approx(expected.max_speed_knots));
    CHECK(actual.fraction_low_speed == Catch::Approx(expected.fraction_low_speed));
    CHECK(actual.fraction_high_turning == Catch::Approx(expected.fraction_high_turning));
    CHECK(actual.mean_turning_angle == Catch::Approx(expected.mean_turning_angle));
    CHECK(actual.max_time_gap_hours == Catch::Approx(expected.max_time_gap_hours));
    CHECK(actual.number_of_ais_gaps == expected.number_of_ais_gaps);
    CHECK(actual.loitering_score == Catch::Approx(expected.loitering_score));
    CHECK(actual.suspicious_manoeuvre_score == Catch::Approx(expected.suspicious_manoeuvre_score));
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

TEST_CASE("Feature extractor constructed from default config matches default constructor")
{
    const auto config = oceanwatchai::default_analysis_config().feature_extraction;
    const oceanwatchai::FeatureExtractor default_extractor;
    const oceanwatchai::FeatureExtractor configured_extractor{config};
    const auto track = make_track({
        make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 2.0, 0.0),
        make_point("2026-06-16T06:00:00Z", 51.5, 1.4, 4.0, 90.0),
        make_point("2026-06-16T09:00:00Z", 51.5, 1.4, 10.0, 100.0),
    });

    check_features_equal(configured_extractor.extract(track), default_extractor.extract(track));
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

TEST_CASE("Feature extractor uses custom feature extraction config")
{
    auto config = oceanwatchai::default_analysis_config().feature_extraction;
    config.low_speed_min_knots = 2.0;
    config.low_speed_max_knots = 4.0;
    config.high_turning_threshold_deg = 30.0;
    config.ais_gap_threshold_hours = 1.0;
    config.suspicious_manoeuvre_turning_weight = 0.2;
    config.suspicious_manoeuvre_gap_weight = 0.8;

    const oceanwatchai::FeatureExtractor extractor{config};
    const auto track = make_track({
        make_point("2026-06-16T05:00:00Z", 51.5, 1.4, 1.5, 0.0),
        make_point("2026-06-16T06:00:00Z", 51.5, 1.4, 3.0, 40.0),
        make_point("2026-06-16T07:30:00Z", 51.5, 1.4, 6.0, 100.0),
    });

    const auto features = extractor.extract(track);

    CHECK(features.fraction_low_speed == Catch::Approx(1.0 / 3.0));
    CHECK(features.fraction_high_turning == Catch::Approx(1.0));
    CHECK(features.max_time_gap_hours == Catch::Approx(1.5));
    CHECK(features.number_of_ais_gaps == 1);
    CHECK(features.loitering_score == Catch::Approx(1.0 / 3.0));
    CHECK(features.suspicious_manoeuvre_score == Catch::Approx(0.6));
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

    auto invalid_config = oceanwatchai::FeatureExtractionConfig{};
    invalid_config.low_speed_max_knots = 0.5;
    CHECK_THROWS_AS((oceanwatchai::FeatureExtractor{invalid_config}), oceanwatchai::AnalysisConfigError);
}
