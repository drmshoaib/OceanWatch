#include "oceanwatchai/StatisticalAnomalyDetector.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace oceanwatchai {
namespace {

constexpr double kZeroScaleDeviationMultiplier = 2.0;
constexpr std::size_t kContributionFeatureCount = 3;

struct NumericFeature {
    const char* name;
    double (*value)(const TrackFeatures&);
};

[[nodiscard]] const std::vector<NumericFeature>& numeric_features()
{
    static const std::vector<NumericFeature> features{
        {"total_distance_km", [](const TrackFeatures& features) { return features.total_distance_km; }},
        {"duration_hours", [](const TrackFeatures& features) { return features.duration_hours; }},
        {"mean_speed_knots", [](const TrackFeatures& features) { return features.mean_speed_knots; }},
        {"max_speed_knots", [](const TrackFeatures& features) { return features.max_speed_knots; }},
        {"fraction_low_speed", [](const TrackFeatures& features) { return features.fraction_low_speed; }},
        {"fraction_high_turning", [](const TrackFeatures& features) { return features.fraction_high_turning; }},
        {"mean_turning_angle", [](const TrackFeatures& features) { return features.mean_turning_angle; }},
        {"max_time_gap_hours", [](const TrackFeatures& features) { return features.max_time_gap_hours; }},
        {"number_of_ais_gaps", [](const TrackFeatures& features) {
             return static_cast<double>(features.number_of_ais_gaps);
         }},
        {"loitering_score", [](const TrackFeatures& features) { return features.loitering_score; }},
        {"suspicious_manoeuvre_score", [](const TrackFeatures& features) {
             return features.suspicious_manoeuvre_score;
         }},
    };

    return features;
}

void require_finite(double value, const char* name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument{std::string{name} + " must be finite"};
    }
}

[[nodiscard]] double clamp_score(double value)
{
    return std::clamp(value, 0.0, 100.0);
}

[[nodiscard]] double mean(const std::vector<double>& values)
{
    return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
}

[[nodiscard]] double population_standard_deviation(const std::vector<double>& values, double location)
{
    const auto squared_sum = std::accumulate(values.begin(), values.end(), 0.0, [location](double total, double value) {
        const auto delta = value - location;
        return total + delta * delta;
    });

    return std::sqrt(squared_sum / static_cast<double>(values.size()));
}

[[nodiscard]] double median(std::vector<double> values)
{
    if (values.empty()) {
        throw std::invalid_argument{"median requires at least one value"};
    }

    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if (values.size() % 2 == 1) {
        return values[middle];
    }

    return (values[middle - 1] + values[middle]) / 2.0;
}

[[nodiscard]] double median_absolute_deviation(const std::vector<double>& values, double location)
{
    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (const auto value : values) {
        deviations.push_back(std::abs(value - location));
    }

    return median(std::move(deviations));
}

[[nodiscard]] std::vector<double> baseline_values(
    const std::vector<TrackFeatures>& baseline,
    const NumericFeature& feature)
{
    std::vector<double> values;
    values.reserve(baseline.size());

    for (const auto& vessel_features : baseline) {
        const auto value = feature.value(vessel_features);
        require_finite(value, feature.name);
        values.push_back(value);
    }

    return values;
}

[[nodiscard]] double zero_scale_deviation(double value, double location, double threshold)
{
    return value == location ? 0.0 : threshold * kZeroScaleDeviationMultiplier;
}

[[nodiscard]] double normalized_deviation(
    double value,
    double location,
    double scale,
    double threshold,
    AnomalyDetectionMethod method)
{
    if (scale == 0.0) {
        return zero_scale_deviation(value, location, threshold);
    }

    const auto raw_deviation = std::abs(value - location) / scale;
    if (method == AnomalyDetectionMethod::MedianAbsoluteDeviation) {
        return 0.6745 * raw_deviation;
    }

    return raw_deviation;
}

[[nodiscard]] std::string build_feature_explanation(
    const FeatureAnomalyFlag& flag,
    AnomalyDetectionMethod method)
{
    std::ostringstream output;
    output << flag.feature_name << " was anomalous under " << anomaly_method_to_string(method)
           << " detection: value " << flag.value << ", baseline " << flag.baseline_location
           << ", normalized deviation " << flag.normalized_deviation << ".";
    return output.str();
}

