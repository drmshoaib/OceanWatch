#pragma once

#include <cstddef>
#include <string>

namespace oceanwatchai {

// Deterministic summary metrics extracted from one vessel trajectory.
struct TrackFeatures {
    std::string vessel_id;
    double total_distance_km{};
    double duration_hours{};
    double mean_speed_knots{};
    double max_speed_knots{};
    double fraction_low_speed{};
    double fraction_high_turning{};
    double mean_turning_angle{};
    double max_time_gap_hours{};
    std::size_t number_of_ais_gaps{};
    double loitering_score{};
    double suspicious_manoeuvre_score{};
};

} // namespace oceanwatchai
