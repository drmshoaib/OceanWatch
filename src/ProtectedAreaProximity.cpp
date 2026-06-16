#include "oceanwatchai/ProtectedAreaProximity.hpp"

#include "oceanwatchai/GeospatialUtils.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oceanwatchai {
namespace {

constexpr std::array<std::string_view, 4> kRequiredColumns{
    "name",
    "centre_latitude",
    "centre_longitude",
    "radius_km",
};

[[nodiscard]] std::string trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string{value.substr(first, last - first + 1)};
}

[[nodiscard]] std::string line_error(std::size_t line_number, std::string_view message)
{
    std::ostringstream output;
    output << "Line " << line_number << ": " << message;
    return output.str();
}

[[nodiscard]] std::vector<std::string> parse_csv_line(std::string_view line, std::size_t line_number)
{
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];

        if (in_quotes) {
            if (ch == '"') {
                if ((index + 1) < line.size() && line[index + 1] == '"') {
                    field.push_back('"');
                    ++index;
                } else {
                    in_quotes = false;
                }
            } else {
                field.push_back(ch);
            }
            continue;
        }

        if (ch == ',') {
            fields.push_back(trim(field));
            field.clear();
        } else if (ch == '"') {
            if (!trim(field).empty()) {
                throw ProtectedAreaCsvError{line_error(line_number, "unexpected quote inside unquoted field")};
            }
            in_quotes = true;
        } else {
            field.push_back(ch);
        }
    }

    if (in_quotes) {
        throw ProtectedAreaCsvError{line_error(line_number, "unterminated quoted field")};
    }

    fields.push_back(trim(field));
    return fields;
}

[[nodiscard]] std::unordered_map<std::string, std::size_t> build_header_index(
    const std::vector<std::string>& headers,
    std::size_t line_number)
{
    std::unordered_map<std::string, std::size_t> index_by_name;
    index_by_name.reserve(headers.size());

    for (std::size_t index = 0; index < headers.size(); ++index) {
        if (headers[index].empty()) {
            throw ProtectedAreaCsvError{line_error(line_number, "empty column name in header")};
        }

        const auto insert_result = index_by_name.emplace(headers[index], index);
        if (!insert_result.second) {
            throw ProtectedAreaCsvError{line_error(line_number, "duplicate column in header: " + headers[index])};
        }
    }

    for (const auto required_column : kRequiredColumns) {
        if (!index_by_name.contains(std::string{required_column})) {
            throw ProtectedAreaCsvError{
                line_error(line_number, "missing required column: " + std::string{required_column})};
        }
    }

    return index_by_name;
}

[[nodiscard]] double parse_double(
    const std::string& value,
    std::string_view column_name,
    std::size_t line_number)
{
    const auto trimmed = trim(value);
    if (trimmed.empty()) {
        throw ProtectedAreaCsvError{line_error(line_number, "empty numeric field: " + std::string{column_name})};
    }

    double result = 0.0;
    const auto* begin = trimmed.data();
    const auto* end = begin + trimmed.size();
    const auto [next, error] = std::from_chars(begin, end, result);

    if (error != std::errc{} || next != end || !std::isfinite(result)) {
        throw ProtectedAreaCsvError{line_error(line_number, "invalid numeric field: " + std::string{column_name})};
    }

    return result;
}

[[nodiscard]] const std::string& field_for(
    const std::vector<std::string>& fields,
    const std::unordered_map<std::string, std::size_t>& header_index,
    std::string_view column_name)
{
    return fields.at(header_index.at(std::string{column_name}));
}

[[nodiscard]] ProtectedArea parse_area(
    const std::vector<std::string>& fields,
    const std::unordered_map<std::string, std::size_t>& header_index,
    std::size_t line_number)
{
    ProtectedArea area{
        .name = field_for(fields, header_index, "name"),
        .centre_latitude = parse_double(field_for(fields, header_index, "centre_latitude"), "centre_latitude", line_number),
        .centre_longitude =
            parse_double(field_for(fields, header_index, "centre_longitude"), "centre_longitude", line_number),
        .radius_km = parse_double(field_for(fields, header_index, "radius_km"), "radius_km", line_number),
    };

    if (area.name.empty()) {
        throw ProtectedAreaCsvError{line_error(line_number, "name is required")};
    }

    if (area.centre_latitude < -90.0 || area.centre_latitude > 90.0) {
        throw ProtectedAreaCsvError{line_error(line_number, "centre_latitude outside expected range [-90, 90]")};
    }

    if (area.centre_longitude < -180.0 || area.centre_longitude > 180.0) {
        throw ProtectedAreaCsvError{line_error(line_number, "centre_longitude outside expected range [-180, 180]")};
    }

    if (area.radius_km <= 0.0) {
        throw ProtectedAreaCsvError{line_error(line_number, "radius_km must be positive")};
    }

    return area;
}

[[nodiscard]] double segment_overlap_hours(bool previous_in_zone, bool current_in_zone, double segment_hours)
{
    if (previous_in_zone && current_in_zone) {
        return segment_hours;
    }

    if (previous_in_zone || current_in_zone) {
        return segment_hours / 2.0;
    }

    return 0.0;
}

[[nodiscard]] std::string format_hours(double hours)
{
    std::ostringstream output;
    output.setf(std::ios::fixed);
    output.precision(1);
    output << hours;
    return output.str();
}

} // namespace

double ProtectedAreaProximityResult::total_time_inside_hours() const noexcept
{
    return std::accumulate(visits.begin(), visits.end(), 0.0, [](double total, const ProtectedAreaVisit& visit) {
        return total + visit.time_inside_hours;
    });
}

