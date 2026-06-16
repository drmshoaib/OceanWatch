#include "oceanwatchai/RiskScoringEngine.hpp"
#include "oceanwatchai/StatisticalAnomalyDetector.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

oceanwatchai::TrackFeatures make_features(std::string vessel_id, double mean_speed_knots)
{
    return oceanwatchai::TrackFeatures{
        .vessel_id = std::move(vessel_id),
        .duration_hours = 4.0,
        .mean_speed_knots = mean_speed_knots,
        .max_speed_knots = mean_speed_knots + 2.0,
    };
}

bool has_explanation_containing(const oceanwatchai::RiskScoreResult& result, std::string_view text)
{
    return std::any_of(result.explanations.begin(), result.explanations.end(), [text](const std::string& explanation) {
        return explanation.find(text) != std::string::npos;
    });
}

} // namespace

TEST_CASE("Z-score detector flags a vessel against a clean fleet baseline")
{
    const std::vector<oceanwatchai::TrackFeatures> baseline{
        make_features("NORMAL-001", 8.0),
        make_features("NORMAL-002", 10.0),
        make_features("NORMAL-003", 10.0),
        make_features("NORMAL-004", 12.0),
    };
    const auto anomalous = make_features("ANOMALOUS-001", 20.0);

    const oceanwatchai::StatisticalAnomalyDetector detector;
    const auto result = detector.score_against_baseline(
        anomalous,
        baseline,
        oceanwatchai::AnomalyDetectionMethod::ZScore);

    REQUIRE(result.feature_flags.size() == 11);
    CHECK(result.vessel_id == "ANOMALOUS-001");
    CHECK(result.method == oceanwatchai::AnomalyDetectionMethod::ZScore);
    CHECK(oceanwatchai::anomaly_method_to_string(result.method) == "z-score");
    CHECK(result.anomaly_score == Catch::Approx(100.0 * 2.0 / 3.0));

    const auto* mean_speed_flag = result.find_feature("mean_speed_knots");
    REQUIRE(mean_speed_flag != nullptr);
    CHECK(mean_speed_flag->is_anomaly);
    CHECK(mean_speed_flag->baseline_location == Catch::Approx(10.0));
    CHECK(mean_speed_flag->baseline_scale == Catch::Approx(std::sqrt(2.0)));
    CHECK(mean_speed_flag->normalized_deviation == Catch::Approx(10.0 / std::sqrt(2.0)));
    CHECK(mean_speed_flag->contribution_score == Catch::Approx(100.0));
    CHECK(mean_speed_flag->explanation.find("mean_speed_knots") != std::string::npos);

    const auto* duration_flag = result.find_feature("duration_hours");
    REQUIRE(duration_flag != nullptr);
    CHECK_FALSE(duration_flag->is_anomaly);
}

TEST_CASE("MAD detector is robust when the fleet baseline contains an outlier")
{
    const std::vector<oceanwatchai::TrackFeatures> baseline{
        make_features("NORMAL-001", 9.0),
        make_features("NORMAL-002", 10.0),
        make_features("NORMAL-003", 11.0),
        make_features("NORMAL-004", 12.0),
        make_features("FLEET-OUTLIER", 100.0),
    };
    const auto vessel = make_features("ANOMALOUS-001", 30.0);

    const oceanwatchai::StatisticalAnomalyDetector detector;
    const auto z_result = detector.score_against_baseline(
        vessel,
        baseline,
        oceanwatchai::AnomalyDetectionMethod::ZScore);
    const auto mad_result = detector.score_against_baseline(
        vessel,
        baseline,
        oceanwatchai::AnomalyDetectionMethod::MedianAbsoluteDeviation);

    const auto* z_mean_speed = z_result.find_feature("mean_speed_knots");
    const auto* mad_mean_speed = mad_result.find_feature("mean_speed_knots");
    REQUIRE(z_mean_speed != nullptr);
    REQUIRE(mad_mean_speed != nullptr);

    CHECK_FALSE(z_mean_speed->is_anomaly);
    CHECK(mad_mean_speed->is_anomaly);
    CHECK(mad_mean_speed->baseline_location == Catch::Approx(11.0));
    CHECK(mad_mean_speed->baseline_scale == Catch::Approx(1.0));
    CHECK(mad_mean_speed->normalized_deviation == Catch::Approx(0.6745 * 19.0));
    CHECK(mad_result.anomaly_score == Catch::Approx(100.0 * 2.0 / 3.0));
}

