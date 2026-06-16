#include "oceanwatchai/ReportWriter.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string_view>

namespace oceanwatchai {
namespace {

void ensure_parent_directory(const std::filesystem::path& output_path)
{
    const auto parent = output_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

[[nodiscard]] std::string csv_escape(std::string_view value)
{
    bool needs_quotes = false;
    for (const auto ch : value) {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return std::string{value};
    }

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const auto ch : value) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

[[nodiscard]] std::string markdown_escape(std::string value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const auto ch : value) {
        if (ch == '|') {
            escaped += "\\|";
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

[[nodiscard]] bool is_high_risk(const RiskScoreResult& risk_score)
{
    return risk_score.risk_band == RiskBand::High || risk_score.risk_band == RiskBand::Critical;
}

[[nodiscard]] double protected_area_score(const VesselAnalysisReport& report)
{
    if (!report.protected_area_result) {
        return 0.0;
    }

    const auto inside_score = report.protected_area_result->total_time_inside_hours() * 50.0;
    const auto near_score = report.protected_area_result->total_time_near_hours() * 25.0;
    return std::clamp(inside_score + near_score, 0.0, 100.0);
}

[[nodiscard]] double anomaly_score(const VesselAnalysisReport& report)
{
    return report.risk_score.component_scores.route_anomaly;
}

[[nodiscard]] std::vector<VesselAnalysisReport> sorted_by_risk(const std::vector<VesselAnalysisReport>& reports)
{
    auto sorted = reports;
    std::sort(sorted.begin(), sorted.end(), [](const VesselAnalysisReport& lhs, const VesselAnalysisReport& rhs) {
        return lhs.risk_score.total_score > rhs.risk_score.total_score;
    });
    return sorted;
}

[[nodiscard]] std::vector<std::string> combined_explanations(const VesselAnalysisReport& report)
{
    auto explanations = report.risk_score.explanations;
    if (report.protected_area_result) {
        explanations.insert(
            explanations.end(),
            report.protected_area_result->explanations.begin(),
            report.protected_area_result->explanations.end());
    }
    return explanations;
}

template <typename Value>
void write_csv_value(std::ostream& output, const Value& value)
{
    output << value;
}

void write_csv_value(std::ostream& output, const std::string& value)
{
    output << csv_escape(value);
}

void write_csv_value(std::ostream& output, std::string_view value)
{
    output << csv_escape(value);
}

template <typename First, typename... Rest>
void write_csv_row(std::ostream& output, const First& first, const Rest&... rest)
{
    write_csv_value(output, first);
    ((output << ',', write_csv_value(output, rest)), ...);
    output << '\n';
}

} // namespace

void write_csv_report(const std::filesystem::path& output_path, const std::vector<VesselAnalysisReport>& reports)
{
    ensure_parent_directory(output_path);

    std::ofstream output{output_path};
    if (!output) {
        throw std::runtime_error{"Unable to open CSV report for writing: " + output_path.string()};
    }

    output << std::fixed << std::setprecision(3);
    write_csv_row(
        output,
        "vessel_id",
        "total_score",
        "risk_band",
        "loitering_score",
        "ais_gap_score",
        "low_speed_score",
        "turning_score",
        "protected_area_score",
        "anomaly_score");

    for (const auto& report : reports) {
        write_csv_row(
            output,
            report.risk_score.vessel_id,
            report.risk_score.total_score,
            risk_band_to_string(report.risk_score.risk_band),
            report.risk_score.component_scores.loitering,
            report.risk_score.component_scores.AIS_gap,
            report.risk_score.component_scores.low_speed,
            report.risk_score.component_scores.turning,
            protected_area_score(report),
            anomaly_score(report));
    }
}

void write_markdown_report(
    const std::filesystem::path& output_path,
    const ReportMetadata& metadata,
    const std::vector<VesselAnalysisReport>& reports)
{
    ensure_parent_directory(output_path);

    std::ofstream output{output_path};
    if (!output) {
        throw std::runtime_error{"Unable to open Markdown report for writing: " + output_path.string()};
    }

    output << std::fixed << std::setprecision(2);
    const auto high_risk_count = std::count_if(reports.begin(), reports.end(), [](const VesselAnalysisReport& report) {
        return is_high_risk(report.risk_score);
    });
    const auto sorted_reports = sorted_by_risk(reports);

    output << "# OceanWatchAI Risk Report\n\n";
    output << "## Summary\n\n";
    output << "| Metric | Value |\n";
    output << "|---|---:|\n";
    output << "| AIS points loaded | " << metadata.ais_point_count << " |\n";
    output << "| AIS row warnings | " << metadata.ais_warning_count << " |\n";
    output << "| Vessels analysed | " << reports.size() << " |\n";
    output << "| Protected areas loaded | " << metadata.protected_area_count << " |\n";
    output << "| High or critical vessels | " << high_risk_count << " |\n\n";

    output << "AIS source: `" << markdown_escape(metadata.ais_path.string()) << "`\n\n";
    if (metadata.protected_areas_path) {
        output << "Protected-area source: `" << markdown_escape(metadata.protected_areas_path->string()) << "`\n\n";
    }

    output << "## Summary Table\n\n";
    output << "| Vessel ID | Score | Band | Loitering | AIS Gap | Low Speed | Turning | Protected Area | Anomaly |\n";
    output << "|---|---:|---|---:|---:|---:|---:|---:|---:|\n";
    for (const auto& report : sorted_reports) {
        output << "| " << markdown_escape(report.risk_score.vessel_id) << " | "
               << report.risk_score.total_score << " | "
               << risk_band_to_string(report.risk_score.risk_band) << " | "
               << report.risk_score.component_scores.loitering << " | "
               << report.risk_score.component_scores.AIS_gap << " | "
               << report.risk_score.component_scores.low_speed << " | "
               << report.risk_score.component_scores.turning << " | "
               << protected_area_score(report) << " | "
               << anomaly_score(report) << " |\n";
    }

    output << "\n## Top Suspicious Vessels\n\n";
    output << "| Rank | Vessel ID | Score | Band |\n";
    output << "|---:|---|---:|---|\n";
    const auto top_count = std::min<std::size_t>(5, sorted_reports.size());
    for (std::size_t index = 0; index < top_count; ++index) {
        const auto& report = sorted_reports[index];
        output << "| " << (index + 1) << " | " << markdown_escape(report.risk_score.vessel_id) << " | "
               << report.risk_score.total_score << " | " << risk_band_to_string(report.risk_score.risk_band)
               << " |\n";
    }

    output << "\n## High-Risk Explanations\n\n";
    if (high_risk_count == 0) {
        output << "No vessels reached the High or Critical risk bands in this run.\n";
    } else {
        for (const auto& report : sorted_reports) {
            if (!is_high_risk(report.risk_score)) {
                continue;
            }

            output << "### " << markdown_escape(report.risk_score.vessel_id) << "\n\n";
            output << "- Score: " << report.risk_score.total_score << " / 100\n";
            output << "- Band: " << risk_band_to_string(report.risk_score.risk_band) << "\n";
            for (const auto& explanation : combined_explanations(report)) {
                output << "- " << markdown_escape(explanation) << "\n";
            }
            output << '\n';
        }
    }

    output << "\n## Limitations\n\n";
    output << "- AIS timestamps are treated as UTC ISO-8601 strings.\n";
    output << "- Protected areas are approximated as circles, not legal polygon boundaries.\n";
    output << "- Protected-area score is report-only and is not currently part of the risk total.\n";
    output << "- Anomaly score reflects the current route-anomaly component or proxy, depending on pipeline input.\n";
    output << "- Rule-based scores are deterministic prototype indicators, not enforcement-grade classifications.\n";
}

} // namespace oceanwatchai
