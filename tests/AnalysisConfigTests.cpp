#include "oceanwatchai/AnalysisConfig.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>

namespace {

void check_default_values(const oceanwatchai::AnalysisConfig& config)
{
    CHECK(config.feature_extraction.low_speed_min_knots == Catch::Approx(1.0));
    CHECK(config.feature_extraction.low_speed_max_knots == Catch::Approx(5.0));
    CHECK(config.feature_extraction.high_turning_threshold_deg == Catch::Approx(45.0));
    CHECK(config.feature_extraction.ais_gap_threshold_hours == Catch::Approx(2.0));
    CHECK(config.feature_extraction.suspicious_manoeuvre_turning_weight == Catch::Approx(0.7));
    CHECK(config.feature_extraction.suspicious_manoeuvre_gap_weight == Catch::Approx(0.3));

    CHECK(config.risk_scoring.loitering_weight == Catch::Approx(0.30));
    CHECK(config.risk_scoring.ais_gap_weight == Catch::Approx(0.25));
    CHECK(config.risk_scoring.low_speed_weight == Catch::Approx(0.15));
    CHECK(config.risk_scoring.turning_weight == Catch::Approx(0.20));
    CHECK(config.risk_scoring.route_anomaly_weight == Catch::Approx(0.10));

    CHECK(config.risk_scoring.ais_gap_threshold_hours == Catch::Approx(2.0));
    CHECK(config.risk_scoring.severe_ais_gap_hours == Catch::Approx(6.0));
    CHECK(config.risk_scoring.max_gap_scale_hours == Catch::Approx(10.0));
    CHECK(config.risk_scoring.gap_count_scale == Catch::Approx(3.0));
    CHECK(config.risk_scoring.elevated_component_threshold == Catch::Approx(50.0));

    CHECK(config.risk_bands.low_upper_bound == Catch::Approx(25.0));
    CHECK(config.risk_bands.medium_upper_bound == Catch::Approx(50.0));
    CHECK(config.risk_bands.high_upper_bound == Catch::Approx(75.0));

    CHECK(config.protected_area.near_buffer_km == Catch::Approx(5.0));
    CHECK(config.protected_area.loitering_time_threshold_hours == Catch::Approx(1.0));
    CHECK(config.protected_area.inside_score_per_hour == Catch::Approx(50.0));
    CHECK(config.protected_area.near_score_per_hour == Catch::Approx(25.0));
}

} // namespace

TEST_CASE("Default analysis config matches current hardcoded behaviour")
{
    const auto config = oceanwatchai::default_analysis_config();
    check_default_values(config);
    CHECK_NOTHROW(oceanwatchai::validate_analysis_config(config));
}

TEST_CASE("Default risk weights sum to one")
{
    const auto config = oceanwatchai::default_analysis_config();
    const auto weight_sum = config.risk_scoring.loitering_weight + config.risk_scoring.ais_gap_weight +
                            config.risk_scoring.low_speed_weight + config.risk_scoring.turning_weight +
                            config.risk_scoring.route_anomaly_weight;

    CHECK(weight_sum == Catch::Approx(1.0));
}

TEST_CASE("Default risk band thresholds are strictly ordered")
{
    const auto config = oceanwatchai::default_analysis_config();

    CHECK(config.risk_bands.low_upper_bound < config.risk_bands.medium_upper_bound);
    CHECK(config.risk_bands.medium_upper_bound < config.risk_bands.high_upper_bound);
    CHECK(config.risk_bands.high_upper_bound <= 100.0);
}

TEST_CASE("Analysis config validation rejects unsafe threshold values")
{
    auto config = oceanwatchai::default_analysis_config();

    config.feature_extraction.high_turning_threshold_deg = 181.0;
    CHECK_THROWS_AS(oceanwatchai::validate_analysis_config(config), oceanwatchai::AnalysisConfigError);

    config = oceanwatchai::default_analysis_config();
    config.risk_scoring.max_gap_scale_hours = 0.0;
    CHECK_THROWS_AS(oceanwatchai::validate_analysis_config(config), oceanwatchai::AnalysisConfigError);

    config = oceanwatchai::default_analysis_config();
    config.protected_area.near_buffer_km = -1.0;
    CHECK_THROWS_AS(oceanwatchai::validate_analysis_config(config), oceanwatchai::AnalysisConfigError);
}

TEST_CASE("Analysis config validation rejects invalid risk weights and band ordering")
{
    auto config = oceanwatchai::default_analysis_config();
    config.risk_scoring.route_anomaly_weight = 0.25;
    CHECK_THROWS_AS(oceanwatchai::validate_analysis_config(config), oceanwatchai::AnalysisConfigError);

    config = oceanwatchai::default_analysis_config();
    config.risk_bands.medium_upper_bound = config.risk_bands.low_upper_bound;
    CHECK_THROWS_AS(oceanwatchai::validate_analysis_config(config), oceanwatchai::AnalysisConfigError);
}

TEST_CASE("Default JSON config loads to the same defaults")
{
    const auto config_path = std::filesystem::path{OCEANWATCHAI_SOURCE_DIR} / "configs" / "default_config.json";
    const auto config = oceanwatchai::load_analysis_config_json(config_path);

    check_default_values(config);
}

TEST_CASE("JSON config loader supports partial overrides and validation")
{
    std::istringstream valid_input{
        R"({
            "feature_extraction": {
                "low_speed_max_knots": 4.5
            },
            "risk_bands": {
                "high_upper_bound": 80.0
            }
        })"
    };

    const auto config = oceanwatchai::load_analysis_config_json(valid_input);
    CHECK(config.feature_extraction.low_speed_min_knots == Catch::Approx(1.0));
    CHECK(config.feature_extraction.low_speed_max_knots == Catch::Approx(4.5));
    CHECK(config.risk_bands.high_upper_bound == Catch::Approx(80.0));

    std::istringstream invalid_input{R"({"risk_scoring": {"loitering_weight": 0.9}})"};
    CHECK_THROWS_AS(oceanwatchai::load_analysis_config_json(invalid_input), oceanwatchai::AnalysisConfigError);
}
