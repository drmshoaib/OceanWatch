#pragma once

#include "oceanwatchai/AnalysisConfig.hpp"
#include "oceanwatchai/TrackFeatures.hpp"
#include "oceanwatchai/VesselTrack.hpp"

namespace oceanwatchai {

// Converts ordered AIS vessel tracks into transparent trajectory features.
class FeatureExtractor {
public:
    FeatureExtractor(
        double low_speed_min_knots = 1.0,
        double low_speed_max_knots = 5.0,
        double high_turning_threshold_deg = 45.0,
        double ais_gap_threshold_hours = 2.0);
    explicit FeatureExtractor(const FeatureExtractionConfig& config);

    [[nodiscard]] TrackFeatures extract(const VesselTrack& track) const;

private:
    double low_speed_min_knots_;
    double low_speed_max_knots_;
    double high_turning_threshold_deg_;
    double ais_gap_threshold_hours_;
    double suspicious_manoeuvre_turning_weight_;
    double suspicious_manoeuvre_gap_weight_;
};

} // namespace oceanwatchai
