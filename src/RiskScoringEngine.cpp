#include "oceanwatchai/RiskScoringEngine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace oceanwatchai {
namespace {

[[nodiscard]] double clamp_score(double value)
{
    return std::clamp(value, 0.0, 100.0);
}

[[nodiscard]] double to_score(double unit_value)
{
    return clamp_score(unit_value * 100.0);
}

void require_finite(double value, const char* name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument{std::string{name} + " must be finite"};
    }
}

void require_non_negative(double value, const char* name)
{
    require_finite(value, name);
    if (value < 0.0) {
        throw std::invalid_argument{std::string{name} + " must be non-negative"};
    }
}

void require_unit_interval(double value, const char* name)
{
    require_finite(value, name);
    if (value < 0.0 || value > 1.0) {
        throw std::invalid_argument{std::string{name} + " must be in [0, 1]"};
    }
}

void validate_features(const TrackFeatures& features)
{
    require_non_negative(features.total_distance_km, "total_distance_km");
    require_non_negative(features.duration_hours, "duration_hours");
    require_non_negative(features.mean_speed_knots, "mean_speed_knots");
    require_non_negative(features.max_speed_knots, "max_speed_knots");
    require_unit_interval(features.fraction_low_speed, "fraction_low_speed");
    require_unit_interval(features.fraction_high_turning, "fraction_high_turning");
    require_non_negative(features.mean_turning_angle, "mean_turning_angle");
    require_non_negative(features.max_time_gap_hours, "max_time_gap_hours");
    require_unit_interval(features.loitering_score, "loitering_score");
    require_unit_interval(features.suspicious_manoeuvre_score, "suspicious_manoeuvre_score");
}

void validate_route_anomaly(const TrackFeatures& features, const AnomalyDetectionResult& route_anomaly)
{
    if (!route_anomaly.vessel_id.empty() && route_anomaly.vessel_id != features.vessel_id) {
        throw std::invalid_argument{"route anomaly result vessel_id does not match TrackFeatures vessel_id"};
    }

    require_finite(route_anomaly.anomaly_score, "route_anomaly.anomaly_score");
    if (route_anomaly.anomaly_score < 0.0 || route_anomaly.anomaly_score > 100.0) {
        throw std::invalid_argument{"route_anomaly.anomaly_score must be in [0, 100]"};
    }
}

[[nodiscard]] std::string format_hours(double hours)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << hours;
    return output.str();
}

[[nodiscard]] std::string format_compact_hours(double hours)
{
    std::ostringstream output;
    if (std::abs(hours - std::round(hours)) < 1.0e-9) {
        output << std::fixed << std::setprecision(0) << hours;
    } else {
        output << std::fixed << std::setprecision(1) << hours;
    }
    return output.str();
}

[[nodiscard]] double ais_gap_component_score(const TrackFeatures& features, const RiskScoringConfig& config)
{
    if (features.max_time_gap_hours <= config.ais_gap_threshold_hours && features.number_of_ais_gaps == 0) {
        return 0.0;
    }

    const auto gap_duration_score =
        to_score((features.max_time_gap_hours - config.ais_gap_threshold_hours) / config.max_gap_scale_hours);
    const auto gap_count_score = to_score(static_cast<double>(features.number_of_ais_gaps) / config.gap_count_scale);

    return std::max(gap_duration_score, gap_count_score);
}

[[nodiscard]] RiskBand band_for_score(double total_score, const RiskBandConfig& config)
{
    if (total_score < config.low_upper_bound) {
        return RiskBand::Low;
    }

    if (total_score < config.medium_upper_bound) {
        return RiskBand::Medium;
    }

    if (total_score < config.high_upper_bound) {
        return RiskBand::High;
    }

    return RiskBand::Critical;
}

