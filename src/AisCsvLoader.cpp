#include "oceanwatchai/AisCsvLoader.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oceanwatchai {
namespace {

constexpr std::array<std::string_view, 6> kRequiredColumns{
    "vessel_id",
    "timestamp",
    "latitude",
    "longitude",
    "speed_knots",
    "course_deg",
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
                throw CsvLoadError{line_error(line_number, "unexpected quote inside unquoted field")};
            }
            in_quotes = true;
        } else {
            field.push_back(ch);
        }
    }

    if (in_quotes) {
        throw CsvLoadError{line_error(line_number, "unterminated quoted field")};
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
            throw CsvLoadError{line_error(line_number, "empty column name in header")};
        }

        const auto insert_result = index_by_name.emplace(headers[index], index);
        if (!insert_result.second) {
            throw CsvLoadError{line_error(line_number, "duplicate column in header: " + headers[index])};
        }
    }

    for (const auto required_column : kRequiredColumns) {
        if (!index_by_name.contains(std::string{required_column})) {
            throw CsvLoadError{line_error(line_number, "missing required column: " + std::string{required_column})};
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
        throw CsvLoadError{line_error(line_number, "empty numeric field: " + std::string{column_name})};
    }

    double result = 0.0;
    const auto* begin = trimmed.data();
    const auto* end = begin + trimmed.size();
    const auto [next, error] = std::from_chars(begin, end, result);

    if (error != std::errc{} || next != end || !std::isfinite(result)) {
        throw CsvLoadError{line_error(line_number, "invalid numeric field: " + std::string{column_name})};
    }

    return result;
}

void require_range(
    double value,
    double min_value,
    double max_value,
    std::string_view column_name,
    std::size_t line_number)
{
    if (value < min_value || value > max_value) {
        std::ostringstream message;
        message << column_name << " outside expected range [" << min_value << ", " << max_value << "]";
        throw CsvLoadError{line_error(line_number, message.str())};
    }
}

[[nodiscard]] const std::string& field_for(
    const std::vector<std::string>& fields,
    const std::unordered_map<std::string, std::size_t>& header_index,
    std::string_view column_name)
{
    return fields.at(header_index.at(std::string{column_name}));
}

[[nodiscard]] AISPoint parse_point(
    const std::vector<std::string>& fields,
    const std::unordered_map<std::string, std::size_t>& header_index,
    std::size_t line_number)
{
    AISPoint point{
        .vessel_id = field_for(fields, header_index, "vessel_id"),
        .timestamp = field_for(fields, header_index, "timestamp"),
        .latitude = parse_double(field_for(fields, header_index, "latitude"), "latitude", line_number),
        .longitude = parse_double(field_for(fields, header_index, "longitude"), "longitude", line_number),
        .speed_knots = parse_double(field_for(fields, header_index, "speed_knots"), "speed_knots", line_number),
        .course_deg = parse_double(field_for(fields, header_index, "course_deg"), "course_deg", line_number),
    };

    if (point.vessel_id.empty()) {
        throw CsvLoadError{line_error(line_number, "vessel_id is required")};
    }

    if (point.timestamp.empty()) {
        throw CsvLoadError{line_error(line_number, "timestamp is required")};
    }

    require_range(point.latitude, -90.0, 90.0, "latitude", line_number);
    require_range(point.longitude, -180.0, 180.0, "longitude", line_number);
    require_range(point.speed_knots, 0.0, std::numeric_limits<double>::max(), "speed_knots", line_number);
    require_range(point.course_deg, 0.0, 360.0, "course_deg", line_number);

    return point;
}

void add_warning(
    std::vector<CsvWarning>& warnings,
    std::size_t line_number,
    std::string message,
    std::string raw_line)
{
    warnings.push_back(CsvWarning{
        .line_number = line_number,
        .message = std::move(message),
        .raw_line = std::move(raw_line),
    });
}

[[nodiscard]] std::vector<VesselTrack> to_sorted_tracks(std::map<std::string, VesselTrack>& grouped_tracks)
{
    std::vector<VesselTrack> tracks;
    tracks.reserve(grouped_tracks.size());

    for (auto& entry : grouped_tracks) {
        auto& track = entry.second;
        track.sort_by_timestamp();
        tracks.push_back(std::move(track));
    }

    return tracks;
}

} // namespace

std::size_t AisCsvLoadResult::point_count() const noexcept
{
    return std::accumulate(
        tracks.begin(),
        tracks.end(),
        std::size_t{0},
        [](std::size_t total, const VesselTrack& track) {
            return total + track.size();
        });
}

const VesselTrack* AisCsvLoadResult::find_track(std::string_view vessel_id) const noexcept
{
    for (const auto& track : tracks) {
        const auto track_id = std::string_view{track.vessel_id().data(), track.vessel_id().size()};
        if (track_id == vessel_id) {
            return &track;
        }
    }

    return nullptr;
}

AisCsvLoadResult load_ais_csv(std::istream& input)
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
        throw CsvLoadError{"CSV input is empty"};
    }

    const auto header_index = build_header_index(headers, line_number);
    std::map<std::string, VesselTrack> grouped_tracks;
    AisCsvLoadResult result;

    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }

        try {
            const auto fields = parse_csv_line(line, line_number);
            if (fields.size() != headers.size()) {
                std::ostringstream message;
                message << "expected " << headers.size() << " fields but found " << fields.size();
                throw CsvLoadError{line_error(line_number, message.str())};
            }

            auto point = parse_point(fields, header_index, line_number);
            const auto vessel_id = point.vessel_id;
            auto insert_result = grouped_tracks.try_emplace(vessel_id, vessel_id);
            insert_result.first->second.add_point(std::move(point));
        } catch (const CsvLoadError& error) {
            add_warning(result.warnings, line_number, error.what(), line);
        }
    }

    result.tracks = to_sorted_tracks(grouped_tracks);
    return result;
}

AisCsvLoadResult load_ais_csv(const std::filesystem::path& file_path)
{
    std::ifstream input{file_path};
    if (!input) {
        throw CsvLoadError{"Unable to open AIS CSV file: " + file_path.string()};
    }

    return load_ais_csv(input);
}

} // namespace oceanwatchai
