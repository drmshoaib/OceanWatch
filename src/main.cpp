#include "oceanwatchai/AisCsvLoader.hpp"
#include "oceanwatchai/FeatureExtractor.hpp"
#include "oceanwatchai/ProtectedAreaProximity.hpp"
#include "oceanwatchai/ReportWriter.hpp"
#include "oceanwatchai/RiskScoringEngine.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct CliOptions {
    bool show_help{};
    std::filesystem::path ais_path;
    std::optional<std::filesystem::path> protected_areas_path;
    std::filesystem::path csv_output_path;
    std::optional<std::filesystem::path> markdown_output_path;
};

[[nodiscard]] std::string usage_text()
{
    return R"(OceanWatchAI - AIS vessel trajectory risk analysis

Usage:
  OceanWatchAI.exe --ais <ais.csv> --output <risk_report.csv> [options]

Required:
  --ais <path>                 AIS CSV file with vessel trajectory points.
  --output <path>              CSV report output path.

Options:
  --protected-areas <path>     Optional protected-area CSV file.
  --markdown-output <path>     Optional Markdown report output path.
                               Defaults to --output with a .md extension.
  --text-output <path>         Backward-compatible alias for --markdown-output.
  -h, --help                   Show this help text.

Examples:
  OceanWatchAI.exe --ais data/sample/sample_ais.csv --output reports/risk_report.csv
  OceanWatchAI.exe --ais data/sample/sample_ais.csv --protected-areas data/sample/protected_areas.csv --output reports/risk_report.csv
)";
}

[[nodiscard]] bool is_option(std::string_view value)
{
    return value.starts_with("-");
}

[[nodiscard]] std::filesystem::path require_value(int& index, int argc, char* argv[], std::string_view option_name)
{
    if ((index + 1) >= argc || is_option(argv[index + 1])) {
        throw std::invalid_argument{"Missing value for option " + std::string{option_name}};
    }

    ++index;
    return std::filesystem::path{argv[index]};
}

[[nodiscard]] CliOptions parse_args(int argc, char* argv[])
{
    CliOptions options;

    if (argc == 1) {
        options.show_help = true;
        return options;
    }

    for (int index = 1; index < argc; ++index) {
        const auto arg = std::string_view{argv[index]};

        if (arg == "-h" || arg == "--help") {
            options.show_help = true;
        } else if (arg == "--ais") {
            options.ais_path = require_value(index, argc, argv, arg);
        } else if (arg == "--protected-areas") {
            options.protected_areas_path = require_value(index, argc, argv, arg);
        } else if (arg == "--output") {
            options.csv_output_path = require_value(index, argc, argv, arg);
        } else if (arg == "--markdown-output" || arg == "--text-output") {
            options.markdown_output_path = require_value(index, argc, argv, arg);
        } else {
            throw std::invalid_argument{"Unknown argument: " + std::string{arg}};
        }
    }

    if (!options.show_help) {
        if (options.ais_path.empty()) {
            throw std::invalid_argument{"Missing required option --ais"};
        }

        if (options.csv_output_path.empty()) {
            throw std::invalid_argument{"Missing required option --output"};
        }
    }

    return options;
}

[[nodiscard]] std::filesystem::path default_markdown_report_path(const std::filesystem::path& csv_output_path)
{
    auto markdown_path = csv_output_path;
    markdown_path.replace_extension(".md");
    return markdown_path;
}

[[nodiscard]] std::vector<oceanwatchai::VesselAnalysisReport> analyse_vessels(
    const std::vector<oceanwatchai::VesselTrack>& tracks,
    const std::vector<oceanwatchai::ProtectedArea>& protected_areas,
    bool analyse_protected_areas)
{
    const oceanwatchai::FeatureExtractor feature_extractor;
    const oceanwatchai::RiskScoringEngine risk_scoring_engine;
    const oceanwatchai::ProtectedAreaProximityAnalyzer protected_area_analyzer;

    std::vector<oceanwatchai::VesselAnalysisReport> reports;
    reports.reserve(tracks.size());

    for (const auto& track : tracks) {
        auto features = feature_extractor.extract(track);
        auto risk_score = risk_scoring_engine.score(features);

        std::optional<oceanwatchai::ProtectedAreaProximityResult> protected_area_result;
        if (analyse_protected_areas) {
            protected_area_result = protected_area_analyzer.analyze_track(track, protected_areas);
        }

        reports.push_back(oceanwatchai::VesselAnalysisReport{
            .features = std::move(features),
            .risk_score = std::move(risk_score),
            .protected_area_result = std::move(protected_area_result),
        });
    }

    return reports;
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        const auto options = parse_args(argc, argv);
        if (options.show_help) {
            std::cout << usage_text();
            return EXIT_SUCCESS;
        }

        const auto ais_data = oceanwatchai::load_ais_csv(options.ais_path);
        for (const auto& warning : ais_data.warnings) {
            std::cerr << "warning: " << warning.message << '\n';
        }

        std::vector<oceanwatchai::ProtectedArea> protected_areas;
        if (options.protected_areas_path) {
            protected_areas = oceanwatchai::load_protected_areas_csv(*options.protected_areas_path);
        }

        const auto reports = analyse_vessels(ais_data.tracks, protected_areas, options.protected_areas_path.has_value());
        const auto markdown_report_path =
            options.markdown_output_path.value_or(default_markdown_report_path(options.csv_output_path));

        const oceanwatchai::ReportMetadata metadata{
            .ais_path = options.ais_path,
            .protected_areas_path = options.protected_areas_path,
            .ais_point_count = ais_data.point_count(),
            .ais_warning_count = ais_data.warnings.size(),
            .protected_area_count = protected_areas.size(),
        };

        oceanwatchai::write_csv_report(options.csv_output_path, reports);
        oceanwatchai::write_markdown_report(markdown_report_path, metadata, reports);

        std::cout << "Analysed " << reports.size() << " vessels from " << ais_data.point_count() << " AIS points.\n";
        std::cout << "CSV report: " << options.csv_output_path.string() << '\n';
        std::cout << "Markdown report: " << markdown_report_path.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n\n";
        std::cerr << "Run 'OceanWatchAI.exe --help' for usage.\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
