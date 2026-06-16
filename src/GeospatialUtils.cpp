#include "oceanwatchai/GeospatialUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace oceanwatchai {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::int64_t kSecondsPerHour = 3600;
constexpr std::int64_t kSecondsPerDay = 86400;

[[nodiscard]] double degrees_to_radians(double degrees)
{
    return degrees * kPi / 180.0;
}

void require_finite(double value, const char* name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument{std::string{name} + " must be finite"};
    }
}

void require_latitude(double latitude, const char* name)
{
    require_finite(latitude, name);
    if (latitude < -90.0 || latitude > 90.0) {
        throw std::invalid_argument{std::string{name} + " must be in [-90, 90]"};
    }
}

void require_longitude(double longitude, const char* name)
{
    require_finite(longitude, name);
    if (longitude < -180.0 || longitude > 180.0) {
        throw std::invalid_argument{std::string{name} + " must be in [-180, 180]"};
    }
}

[[nodiscard]] bool is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

[[nodiscard]] int parse_int(std::string_view text, std::size_t offset, std::size_t length)
{
    int value = 0;
    for (std::size_t index = 0; index < length; ++index) {
        const char ch = text.at(offset + index);
        if (!is_digit(ch)) {
            throw std::invalid_argument{"timestamp contains a non-digit component"};
        }
        value = (value * 10) + (ch - '0');
    }

    return value;
}

[[nodiscard]] bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

[[nodiscard]] int days_in_month(int year, int month)
{
    switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return is_leap_year(year) ? 29 : 28;
    default:
        throw std::invalid_argument{"timestamp month is outside [1, 12]"};
    }
}

// Howard Hinnant's civil-date conversion. Returns days since 1970-01-01.
[[nodiscard]] std::int64_t days_from_civil(int year, unsigned month, unsigned day)
{
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const auto year_of_era = static_cast<unsigned>(year - era * 400);
    const auto day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const auto day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(day_of_era) - 719468;
}

[[nodiscard]] std::int64_t parse_utc_timestamp_seconds(std::string_view timestamp)
{
    if (timestamp.size() != 20 || timestamp[4] != '-' || timestamp[7] != '-' ||
        (timestamp[10] != 'T' && timestamp[10] != 't') || timestamp[13] != ':' ||
        timestamp[16] != ':' || (timestamp[19] != 'Z' && timestamp[19] != 'z')) {
        throw std::invalid_argument{"timestamp must use ISO-8601 UTC format YYYY-MM-DDTHH:MM:SSZ"};
    }

    const int year = parse_int(timestamp, 0, 4);
    const int month = parse_int(timestamp, 5, 2);
    const int day = parse_int(timestamp, 8, 2);
    const int hour = parse_int(timestamp, 11, 2);
    const int minute = parse_int(timestamp, 14, 2);
    const int second = parse_int(timestamp, 17, 2);

    if (month < 1 || month > 12) {
        throw std::invalid_argument{"timestamp month is outside [1, 12]"};
    }

    if (day < 1 || day > days_in_month(year, month)) {
        throw std::invalid_argument{"timestamp day is invalid for its month"};
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        throw std::invalid_argument{"timestamp time component is outside the supported range"};
    }

    const auto days = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    return days * kSecondsPerDay + static_cast<std::int64_t>(hour) * kSecondsPerHour +
           static_cast<std::int64_t>(minute) * 60 + second;
}

} // namespace

double normalize_degrees(double degrees)
{
    require_finite(degrees, "degrees");

    auto normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }

    return normalized;
}

double angular_difference_deg(double first_deg, double second_deg)
{
    const auto first = normalize_degrees(first_deg);
    const auto second = normalize_degrees(second_deg);
    const auto difference = std::abs(first - second);
    return difference > 180.0 ? 360.0 - difference : difference;
}

double bearing_difference_deg(double first_bearing_deg, double second_bearing_deg)
{
    return angular_difference_deg(first_bearing_deg, second_bearing_deg);
}

double haversine_distance_km(double from_latitude, double from_longitude, double to_latitude, double to_longitude)
{
    require_latitude(from_latitude, "from_latitude");
    require_longitude(from_longitude, "from_longitude");
    require_latitude(to_latitude, "to_latitude");
    require_longitude(to_longitude, "to_longitude");

    const auto delta_latitude = degrees_to_radians(to_latitude - from_latitude);
    const auto delta_longitude = degrees_to_radians(to_longitude - from_longitude);
    const auto from_latitude_rad = degrees_to_radians(from_latitude);
    const auto to_latitude_rad = degrees_to_radians(to_latitude);

    const auto sin_half_latitude = std::sin(delta_latitude / 2.0);
    const auto sin_half_longitude = std::sin(delta_longitude / 2.0);
    const auto haversine = sin_half_latitude * sin_half_latitude +
                           std::cos(from_latitude_rad) * std::cos(to_latitude_rad) *
                               sin_half_longitude * sin_half_longitude;
    const auto central_angle = 2.0 * std::asin(std::sqrt(std::clamp(haversine, 0.0, 1.0)));

    return kMeanEarthRadiusKm * central_angle;
}

double haversine_distance_km(const AISPoint& from, const AISPoint& to)
{
    return haversine_distance_km(from.latitude, from.longitude, to.latitude, to.longitude);
}

double time_difference_hours(std::string_view from_timestamp, std::string_view to_timestamp)
{
    const auto from_seconds = parse_utc_timestamp_seconds(from_timestamp);
    const auto to_seconds = parse_utc_timestamp_seconds(to_timestamp);
    return static_cast<double>(to_seconds - from_seconds) / static_cast<double>(kSecondsPerHour);
}

double time_difference_hours(const AISPoint& from, const AISPoint& to)
{
    return time_difference_hours(from.timestamp, to.timestamp);
}

double segment_speed_knots(const AISPoint& from, const AISPoint& to)
{
    const auto elapsed_hours = time_difference_hours(from, to);
    if (elapsed_hours <= 0.0) {
        throw std::domain_error{"segment speed requires a positive time difference"};
    }

    const auto distance_nautical_miles = haversine_distance_km(from, to) / kKilometresPerNauticalMile;
    return distance_nautical_miles / elapsed_hours;
}

bool is_speed_consistent(const AISPoint& from, const AISPoint& to, double tolerance_knots)
{
    require_finite(tolerance_knots, "tolerance_knots");
    if (tolerance_knots < 0.0 || !std::isfinite(from.speed_knots) || !std::isfinite(to.speed_knots)) {
        return false;
    }

    const auto elapsed_hours = time_difference_hours(from, to);
    if (elapsed_hours <= 0.0) {
        return false;
    }

    const auto distance_nautical_miles = haversine_distance_km(from, to) / kKilometresPerNauticalMile;
    const auto implied_speed_knots = distance_nautical_miles / elapsed_hours;
    const auto reported_average_speed = (from.speed_knots + to.speed_knots) / 2.0;

    return std::abs(implied_speed_knots - reported_average_speed) <= tolerance_knots;
}

double approximate_acceleration_knots_per_hour(const AISPoint& from, const AISPoint& to)
{
    require_finite(from.speed_knots, "from.speed_knots");
    require_finite(to.speed_knots, "to.speed_knots");

    const auto elapsed_hours = time_difference_hours(from, to);
    if (elapsed_hours <= 0.0) {
        throw std::domain_error{"acceleration requires a positive time difference"};
    }

    return (to.speed_knots - from.speed_knots) / elapsed_hours;
}

double turning_angle_deg(const AISPoint& from, const AISPoint& to)
{
    return angular_difference_deg(from.course_deg, to.course_deg);
}

} // namespace oceanwatchai