[[nodiscard]] RiskScoreResult score_with_route_anomaly(
    const TrackFeatures& features,
    double route_anomaly_score,
    const std::vector<std::string>* route_anomaly_explanations,
    bool statistical_route_anomaly,
    const RiskScoringConfig& risk_config,
    const RiskBandConfig& band_config)
{
    RiskScoreResult result{
        .vessel_id = features.vessel_id,
    };

    result.component_scores = RiskComponentScores{
        .loitering = to_score(features.loitering_score),
        .AIS_gap = ais_gap_component_score(features, risk_config),
        .low_speed = to_score(features.fraction_low_speed),
        .turning = to_score(features.fraction_high_turning),
        .route_anomaly = clamp_score(route_anomaly_score),
    };

    result.total_score = clamp_score(
        risk_config.loitering_weight * result.component_scores.loitering +
        risk_config.ais_gap_weight * result.component_scores.AIS_gap +
        risk_config.low_speed_weight * result.component_scores.low_speed +
        risk_config.turning_weight * result.component_scores.turning +
        risk_config.route_anomaly_weight * result.component_scores.route_anomaly);
    result.risk_band = band_for_score(result.total_score, band_config);

    if (result.component_scores.loitering >= risk_config.elevated_component_threshold) {
        result.explanations.push_back(
            "Vessel displayed slow repeated turning behaviour consistent with loitering.");
    }

    if (features.max_time_gap_hours > risk_config.severe_ais_gap_hours) {
        result.explanations.push_back(
            "AIS transmission gap exceeded " + format_compact_hours(risk_config.severe_ais_gap_hours) +
            " hours; maximum observed gap was " +
            format_hours(features.max_time_gap_hours) + " hours.");
    } else if (features.number_of_ais_gaps > 0) {
        result.explanations.push_back(
            "AIS gaps above " + format_compact_hours(risk_config.ais_gap_threshold_hours) +
            " hours were detected in the track.");
    }

    if (result.component_scores.low_speed >= risk_config.elevated_component_threshold) {
        result.explanations.push_back("A large share of AIS points were in the 1-5 knot low-speed band.");
    }

    if (result.component_scores.turning >= risk_config.elevated_component_threshold) {
        result.explanations.push_back("Frequent heading changes above 45 degrees were detected.");
    }

    if (result.component_scores.route_anomaly >= risk_config.elevated_component_threshold) {
        if (statistical_route_anomaly) {
            result.explanations.push_back(
                "Route-anomaly component is elevated from fleet-baseline anomaly detection.");
        } else {
            result.explanations.push_back(
                "Route-anomaly proxy is elevated from combined manoeuvre and signal-gap indicators.");
        }
    }

    if (statistical_route_anomaly && route_anomaly_explanations != nullptr) {
        for (const auto& explanation : *route_anomaly_explanations) {
            result.explanations.push_back("Anomaly detector: " + explanation);
        }
    }

    if (result.explanations.empty()) {
        result.explanations.push_back("No elevated risk indicators triggered by the current rule set.");
    }

    return result;
}

} // namespace

std::string risk_band_to_string(RiskBand risk_band)
{
    switch (risk_band) {
    case RiskBand::Low:
        return "Low";
    case RiskBand::Medium:
        return "Medium";
    case RiskBand::High:
        return "High";
    case RiskBand::Critical:
        return "Critical";
    }

    return "Unknown";
}

RiskScoringEngine::RiskScoringEngine()
    : RiskScoringEngine(RiskScoringConfig{}, RiskBandConfig{})
{
}

RiskScoringEngine::RiskScoringEngine(const RiskScoringConfig& risk_config, const RiskBandConfig& band_config)
    : risk_config_{risk_config}
    , band_config_{band_config}
{
    validate_analysis_config(risk_config_);
    validate_analysis_config(band_config_);
}

RiskScoreResult RiskScoringEngine::score(const TrackFeatures& features) const
{
    validate_features(features);
    return score_with_route_anomaly(
        features,
        to_score(features.suspicious_manoeuvre_score),
        nullptr,
        false,
        risk_config_,
        band_config_);
}

RiskScoreResult RiskScoringEngine::score(
    const TrackFeatures& features,
    const AnomalyDetectionResult& route_anomaly) const
{
    validate_features(features);
    validate_route_anomaly(features, route_anomaly);
    return score_with_route_anomaly(
        features,
        route_anomaly.anomaly_score,
        &route_anomaly.explanations,
        true,
        risk_config_,
        band_config_);
}

} // namespace oceanwatchai
