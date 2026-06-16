#pragma once

#include "oceanwatchai/TrackFeatures.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace oceanwatchai {

enum class AnomalyDetectionMethod {
    ZScore,
    MedianAbsoluteDeviation,
};

struct FeatureAnomalyFlag {
    std::string feature_name;
    double value{};
    double baseline_location{};
    double baseline_scale{};
    double normalized_deviation{};
    double contribution_score{};
    bool is_anomaly{};
    std::string explanation;
};

struct AnomalyDetectionResult {
    std::string vessel_id;
    double anomaly_score{};
    AnomalyDetectionMethod method{AnomalyDetectionMethod::ZScore};
    std::vector<FeatureAnomalyFlag> feature_flags;
    std::vector<std::string> explanations;

    [[nodiscard]] const FeatureAnomalyFlag* find_feature(std::string_view feature_name) const noexcept;
};

[[nodiscard]] std::string anomaly_method_to_string(AnomalyDetectionMethod method);

// Lightweight fleet-baseline anomaly detector for numeric TrackFeatures.
class StatisticalAnomalyDetector {
public:
    explicit StatisticalAnomalyDetector(double z_score_threshold = 2.0, double mad_threshold = 3.5);

    [[nodiscard]] AnomalyDetectionResult score_against_baseline(
        const TrackFeatures& vessel,
        const std::vector<TrackFeatures>& fleet_baseline,
        AnomalyDetectionMethod method) const;

    [[nodiscard]] std::vector<AnomalyDetectionResult> score_fleet(
        const std::vector<TrackFeatures>& fleet,
        AnomalyDetectionMethod method) const;

private:
    double z_score_threshold_;
    double mad_threshold_;
};

} // namespace oceanwatchai
