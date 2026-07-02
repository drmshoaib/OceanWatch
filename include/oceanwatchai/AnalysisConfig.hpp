#pragma once

#include <filesystem>
#include <iosfwd>
#include <stdexcept>

namespace oceanwatchai {

class AnalysisConfigError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct FeatureExtractionConfig {
    double low_speed_min_knots{1.0};
    double low_speed_max_knots{5.0};
    double high_turning_threshold_deg{45.0};
    double ais_gap_threshold_hours{2.0};
    double suspicious_manoeuvre_turning_weight{0.7};
    double suspicious_manoeuvre_gap_weight{0.3};
};

struct RiskScoringConfig {
    double loitering_weight{0.30};
    double ais_gap_weight{0.25};
    double low_speed_weight{0.15};
    double turning_weight{0.20};
    double route_anomaly_weight{0.10};

    double ais_gap_threshold_hours{2.0};
    double severe_ais_gap_hours{6.0};
    double max_gap_scale_hours{10.0};
    double gap_count_scale{3.0};
    double elevated_component_threshold{50.0};
};

struct RiskBandConfig {
    double low_upper_bound{25.0};
    double medium_upper_bound{50.0};
    double high_upper_bound{75.0};
};

struct ProtectedAreaConfig {
    double near_buffer_km{5.0};
    double loitering_time_threshold_hours{1.0};
    double inside_score_per_hour{50.0};
    double near_score_per_hour{25.0};
};

struct AnalysisConfig {
    FeatureExtractionConfig feature_extraction;
    RiskScoringConfig risk_scoring;
    RiskBandConfig risk_bands;
    ProtectedAreaConfig protected_area;
};

[[nodiscard]] AnalysisConfig default_analysis_config();

void validate_analysis_config(const FeatureExtractionConfig& config);
void validate_analysis_config(const RiskScoringConfig& config);
void validate_analysis_config(const RiskBandConfig& config);
void validate_analysis_config(const ProtectedAreaConfig& config);
void validate_analysis_config(const AnalysisConfig& config);

[[nodiscard]] AnalysisConfig load_analysis_config_json(std::istream& input);
[[nodiscard]] AnalysisConfig load_analysis_config_json(const std::filesystem::path& file_path);

} // namespace oceanwatchai
