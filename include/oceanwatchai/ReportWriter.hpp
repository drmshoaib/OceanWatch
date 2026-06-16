#pragma once

#include "oceanwatchai/ProtectedAreaProximity.hpp"
#include "oceanwatchai/RiskScoringEngine.hpp"
#include "oceanwatchai/TrackFeatures.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace oceanwatchai {

struct VesselAnalysisReport {
    TrackFeatures features;
    RiskScoreResult risk_score;
    std::optional<ProtectedAreaProximityResult> protected_area_result;
};

struct ReportMetadata {
    std::filesystem::path ais_path;
    std::optional<std::filesystem::path> protected_areas_path;
    std::size_t ais_point_count{};
    std::size_t ais_warning_count{};
    std::size_t protected_area_count{};
};

void write_csv_report(const std::filesystem::path& output_path, const std::vector<VesselAnalysisReport>& reports);
void write_markdown_report(
    const std::filesystem::path& output_path,
    const ReportMetadata& metadata,
    const std::vector<VesselAnalysisReport>& reports);

} // namespace oceanwatchai
