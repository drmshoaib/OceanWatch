#include "oceanwatchai/AnalysisConfig.hpp"

#include <cmath>
#include <fstream>
#include <istream>
#include <nlohmann/json.hpp>
#include <string>

namespace oceanwatchai {
namespace {

constexpr double kWeightSumTolerance = 1.0e-9;

void require_finite(double value, const char* name)
{
    if (!std::isfinite(value)) {
        throw AnalysisConfigError{std::string{name} + " must be finite"};
    }
}

void require_non_negative(double value, const char* name)
{
    require_finite(value, name);
    if (value < 0.0) {
        throw AnalysisConfigError{std::string{name} + " must be non-negative"};
    }
}

void require_positive(double value, const char* name)
{
    require_finite(value, name);
    if (value <= 0.0) {
        throw AnalysisConfigError{std::string{name} + " must be positive"};
    }
}

void require_score_range(double value, const char* name)
{
    require_finite(value, name);
    if (value < 0.0 || value > 100.0) {
        throw AnalysisConfigError{std::string{name} + " must be in [0, 100]"};
    }
}

template <typename Config>
void assign_if_present(const nlohmann::json& section, const char* key, double Config::*member, Config& config)
{
    if (!section.contains(key)) {
        return;
    }

    if (!section.at(key).is_number()) {
        throw AnalysisConfigError{std::string{key} + " must be numeric"};
    }

    config.*member = section.at(key).get<double>();
}

void require_object_if_present(const nlohmann::json& root, const char* key)
{
    if (root.contains(key) && !root.at(key).is_object()) {
        throw AnalysisConfigError{std::string{key} + " must be an object"};
    }
}

void apply_feature_extraction_json(const nlohmann::json& root, AnalysisConfig& config)
{
    constexpr auto key = "feature_extraction";
    require_object_if_present(root, key);
    if (!root.contains(key)) {
        return;
    }

    const auto& section = root.at(key);
    assign_if_present(section, "low_speed_min_knots", &FeatureExtractionConfig::low_speed_min_knots, config.feature_extraction);
    assign_if_present(section, "low_speed_max_knots", &FeatureExtractionConfig::low_speed_max_knots, config.feature_extraction);
    assign_if_present(
        section,
        "high_turning_threshold_deg",
        &FeatureExtractionConfig::high_turning_threshold_deg,
        config.feature_extraction);
    assign_if_present(
        section,
        "ais_gap_threshold_hours",
        &FeatureExtractionConfig::ais_gap_threshold_hours,
        config.feature_extraction);
    assign_if_present(
        section,
        "suspicious_manoeuvre_turning_weight",
        &FeatureExtractionConfig::suspicious_manoeuvre_turning_weight,
        config.feature_extraction);
    assign_if_present(
        section,
        "suspicious_manoeuvre_gap_weight",
        &FeatureExtractionConfig::suspicious_manoeuvre_gap_weight,
        config.feature_extraction);
}

void apply_risk_scoring_json(const nlohmann::json& root, AnalysisConfig& config)
{
    constexpr auto key = "risk_scoring";
    require_object_if_present(root, key);
    if (!root.contains(key)) {
        return;
    }

    const auto& section = root.at(key);
    assign_if_present(section, "loitering_weight", &RiskScoringConfig::loitering_weight, config.risk_scoring);
    assign_if_present(section, "ais_gap_weight", &RiskScoringConfig::ais_gap_weight, config.risk_scoring);
    assign_if_present(section, "low_speed_weight", &RiskScoringConfig::low_speed_weight, config.risk_scoring);
    assign_if_present(section, "turning_weight", &RiskScoringConfig::turning_weight, config.risk_scoring);
    assign_if_present(section, "route_anomaly_weight", &RiskScoringConfig::route_anomaly_weight, config.risk_scoring);
    assign_if_present(section, "ais_gap_threshold_hours", &RiskScoringConfig::ais_gap_threshold_hours, config.risk_scoring);
    assign_if_present(section, "severe_ais_gap_hours", &RiskScoringConfig::severe_ais_gap_hours, config.risk_scoring);
    assign_if_present(section, "max_gap_scale_hours", &RiskScoringConfig::max_gap_scale_hours, config.risk_scoring);
    assign_if_present(section, "gap_count_scale", &RiskScoringConfig::gap_count_scale, config.risk_scoring);
    assign_if_present(
        section,
        "elevated_component_threshold",
        &RiskScoringConfig::elevated_component_threshold,
        config.risk_scoring);
}

void apply_risk_bands_json(const nlohmann::json& root, AnalysisConfig& config)
{
    constexpr auto key = "risk_bands";
    require_object_if_present(root, key);
    if (!root.contains(key)) {
        return;
    }

    const auto& section = root.at(key);
    assign_if_present(section, "low_upper_bound", &RiskBandConfig::low_upper_bound, config.risk_bands);
    assign_if_present(section, "medium_upper_bound", &RiskBandConfig::medium_upper_bound, config.risk_bands);
    assign_if_present(section, "high_upper_bound", &RiskBandConfig::high_upper_bound, config.risk_bands);
}

void apply_protected_area_json(const nlohmann::json& root, AnalysisConfig& config)
{
    constexpr auto key = "protected_area";
    require_object_if_present(root, key);
    if (!root.contains(key)) {
        return;
    }

    const auto& section = root.at(key);
    assign_if_present(section, "near_buffer_km", &ProtectedAreaConfig::near_buffer_km, config.protected_area);
    assign_if_present(
        section,
        "loitering_time_threshold_hours",
        &ProtectedAreaConfig::loitering_time_threshold_hours,
        config.protected_area);
    assign_if_present(section, "inside_score_per_hour", &ProtectedAreaConfig::inside_score_per_hour, config.protected_area);
    assign_if_present(section, "near_score_per_hour", &ProtectedAreaConfig::near_score_per_hour, config.protected_area);
}

} // namespace

AnalysisConfig default_analysis_config()
{
    AnalysisConfig config;
    validate_analysis_config(config);
    return config;
}

void validate_analysis_config(const FeatureExtractionConfig& config)
{
    require_non_negative(config.low_speed_min_knots, "feature_extraction.low_speed_min_knots");
    require_non_negative(config.low_speed_max_knots, "feature_extraction.low_speed_max_knots");
    if (config.low_speed_max_knots < config.low_speed_min_knots) {
        throw AnalysisConfigError{"feature_extraction low-speed thresholds must satisfy min <= max"};
    }

    require_finite(config.high_turning_threshold_deg, "feature_extraction.high_turning_threshold_deg");
    if (config.high_turning_threshold_deg < 0.0 || config.high_turning_threshold_deg > 180.0) {
        throw AnalysisConfigError{"feature_extraction.high_turning_threshold_deg must be in [0, 180]"};
    }

    require_non_negative(config.ais_gap_threshold_hours, "feature_extraction.ais_gap_threshold_hours");
    require_non_negative(
        config.suspicious_manoeuvre_turning_weight,
        "feature_extraction.suspicious_manoeuvre_turning_weight");
    require_non_negative(config.suspicious_manoeuvre_gap_weight, "feature_extraction.suspicious_manoeuvre_gap_weight");
    if ((config.suspicious_manoeuvre_turning_weight + config.suspicious_manoeuvre_gap_weight) <= 0.0) {
        throw AnalysisConfigError{"feature_extraction suspicious manoeuvre weights must have positive total"};
    }
}

void validate_analysis_config(const RiskScoringConfig& config)
{
    require_non_negative(config.loitering_weight, "risk_scoring.loitering_weight");
    require_non_negative(config.ais_gap_weight, "risk_scoring.ais_gap_weight");
    require_non_negative(config.low_speed_weight, "risk_scoring.low_speed_weight");
    require_non_negative(config.turning_weight, "risk_scoring.turning_weight");
    require_non_negative(config.route_anomaly_weight, "risk_scoring.route_anomaly_weight");

    const auto weight_sum = config.loitering_weight + config.ais_gap_weight + config.low_speed_weight +
                            config.turning_weight + config.route_anomaly_weight;
    if (std::abs(weight_sum - 1.0) > kWeightSumTolerance) {
        throw AnalysisConfigError{"risk_scoring weights must sum to 1.0"};
    }

    require_non_negative(config.ais_gap_threshold_hours, "risk_scoring.ais_gap_threshold_hours");
    require_non_negative(config.severe_ais_gap_hours, "risk_scoring.severe_ais_gap_hours");
    if (config.severe_ais_gap_hours < config.ais_gap_threshold_hours) {
        throw AnalysisConfigError{"risk_scoring.severe_ais_gap_hours must be >= ais_gap_threshold_hours"};
    }

    require_positive(config.max_gap_scale_hours, "risk_scoring.max_gap_scale_hours");
    require_positive(config.gap_count_scale, "risk_scoring.gap_count_scale");
    require_score_range(config.elevated_component_threshold, "risk_scoring.elevated_component_threshold");
}

void validate_analysis_config(const RiskBandConfig& config)
{
    require_score_range(config.low_upper_bound, "risk_bands.low_upper_bound");
    require_score_range(config.medium_upper_bound, "risk_bands.medium_upper_bound");
    require_score_range(config.high_upper_bound, "risk_bands.high_upper_bound");

    if (!(config.low_upper_bound < config.medium_upper_bound &&
          config.medium_upper_bound < config.high_upper_bound)) {
        throw AnalysisConfigError{"risk band upper bounds must be strictly increasing"};
    }
}

void validate_analysis_config(const ProtectedAreaConfig& config)
{
    require_non_negative(config.near_buffer_km, "protected_area.near_buffer_km");
    require_non_negative(config.loitering_time_threshold_hours, "protected_area.loitering_time_threshold_hours");
    require_non_negative(config.inside_score_per_hour, "protected_area.inside_score_per_hour");
    require_non_negative(config.near_score_per_hour, "protected_area.near_score_per_hour");
}

void validate_analysis_config(const AnalysisConfig& config)
{
    validate_analysis_config(config.feature_extraction);
    validate_analysis_config(config.risk_scoring);
    validate_analysis_config(config.risk_bands);
    validate_analysis_config(config.protected_area);
}

AnalysisConfig load_analysis_config_json(std::istream& input)
{
    nlohmann::json root;
    try {
        input >> root;
    } catch (const nlohmann::json::exception& error) {
        throw AnalysisConfigError{std::string{"Invalid analysis config JSON: "} + error.what()};
    }

    if (!root.is_object()) {
        throw AnalysisConfigError{"Analysis config JSON must be an object"};
    }

    auto config = default_analysis_config();
    apply_feature_extraction_json(root, config);
    apply_risk_scoring_json(root, config);
    apply_risk_bands_json(root, config);
    apply_protected_area_json(root, config);
    validate_analysis_config(config);
    return config;
}

AnalysisConfig load_analysis_config_json(const std::filesystem::path& file_path)
{
    std::ifstream input{file_path};
    if (!input) {
        throw AnalysisConfigError{"Unable to open analysis config file: " + file_path.string()};
    }

    return load_analysis_config_json(input);
}

} // namespace oceanwatchai