TEST_CASE("Fleet scoring uses leave-one-out baselines")
{
    const std::vector<oceanwatchai::TrackFeatures> fleet{
        make_features("NORMAL-001", 10.0),
        make_features("NORMAL-002", 11.0),
        make_features("NORMAL-003", 12.0),
        make_features("ANOMALOUS-001", 40.0),
    };

    const oceanwatchai::StatisticalAnomalyDetector detector;
    const auto results = detector.score_fleet(fleet, oceanwatchai::AnomalyDetectionMethod::MedianAbsoluteDeviation);

    REQUIRE(results.size() == 4);
    const auto anomalous_result = std::find_if(results.begin(), results.end(), [](const auto& result) {
        return result.vessel_id == "ANOMALOUS-001";
    });
    REQUIRE(anomalous_result != results.end());

    const auto* mean_speed_flag = anomalous_result->find_feature("mean_speed_knots");
    REQUIRE(mean_speed_flag != nullptr);
    CHECK(mean_speed_flag->is_anomaly);
    CHECK(anomalous_result->anomaly_score == Catch::Approx(100.0 * 2.0 / 3.0));

    const auto normal_result = std::find_if(results.begin(), results.end(), [](const auto& result) {
        return result.vessel_id == "NORMAL-001";
    });
    REQUIRE(normal_result != results.end());
    CHECK_FALSE(normal_result->find_feature("mean_speed_knots")->is_anomaly);
}

TEST_CASE("Risk engine can use statistical anomaly score as route anomaly component")
{
    auto features = make_features("ANOMALOUS-001", 40.0);
    features.suspicious_manoeuvre_score = 0.0;

    oceanwatchai::AnomalyDetectionResult anomaly{
        .vessel_id = "ANOMALOUS-001",
        .anomaly_score = 90.0,
        .method = oceanwatchai::AnomalyDetectionMethod::MedianAbsoluteDeviation,
        .explanations = {"mean_speed_knots was anomalous under median absolute deviation detection."},
    };

    const oceanwatchai::RiskScoringEngine scorer;
    const auto result = scorer.score(features, anomaly);

    CHECK(result.component_scores.route_anomaly == Catch::Approx(90.0));
    CHECK(result.total_score == Catch::Approx(9.0));
    CHECK(has_explanation_containing(result, "fleet-baseline anomaly detection"));
    CHECK(has_explanation_containing(result, "Anomaly detector: mean_speed_knots"));

    anomaly.vessel_id = "OTHER-VESSEL";
    CHECK_THROWS_AS(scorer.score(features, anomaly), std::invalid_argument);
}

TEST_CASE("Anomaly detector validates baseline and threshold inputs")
{
    const oceanwatchai::StatisticalAnomalyDetector detector;
    const auto vessel = make_features("VESSEL-001", 10.0);

    CHECK_THROWS_AS(
        detector.score_against_baseline(vessel, {}, oceanwatchai::AnomalyDetectionMethod::ZScore),
        std::invalid_argument);
    CHECK_THROWS_AS(detector.score_fleet({vessel}, oceanwatchai::AnomalyDetectionMethod::ZScore), std::invalid_argument);
    CHECK_THROWS_AS((oceanwatchai::StatisticalAnomalyDetector{0.0, 3.5}), std::invalid_argument);
    CHECK_THROWS_AS((oceanwatchai::StatisticalAnomalyDetector{2.0, 0.0}), std::invalid_argument);
}
