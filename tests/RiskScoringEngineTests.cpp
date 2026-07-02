#include "oceanwatchai/RiskScoringEngine.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

oceanwatchai::TrackFeatures base_features()
{
    return oceanwatchai::TrackFeatures{
        .vessel_id = "FV-RISK-001",
    };
}

bool has_explanation_containing(const oceanwatchai::RiskScoreResult& result, std::string_view text)
{
    return std::any_of(result.explanations.begin(), result.explanations.end(), [text](const std::string& explanation) {
        return explanation.find(text) != std::string::npos;
    });
}

void check_results_equal(const oceanwatchai::RiskScoreResult& actual, const oceanwatchai::RiskScoreResult& expected)
{
    CHECK(actual.vessel_id == expected.vessel_id);
    CHECK(actual.total_score == Catch::Approx(expected.total_score));
    CHECK(actual.risk_band == expected.risk_band);
    CHECK(actual.explanations == expected.explanations);
    CHECK(actual.component_scores.loitering == Catch::Approx(expected.component_scores.loitering));
    CHECK(actual.component_scores.AIS_gap == Catch::Approx(expected.component_scores.AIS_gap));
    CHECK(actual.component_scores.low_speed == Catch::Approx(expected.component_scores.low_speed));
    CHECK(actual.component_scores.turning == Catch::Approx(expected.component_scores.turning));
    CHECK(actual.component_scores.route_anomaly == Catch::Approx(expected.component_scores.route_anomaly));
}

} // namespace

TEST_CASE("Risk scoring returns a low score when no indicators are elevated")
{
    const oceanwatchai::RiskScoringEngine scorer;

    const auto result = scorer.score(base_features());

    CHECK(result.vessel_id == "FV-RISK-001");
    CHECK(result.total_score == Catch::Approx(0.0));
    CHECK(result.risk_band == oceanwatchai::RiskBand::Low);
    CHECK(oceanwatchai::risk_band_to_string(result.risk_band) == "Low");
    REQUIRE(result.explanations.size() == 1);
    CHECK(has_explanation_containing(result, "No elevated risk indicators"));
    CHECK(result.component_scores.loitering == Catch::Approx(0.0));
    CHECK(result.component_scores.AIS_gap == Catch::Approx(0.0));
    CHECK(result.component_scores.low_speed == Catch::Approx(0.0));
    CHECK(result.component_scores.turning == Catch::Approx(0.0));
    CHECK(result.component_scores.route_anomaly == Catch::Approx(0.0));
}

TEST_CASE("Risk scoring constructed from default config matches default constructor")
{
    auto features = base_features();
    features.fraction_low_speed = 0.5;
    features.fraction_high_turning = 0.25;
    features.max_time_gap_hours = 7.0;
    features.number_of_ais_gaps = 1;
    features.loitering_score = 1.0;
    features.suspicious_manoeuvre_score = 0.8;

    const auto config = oceanwatchai::default_analysis_config();
    const oceanwatchai::RiskScoringEngine default_scorer;
    const oceanwatchai::RiskScoringEngine configured_scorer{config.risk_scoring, config.risk_bands};

    check_results_equal(configured_scorer.score(features), default_scorer.score(features));
}

TEST_CASE("Risk scoring computes component scores and a high composite score")
{
    auto features = base_features();
    features.fraction_low_speed = 0.5;
    features.fraction_high_turning = 0.25;
    features.max_time_gap_hours = 7.0;
    features.number_of_ais_gaps = 1;
    features.loitering_score = 1.0;
    features.suspicious_manoeuvre_score = 0.8;

    const oceanwatchai::RiskScoringEngine scorer;
    const auto result = scorer.score(features);

    CHECK(result.component_scores.loitering == Catch::Approx(100.0));
    CHECK(result.component_scores.AIS_gap == Catch::Approx(50.0));
    CHECK(result.component_scores.low_speed == Catch::Approx(50.0));
    CHECK(result.component_scores.turning == Catch::Approx(25.0));
    CHECK(result.component_scores.route_anomaly == Catch::Approx(80.0));
    CHECK(result.total_score == Catch::Approx(63.0));
    CHECK(result.risk_band == oceanwatchai::RiskBand::High);
    CHECK(has_explanation_containing(result, "slow repeated turning behaviour"));
    CHECK(has_explanation_containing(result, "AIS transmission gap exceeded 6 hours"));
    CHECK(has_explanation_containing(result, "1-5 knot low-speed band"));
    CHECK(has_explanation_containing(result, "Route-anomaly proxy"));
}

TEST_CASE("Risk scoring reaches critical when all components are maximal")
{
    auto features = base_features();
    features.fraction_low_speed = 1.0;
    features.fraction_high_turning = 1.0;
    features.max_time_gap_hours = 12.0;
    features.number_of_ais_gaps = 3;
    features.loitering_score = 1.0;
    features.suspicious_manoeuvre_score = 1.0;

    const oceanwatchai::RiskScoringEngine scorer;
    const auto result = scorer.score(features);

    CHECK(result.component_scores.loitering == Catch::Approx(100.0));
    CHECK(result.component_scores.AIS_gap == Catch::Approx(100.0));
    CHECK(result.component_scores.low_speed == Catch::Approx(100.0));
    CHECK(result.component_scores.turning == Catch::Approx(100.0));
    CHECK(result.component_scores.route_anomaly == Catch::Approx(100.0));
    CHECK(result.total_score == Catch::Approx(100.0));
    CHECK(result.risk_band == oceanwatchai::RiskBand::Critical);
    CHECK(oceanwatchai::risk_band_to_string(result.risk_band) == "Critical");
}