double ProtectedAreaProximityResult::total_time_near_hours() const noexcept
{
    return std::accumulate(visits.begin(), visits.end(), 0.0, [](double total, const ProtectedAreaVisit& visit) {
        return total + visit.time_near_hours;
    });
}

std::vector<ProtectedArea> load_protected_areas_csv(std::istream& input)
{
    std::string line;
    std::size_t line_number = 0;
    std::vector<std::string> headers;

    while (std::getline(input, line)) {
        ++line_number;
        if (!trim(line).empty()) {
            headers = parse_csv_line(line, line_number);
            break;
        }
    }

    if (headers.empty()) {
        throw ProtectedAreaCsvError{"Protected area CSV input is empty"};
    }

    const auto header_index = build_header_index(headers, line_number);
    std::vector<ProtectedArea> areas;

    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }

        const auto fields = parse_csv_line(line, line_number);
        if (fields.size() != headers.size()) {
            std::ostringstream message;
            message << "expected " << headers.size() << " fields but found " << fields.size();
            throw ProtectedAreaCsvError{line_error(line_number, message.str())};
        }

        areas.push_back(parse_area(fields, header_index, line_number));
    }

    return areas;
}

std::vector<ProtectedArea> load_protected_areas_csv(const std::filesystem::path& file_path)
{
    std::ifstream input{file_path};
    if (!input) {
        throw ProtectedAreaCsvError{"Unable to open protected-area CSV file: " + file_path.string()};
    }

    return load_protected_areas_csv(input);
}

ProtectedAreaProximityAnalyzer::ProtectedAreaProximityAnalyzer(
    double near_buffer_km,
    double loitering_time_threshold_hours)
    : near_buffer_km_{near_buffer_km}
    , loitering_time_threshold_hours_{loitering_time_threshold_hours}
{
    if (!std::isfinite(near_buffer_km_) || near_buffer_km_ < 0.0) {
        throw std::invalid_argument{"near_buffer_km must be finite and non-negative"};
    }

    if (!std::isfinite(loitering_time_threshold_hours_) || loitering_time_threshold_hours_ < 0.0) {
        throw std::invalid_argument{"loitering_time_threshold_hours must be finite and non-negative"};
    }
}

bool ProtectedAreaProximityAnalyzer::is_inside(const AISPoint& point, const ProtectedArea& area) const
{
    return haversine_distance_km(point.latitude, point.longitude, area.centre_latitude, area.centre_longitude) <=
           area.radius_km;
}

bool ProtectedAreaProximityAnalyzer::is_near(const AISPoint& point, const ProtectedArea& area) const
{
    const auto distance_km =
        haversine_distance_km(point.latitude, point.longitude, area.centre_latitude, area.centre_longitude);
    return distance_km > area.radius_km && distance_km <= area.radius_km + near_buffer_km_;
}

std::vector<std::string> ProtectedAreaProximityAnalyzer::areas_entered_by_point(
    const AISPoint& point,
    const std::vector<ProtectedArea>& areas) const
{
    std::vector<std::string> entered_areas;

    for (const auto& area : areas) {
        if (is_inside(point, area)) {
            entered_areas.push_back(area.name);
        }
    }

    return entered_areas;
}

ProtectedAreaProximityResult ProtectedAreaProximityAnalyzer::analyze_track(
    const VesselTrack& track,
    const std::vector<ProtectedArea>& areas) const
{
    ProtectedAreaProximityResult result{
        .vessel_id = track.vessel_id(),
    };

    const auto& points = track.points();
    result.visits.reserve(areas.size());

    for (const auto& area : areas) {
        ProtectedAreaVisit visit{
            .area_name = area.name,
        };

        for (const auto& point : points) {
            if (is_inside(point, area)) {
                ++visit.points_inside;
            } else if (is_near(point, area)) {
                ++visit.points_near;
            }
        }

        visit.entered = visit.points_inside > 0;

        for (std::size_t index = 1; index < points.size(); ++index) {
            const auto& previous = points[index - 1];
            const auto& current = points[index];
            const auto segment_hours = time_difference_hours(previous, current);
            if (segment_hours < 0.0) {
                throw std::invalid_argument{"track timestamps must be sorted in ascending order"};
            }

            const auto previous_inside = is_inside(previous, area);
            const auto current_inside = is_inside(current, area);
            const auto previous_near = !previous_inside && is_near(previous, area);
            const auto current_near = !current_inside && is_near(current, area);

            visit.time_inside_hours += segment_overlap_hours(previous_inside, current_inside, segment_hours);
            visit.time_near_hours += segment_overlap_hours(previous_near, current_near, segment_hours);
        }

        if (visit.entered) {
            visit.explanations.push_back("AIS points entered protected area '" + area.name + "'.");
        }

        if (visit.time_inside_hours >= loitering_time_threshold_hours_) {
            visit.explanations.push_back(
                "Vessel loitered inside protected area '" + area.name + "' for approximately " +
                format_hours(visit.time_inside_hours) + " hours.");
        }

        if (visit.time_near_hours >= loitering_time_threshold_hours_) {
            visit.explanations.push_back(
                "Vessel loitered near protected area '" + area.name + "' for approximately " +
                format_hours(visit.time_near_hours) + " hours.");
        }

        for (const auto& explanation : visit.explanations) {
            result.explanations.push_back(explanation);
        }

        result.visits.push_back(std::move(visit));
    }

    if (result.explanations.empty()) {
        result.explanations.push_back("No protected-area entry or loitering indicators were detected.");
    }

    return result;
}

} // namespace oceanwatchai