[[nodiscard]] double aggregate_score(const std::vector<FeatureAnomalyFlag>& flags)
{
    std::vector<double> contributions;
    contributions.reserve(flags.size());
    for (const auto& flag : flags) {
        contributions.push_back(flag.contribution_score);
    }

    std::sort(contributions.begin(), contributions.end(), std::greater<>{});

    const auto count = std::min(kContributionFeatureCount, contributions.size());
    if (count == 0) {
        return 0.0;
    }

    const auto top_sum = std::accumulate(contributions.begin(), contributions.begin() + count, 0.0);
    return clamp_score(top_sum / static_cast<double>(count));
}

[[nodiscard]] double threshold_for(AnomalyDetectionMethod method, double z_score_threshold, double mad_threshold)
{
    return method == AnomalyDetectionMethod::MedianAbsoluteDeviation ? mad_threshold : z_score_threshold;
}

} // namespace

const FeatureAnomalyFlag* AnomalyDetectionResult::find_feature(std::string_view feature_name) const noexcept
{
    for (const auto& flag : feature_flags) {
        const auto flag_name = std::string_view{flag.feature_name.data(), flag.feature_name.size()};
        if (flag_name == feature_name) {
            return &flag;
        }
    }

    return nullptr;
}

std::string anomaly_method_to_string(AnomalyDetectionMethod method)
{
    switch (method) {
    case AnomalyDetectionMethod::ZScore:
        return "z-score";
    case AnomalyDetectionMethod::MedianAbsoluteDeviation:
        return "median absolute deviation";
    }

    return "unknown";
}

StatisticalAnomalyDetector::StatisticalAnomalyDetector(double z_score_threshold, double mad_threshold)
    : z_score_threshold_{z_score_threshold}
    , mad_threshold_{mad_threshold}
{
    require_finite(z_score_threshold_, "z_score_threshold");
    require_finite(mad_threshold_, "mad_threshold");

    if (z_score_threshold_ <= 0.0 || mad_threshold_ <= 0.0) {
        throw std::invalid_argument{"anomaly thresholds must be positive"};
    }
}

AnomalyDetectionResult StatisticalAnomalyDetector::score_against_baseline(
    const TrackFeatures& vessel,
    const std::vector<TrackFeatures>& fleet_baseline,
    AnomalyDetectionMethod method) const
{
    if (fleet_baseline.empty()) {
        throw std::invalid_argument{"fleet baseline must contain at least one vessel"};
    }

    AnomalyDetectionResult result{
        .vessel_id = vessel.vessel_id,
        .method = method,
    };

    const auto threshold = threshold_for(method, z_score_threshold_, mad_threshold_);

    for (const auto& feature : numeric_features()) {
        const auto values = baseline_values(fleet_baseline, feature);
        const auto value = feature.value(vessel);
        require_finite(value, feature.name);

        const auto location = method == AnomalyDetectionMethod::MedianAbsoluteDeviation ? median(values) : mean(values);
        const auto scale = method == AnomalyDetectionMethod::MedianAbsoluteDeviation
                               ? median_absolute_deviation(values, location)
                               : population_standard_deviation(values, location);
        const auto deviation = normalized_deviation(value, location, scale, threshold, method);
        const auto contribution = clamp_score((deviation / threshold) * 100.0);
        const auto is_anomaly = deviation >= threshold;

        FeatureAnomalyFlag flag{
            .feature_name = feature.name,
            .value = value,
            .baseline_location = location,
            .baseline_scale = scale,
            .normalized_deviation = deviation,
            .contribution_score = contribution,
            .is_anomaly = is_anomaly,
        };

        if (is_anomaly) {
            flag.explanation = build_feature_explanation(flag, method);
            result.explanations.push_back(flag.explanation);
        }

        result.feature_flags.push_back(std::move(flag));
    }

    result.anomaly_score = aggregate_score(result.feature_flags);
    if (result.explanations.empty()) {
        result.explanations.push_back("No feature exceeded the fleet-baseline anomaly threshold.");
    }

    return result;
}

std::vector<AnomalyDetectionResult> StatisticalAnomalyDetector::score_fleet(
    const std::vector<TrackFeatures>& fleet,
    AnomalyDetectionMethod method) const
{
    if (fleet.size() < 2) {
        throw std::invalid_argument{"fleet scoring requires at least two vessels"};
    }

    std::vector<AnomalyDetectionResult> results;
    results.reserve(fleet.size());

    for (std::size_t index = 0; index < fleet.size(); ++index) {
        std::vector<TrackFeatures> baseline;
        baseline.reserve(fleet.size() - 1);

        for (std::size_t baseline_index = 0; baseline_index < fleet.size(); ++baseline_index) {
            if (baseline_index != index) {
                baseline.push_back(fleet[baseline_index]);
            }
        }

        results.push_back(score_against_baseline(fleet[index], baseline, method));
    }

    return results;
}

} // namespace oceanwatchai
