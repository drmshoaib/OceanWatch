#pragma once

#include "oceanwatchai/AISPoint.hpp"

#include <string_view>

namespace oceanwatchai {

inline constexpr double kMeanEarthRadiusKm = 6371.0088;
inline constexpr double kKilometresPerNauticalMile = 1.852;

// Normalizes any finite heading into [0, 360).
double normalize_degrees(double degrees);

// Returns the smallest absolute angular difference in [0, 180].
double angular_difference_deg(double first_deg, double second_deg);
double bearing_difference_deg(double first_bearing_deg, double second_bearing_deg);

// Computes great-circle distance using the Haversine formula.
double haversine_distance_km(double from_latitude, double from_longitude, double to_latitude, double to_longitude);
double haversine_distance_km(const AISPoint& from, const AISPoint& to);

// Returns signed elapsed hours between UTC timestamps in YYYY-MM-DDTHH:MM:SSZ format.
double time_difference_hours(std::string_view from_timestamp, std::string_view to_timestamp);
double time_difference_hours(const AISPoint& from, const AISPoint& to);

// Computes movement-derived features between consecutive AIS points.
double segment_speed_knots(const AISPoint& from, const AISPoint& to);
bool is_speed_consistent(const AISPoint& from, const AISPoint& to, double tolerance_knots = 5.0);
double approximate_acceleration_knots_per_hour(const AISPoint& from, const AISPoint& to);
double turning_angle_deg(const AISPoint& from, const AISPoint& to);

} // namespace oceanwatchai
