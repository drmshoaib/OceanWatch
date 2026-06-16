#pragma once

#include "oceanwatchai/StatisticalAnomalyDetector.hpp"
#include "oceanwatchai/TrackFeatures.hpp"

#include <string>
#include <vector>

namespace oceanwatchai {

enum class RiskBand {
    Low,
    Medium,
    High,
    Critical,
};

struct RiskComponentScores {
    double loitering{};
    double AIS_gap{};
    double low_speed{};
    double turning{};
    double route_anomaly{};
};

struct RiskScoreResult {
    std::string vessel_id;
    double total_score{};
    RiskBand risk_band{RiskBand::Low};
    std::vector<std::string> explanations;
    RiskComponentScores component_scores;
};

[[nodiscard]] std::string risk_band_to_string(RiskBand risk_band);

// Transparent rule-based risk scorer for engineered vessel-track features.
class RiskScoringEngine {
public:
    [[nodiscard]] RiskScoreResult score(const TrackFeatures& features) const;
    [[nodiscard]] RiskScoreResult score(const TrackFeatures& features, const AnomalyDetectionResult& route_anomaly) const;
};

} // namespace oceanwatchai
