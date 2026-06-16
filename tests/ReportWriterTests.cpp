#include "oceanwatchai/ReportWriter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input{path};
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

oceanwatchai::VesselAnalysisReport make_report()
{
    oceanwatchai::TrackFeatures features{
        .vessel_id = "FV-REPORT-001",
        .total_distance_km = 12.5,
        .duration_hours = 2.0,
        .mean_speed_knots = 4.0,
        .max_speed_knots = 8.0,
        .fraction_low_speed = 0.5,
        .fraction_high_turning = 0.25,
        .mean_turning_angle = 30.0,
        .max_time_gap_hours = 1.0,
        .loitering_score = 0.25,
        .suspicious_manoeuvre_score = 0.4,
    };

    oceanwatchai::RiskScoreResult risk{
        .vessel_id = "FV-REPORT-001",
        .total_score = 76.0,
        .risk_band = oceanwatchai::RiskBand::Critical,
        .explanations = {"quoted \"risk\", explanation"},
        .component_scores = oceanwatchai::RiskComponentScores{
            .loitering = 25.0,
            .AIS_gap = 0.0,
            .low_speed = 50.0,
            .turning = 25.0,
            .route_anomaly = 40.0,
        },
    };

    oceanwatchai::ProtectedAreaProximityResult protected_area{
        .vessel_id = "FV-REPORT-001",
        .visits = {
            oceanwatchai::ProtectedAreaVisit{
                .area_name = "Comma, Reserve",
                .time_inside_hours = 1.5,
                .time_near_hours = 0.5,
                .points_inside = 2,
                .entered = true,
                .explanations = {"entered \"Comma, Reserve\""},
            },
        },
        .explanations = {"entered \"Comma, Reserve\""},
    };

    return oceanwatchai::VesselAnalysisReport{
        .features = std::move(features),
        .risk_score = std::move(risk),
        .protected_area_result = std::move(protected_area),
    };
}

} // namespace

TEST_CASE("Report writer creates compact CSV and GitHub Markdown reports")
{
    const auto output_dir = std::filesystem::temp_directory_path() / "oceanwatchai_report_writer_tests";
    std::filesystem::remove_all(output_dir);

    const auto csv_path = output_dir / "nested" / "risk_report.csv";
    const auto markdown_path = output_dir / "nested" / "risk_report.md";
    const std::vector<oceanwatchai::VesselAnalysisReport> reports{make_report()};

    oceanwatchai::write_csv_report(csv_path, reports);
    oceanwatchai::write_markdown_report(
        markdown_path,
        oceanwatchai::ReportMetadata{
            .ais_path = "data/sample/sample_ais.csv",
            .protected_areas_path = std::filesystem::path{"data/sample/protected_areas.csv"},
            .ais_point_count = 8,
            .protected_area_count = 3,
        },
        reports);

    REQUIRE(std::filesystem::exists(csv_path));
    REQUIRE(std::filesystem::exists(markdown_path));

    const auto csv = read_file(csv_path);
    CHECK(
        csv.find(
            "vessel_id,total_score,risk_band,loitering_score,ais_gap_score,low_speed_score,turning_score,"
            "protected_area_score,anomaly_score") != std::string::npos);
    CHECK(csv.find("FV-REPORT-001,76.000,Critical,25.000,0.000,50.000,25.000,87.500,40.000") !=
          std::string::npos);

    const auto markdown = read_file(markdown_path);
    CHECK(markdown.find("# OceanWatchAI Risk Report") != std::string::npos);
    CHECK(markdown.find("## Summary") != std::string::npos);
    CHECK(markdown.find("## Summary Table") != std::string::npos);
    CHECK(markdown.find("| Vessel ID | Score | Band | Loitering | AIS Gap | Low Speed | Turning | Protected Area | Anomaly |") !=
          std::string::npos);
    CHECK(markdown.find("## Top Suspicious Vessels") != std::string::npos);
    CHECK(markdown.find("| 1 | FV-REPORT-001 | 76.00 | Critical |") != std::string::npos);
    CHECK(markdown.find("## High-Risk Explanations") != std::string::npos);
    CHECK(markdown.find("### FV-REPORT-001") != std::string::npos);
    CHECK(markdown.find("- quoted \"risk\", explanation") != std::string::npos);
    CHECK(markdown.find("## Limitations") != std::string::npos);

    std::filesystem::remove_all(output_dir);
}
