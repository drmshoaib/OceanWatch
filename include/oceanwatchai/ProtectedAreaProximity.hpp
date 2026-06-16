#pragma once

#include "oceanwatchai/ProtectedArea.hpp"
#include "oceanwatchai/VesselTrack.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace oceanwatchai {

class ProtectedAreaCsvError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct ProtectedAreaVisit {
    std::string area_name;
    double time_inside_hours{};
    double time_near_hours{};
    std::size_t points_inside{};
    std::size_t points_near{};
    bool entered{};
    std::vector<std::string> explanations;
};

struct ProtectedAreaProximityResult {
    std::string vessel_id;
    std::vector<ProtectedAreaVisit> visits;
    std::vector<std::string> explanations;

    [[nodiscard]] double total_time_inside_hours() const noexcept;
    [[nodiscard]] double total_time_near_hours() const noexcept;
};

std::vector<ProtectedArea> load_protected_areas_csv(std::istream& input);
std::vector<ProtectedArea> load_protected_areas_csv(const std::filesystem::path& file_path);

// Uses circular protected-area approximations for lightweight prototype analysis.
class ProtectedAreaProximityAnalyzer {
public:
    explicit ProtectedAreaProximityAnalyzer(
        double near_buffer_km = 5.0,
        double loitering_time_threshold_hours = 1.0);

    [[nodiscard]] bool is_inside(const AISPoint& point, const ProtectedArea& area) const;
    [[nodiscard]] bool is_near(const AISPoint& point, const ProtectedArea& area) const;
    [[nodiscard]] std::vector<std::string> areas_entered_by_point(
        const AISPoint& point,
        const std::vector<ProtectedArea>& areas) const;
    [[nodiscard]] ProtectedAreaProximityResult analyze_track(
        const VesselTrack& track,
        const std::vector<ProtectedArea>& areas) const;

private:
    double near_buffer_km_;
    double loitering_time_threshold_hours_;
};

} // namespace oceanwatchai
