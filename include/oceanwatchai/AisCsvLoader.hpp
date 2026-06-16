#pragma once

#include "oceanwatchai/VesselTrack.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace oceanwatchai {

class CsvLoadError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct CsvWarning {
    std::size_t line_number{};
    std::string message;
    std::string raw_line;
};

struct AisCsvLoadResult {
    std::vector<VesselTrack> tracks;
    std::vector<CsvWarning> warnings;

    [[nodiscard]] std::size_t point_count() const noexcept;
    [[nodiscard]] const VesselTrack* find_track(std::string_view vessel_id) const noexcept;
};

// Loads AIS observations from CSV, grouping valid rows by vessel_id.
//
// Required-header errors are fatal and throw CsvLoadError. Malformed data rows
// are skipped and returned as CsvWarning entries so a partially useful file can
// still be analysed.
AisCsvLoadResult load_ais_csv(std::istream& input);
AisCsvLoadResult load_ais_csv(const std::filesystem::path& file_path);

} // namespace oceanwatchai
