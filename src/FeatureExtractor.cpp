#include "oceanwatchai/FeatureExtractor.hpp"

#include "oceanwatchai/GeospatialUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>

namespace oceanwatchai {
namespace {

[[nodiscard]] double clamp_score(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

void require_finite(double value, const char* name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument{std::string{name} + " must be finite"};
    }
}

} // namespace

FeatureExtractor::FeatureExtractor(
    double low_speed_min_knots,
    double low_speed_max_knots,
    double high_turning_threshold_deg,
    double ais_gap_threshold_hours)
    : low_speed_min_knots_{low_speed_min_knots}
    , low_speed_max_knots_{low_speed_max_knots}
    , high_turning_threshold_deg_{high_turning_threshold_deg}
    , ais_gap_threshold_hours_{ais_gap_threshold_hours}
    , suspicious_manoeuvre_turning_weight_{FeatureExtractionConfig{}.suspicious_manoeuvre_turning_weight}
    , suspicious_manoeuvre_gap_weight_{FeatureExtractionConfig{}.suspicious_manoeuvre_gap_weight}
{
    require_finite(low_speed_min_knots_, "low_speed_min_knots");
    require_finite(low_speed_max_knots_, "low_speed_max_knots");
    require_finite(high_turning_threshold_deg_, "high_turning_threshold_deg");
    require_finite(ais_gap_threshold_hours_, "ais_gap_threshold_hours");

    if (low_speed_min_knots_ < 0.0 || low_speed_max_knots_ < low_speed_min_knots_) {
        throw std::invalid_argument{"low-speed thresholds must satisfy 0 <= min <= max"};
    }

    if (high_turning_threshold_deg_ < 0.0 || high_turning_threshold_deg_ > 180.0) {
        throw std::invalid_argument{"high-turning threshold must be in [0, 180]"};
    }

    if (ais_gap_threshold_hours_ < 0.0) {
        throw std::invalid_argument{"AIS gap threshold must be non-negative"};
    }
}

FeatureExtractor::FeatureExtractor(const FeatureExtractionConfig& config)
    : low_speed_min_knots_{config.low_speed_min_knots}
    , low_speed_max_knots_{config.low_speed_max_knots}
    , high_turning_threshold_deg_{config.high_turning_threshold_deg}
    , ais_gap_threshold_hours_{config.ais_gap_threshold_hours}
    , suspicious_manoeuvre_turning_weight_{config.suspicious_manoeuvre_turning_weight}
    , suspicious_manoeuvre_gap_weight_{config.suspicious_manoeuvre_gap_weight}
{
    validate_analysis_config(config);
}

TrackFeatures FeatureExtractor::extract(const VesselTrack& track) const
{
    TrackFeatures features{
        .vessel_id = track.vessel_id(),
    };

    const auto& points = track.points();
    if (points.empty()) {
        return features;
    }

    const auto low_speed_count = std::count_if(points.begin(), points.end(), [this](const AISPoint& point) {
        require_finite(point.speed_knots, "speed_knots");
        return point.speed_knots >= low_speed_min_knots_ && point.speed_knots <= low_speed_max_knots_;
    });

    const auto speed_sum = std::accumulate(points.begin(), points.end(), 0.0, [](double total, const AISPoint& point) {
        return total + point.speed_knots;
    });

    const auto max_speed = std::max_element(points.begin(), points.end(), [](const AISPoint& lhs, const AISPoint& rhs) {
        return lhs.speed_knots < rhs.speed_knots;
    });

    features.mean_speed_knots = speed_sum / static_cast<double>(points.size());
    features.max_speed_knots = max_speed->speed_knots;
    features.fraction_low_speed = static_cast<double>(low_speed_count) / static_cast<double>(points.size());

    if (points.size() == 1) {
        return features;
    }

    features.duration_hours = time_difference_hours(points.front(), points.back());
    if (features.duration_hours < 0.0) {
        throw std::invalid_argument{"track timestamps must be sorted in ascending order"};
    }

    double turning_sum = 0.0;
    std::size_t high_turning_count = 0;
    std::size_t transition_count = 0;

    for (std::size_t index = 1; index < points.size(); ++index) {
        const auto& previous = points[index - 1];
        const auto& current = points[index];
        const auto time_gap_hours = time_difference_hours(previous, current);

        if (time_gap_hours < 0.0) {
            throw std::invalid_argument{"track timestamps must be sorted in ascending order"};
        }

        features.total_distance_km += haversine_distance_km(previous, current);
        features.max_time_gap_hours = std::max(features.max_time_gap_hours, time_gap_hours);

        if (time_gap_hours > ais_gap_threshold_hours_) {
            ++features.number_of_ais_gaps;
        }

        const auto turn_angle = turning_angle_deg(previous, current);
        turning_sum += turn_angle;
        if (turn_angle > high_turning_threshold_deg_) {
            ++high_turning_count;
        }

        ++transition_count;
    }

    features.fraction_high_turning = static_cast<double>(high_turning_count) / static_cast<double>(transition_count);
    features.mean_turning_angle = turning_sum / static_cast<double>(transition_count);

    const auto gap_fraction = static_cast<double>(features.number_of_ais_gaps) / static_cast<double>(transition_count);
    features.loitering_score = clamp_score(features.fraction_low_speed * features.fraction_high_turning);
    features.suspicious_manoeuvre_score =
        clamp_score(
            suspicious_manoeuvre_turning_weight_ * features.fraction_high_turning +
            suspicious_manoeuvre_gap_weight_ * gap_fraction);

    return features;
}

} // namespace oceanwatchai