TEST_CASE("Custom risk weights alter composite score")
{
    auto features = base_features();
    features.fraction_low_speed = 0.5;
    features.fraction_high_turning = 0.25;
    features.max_time_gap_hours = 7.0;
    features.number_of_ais_gaps = 1;
    features.loitering_score = 1.0;
    features.suspicious_manoeuvre_score = 0.8;

    auto config = oceanwatchai::default_analysis_config();
    config.risk_scoring.loitering_weight = 0.0;
    config.risk_scoring.ais_gap_weight = 0.0;
    config.risk_scoring.low_speed_weight = 0.0;
    config.risk_scoring.turning_weight = 0.0;
    config.risk_scoring.route_anomaly_weight = 1.0;

    const oceanwatchai::RiskScoringEngine scorer{config.risk_scoring, config.risk_bands};
    const auto result = scorer.score(features);

    CHECK(result.component_scores.route_anomaly == Catch::Approx(80.0));
    CHECK(result.total_score == Catch::Approx(80.0));
    CHECK(result.risk_band == oceanwatchai::RiskBand::Critical);
}

TEST_CASE("Risk scoring uses deterministic band thresholds")
{
    const oceanwatchai::RiskScoringEngine scorer;

    auto low_features = base_features();
    low_features.loitering_score = 0.8;
    CHECK(scorer.score(low_features).total_score == Catch::Approx(24.0));
    CHECK(scorer.score(low_features).risk_band == oceanwatchai::RiskBand::Low);

    auto medium_features = base_features();
    medium_features.loitering_score = 25.0 / 30.0;
    CHECK(scorer.score(medium_features).total_score == Catch::Approx(25.0));
    CHECK(scorer.score(medium_features).risk_band == oceanwatchai::RiskBand::Medium);

    auto high_features = base_features();
    high_features.loitering_score = 1.0;
    high_features.fraction_high_turning = 1.0;
    CHECK(scorer.score(high_features).total_score == Catch::Approx(50.0));
    CHECK(scorer.score(high_features).risk_band == oceanwatchai::RiskBand::High);

    auto critical_features = high_features;
    critical_features.fraction_low_speed = 1.0;
    critical_features.suspicious_manoeuvre_score = 1.0;
    CHECK(scorer.score(critical_features).total_score == Catch::Approx(75.0));
    CHECK(scorer.score(critical_features).risk_band == oceanwatchai::RiskBand::Critical);
}

TEST_CASE("Custom risk bands alter risk classification")
{
    auto config = oceanwatchai::default_analysis_config();
    config.risk_bands.low_upper_bound = 10.0;
    config.risk_bands.medium_upper_bound = 20.0;
    config.risk_bands.high_upper_bound = 30.0;

    auto features = base_features();
    features.loitering_score = 0.8;

    const oceanwatchai::RiskScoringEngine scorer{config.risk_scoring, config.risk_bands};
    const auto result = scorer.score(features);

    CHECK(result.total_score == Catch::Approx(24.0));
    CHECK(result.risk_band == oceanwatchai::RiskBand::High);
}

TEST_CASE("Risk scoring explains moderate AIS gaps")
{
    auto features = base_features();
    features.max_time_gap_hours = 3.0;
    features.number_of_ais_gaps = 1;

    const oceanwatchai::RiskScoringEngine scorer;
    const auto result = scorer.score(features);

    CHECK(result.component_scores.AIS_gap == Catch::Approx(100.0 / 3.0));
    CHECK(result.risk_band == oceanwatchai::RiskBand::Low);
    CHECK(has_explanation_containing(result, "AIS gaps above 2 hours"));
}

TEST_CASE("Risk scoring rejects invalid feature values")
{
    auto features = base_features();
    features.fraction_low_speed = 1.1;

    const oceanwatchai::RiskScoringEngine scorer;

    CHECK_THROWS_AS(scorer.score(features), std::invalid_argument);
}

TEST_CASE("Risk scorer rejects invalid config")
{
    auto config = oceanwatchai::default_analysis_config();
    config.risk_scoring.route_anomaly_weight = 0.25;
    CHECK_THROWS_AS(
        (oceanwatchai::RiskScoringEngine{config.risk_scoring, config.risk_bands}),
        oceanwatchai::AnalysisConfigError);

    config = oceanwatchai::default_analysis_config();
    config.risk_bands.medium_upper_bound = config.risk_bands.low_upper_bound;
    CHECK_THROWS_AS(
        (oceanwatchai::RiskScoringEngine{config.risk_scoring, config.risk_bands}),
        oceanwatchai::AnalysisConfigError);
}
