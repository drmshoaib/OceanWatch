#include "oceanwatchai/ProtectedAreaProximity.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::istringstream make_stream(std::string csv)
{
    return std::istringstream{std::move(csv)};
}

oceanwatchai::AISPoint make_point(
    std::string timestamp,
    double latitude,
    double longitude)
{
    return oceanwatchai::AISPoint{
        .vessel_id = "FV-PROTECTED-001",
        .timestamp = std::move(timestamp),
        .latitude = latitude,
        .longitude = longitude,
        .speed_knots = 3.0,
        .course_deg = 90.0,
    };
}

oceanwatchai::VesselTrack make_track(std::initializer_list<oceanwatchai::AISPoint> points)
{
    oceanwatchai::VesselTrack track{"FV-PROTECTED-001"};
    for (auto point : points) {
        track.add_point(std::move(point));
    }
    return track;
}

bool contains_text(const std::vector<std::string>& values, std::string_view text)
{
    for (const auto& value : values) {
        if (value.find(text) != std::string::npos) {
            return true;
        }
    }

    return false;
}

} // namespace

TEST_CASE("Protected area CSV loader parses valid reference data")
{
    auto input = make_stream(
        "name,centre_latitude,centre_longitude,radius_km\n"
        "Test Reserve,51.5,1.4,6.0\n"
        "\"Quoted, Area\",51.6,1.5,4.5\n");

    const auto areas = oceanwatchai::load_protected_areas_csv(input);

    REQUIRE(areas.size() == 2);
    CHECK(areas[0].name == "Test Reserve");
    CHECK(areas[0].centre_latitude == Catch::Approx(51.5));
    CHECK(areas[0].centre_longitude == Catch::Approx(1.4));
    CHECK(areas[0].radius_km == Catch::Approx(6.0));
    CHECK(areas[1].name == "Quoted, Area");
}

TEST_CASE("Protected area CSV loader rejects invalid reference data")
{
    auto missing_column = make_stream(
        "name,centre_latitude,centre_longitude\n"
        "Test Reserve,51.5,1.4\n");
    auto invalid_radius = make_stream(
        "name,centre_latitude,centre_longitude,radius_km\n"
        "Test Reserve,51.5,1.4,-1.0\n");

    CHECK_THROWS_AS(oceanwatchai::load_protected_areas_csv(missing_column), oceanwatchai::ProtectedAreaCsvError);
    CHECK_THROWS_AS(oceanwatchai::load_protected_areas_csv(invalid_radius), oceanwatchai::ProtectedAreaCsvError);
}

TEST_CASE("Protected area analyzer detects point entry and near-zone proximity")
{
    const oceanwatchai::ProtectedArea area{
        .name = "Equator Reserve",
        .centre_latitude = 0.0,
        .centre_longitude = 0.0,
        .radius_km = 2.0,
    };
    const std::vector<oceanwatchai::ProtectedArea> areas{area};

    const oceanwatchai::ProtectedAreaProximityAnalyzer analyzer{5.0};
    const auto inside_point = make_point("2026-06-16T05:00:00Z", 0.0, 0.01);
    const auto near_point = make_point("2026-06-16T05:00:00Z", 0.0, 0.04);
    const auto far_point = make_point("2026-06-16T05:00:00Z", 0.0, 0.20);

    CHECK(analyzer.is_inside(inside_point, area));
    CHECK_FALSE(analyzer.is_inside(near_point, area));
    CHECK(analyzer.is_near(near_point, area));
    CHECK_FALSE(analyzer.is_near(far_point, area));

    const auto entered_areas = analyzer.areas_entered_by_point(inside_point, areas);
    REQUIRE(entered_areas.size() == 1);
    CHECK(entered_areas.front() == "Equator Reserve");
}

TEST_CASE("Protected area analyzer estimates time inside and near protected areas")
{
    const std::vector<oceanwatchai::ProtectedArea> areas{
        oceanwatchai::ProtectedArea{
            .name = "Equator Reserve",
            .centre_latitude = 0.0,
            .centre_longitude = 0.0,
            .radius_km = 2.0,
        },
    };
    const auto track = make_track({
        make_point("2026-06-16T00:00:00Z", 0.0, 0.0),
        make_point("2026-06-16T01:00:00Z", 0.0, 0.0),
        make_point("2026-06-16T02:00:00Z", 0.0, 0.04),
        make_point("2026-06-16T03:00:00Z", 0.0, 0.04),
    });

    const oceanwatchai::ProtectedAreaProximityAnalyzer analyzer{5.0, 1.0};
    const auto result = analyzer.analyze_track(track, areas);

    REQUIRE(result.visits.size() == 1);
    const auto& visit = result.visits.front();
    CHECK(visit.area_name == "Equator Reserve");
    CHECK(visit.entered);
    CHECK(visit.points_inside == 2);
    CHECK(visit.points_near == 2);
    CHECK(visit.time_inside_hours == Catch::Approx(1.5));
    CHECK(visit.time_near_hours == Catch::Approx(1.5));
    CHECK(result.total_time_inside_hours() == Catch::Approx(1.5));
    CHECK(result.total_time_near_hours() == Catch::Approx(1.5));
    CHECK(contains_text(result.explanations, "entered protected area"));
    CHECK(contains_text(result.explanations, "loitered inside"));
    CHECK(contains_text(result.explanations, "loitered near"));
}

TEST_CASE("Protected area analyzer reports no indicators when track remains far away")
{
    const std::vector<oceanwatchai::ProtectedArea> areas{
        oceanwatchai::ProtectedArea{
            .name = "Equator Reserve",
            .centre_latitude = 0.0,
            .centre_longitude = 0.0,
            .radius_km = 2.0,
        },
    };
    const auto track = make_track({
        make_point("2026-06-16T00:00:00Z", 0.0, 0.20),
        make_point("2026-06-16T01:00:00Z", 0.0, 0.21),
    });

    const oceanwatchai::ProtectedAreaProximityAnalyzer analyzer{5.0, 1.0};
    const auto result = analyzer.analyze_track(track, areas);

    REQUIRE(result.visits.size() == 1);
    CHECK_FALSE(result.visits.front().entered);
    CHECK(result.visits.front().time_inside_hours == Catch::Approx(0.0));
    CHECK(result.visits.front().time_near_hours == Catch::Approx(0.0));
    REQUIRE(result.explanations.size() == 1);
    CHECK(contains_text(result.explanations, "No protected-area entry"));
}

TEST_CASE("Protected area analyzer rejects unsorted tracks and invalid configuration")
{
    const std::vector<oceanwatchai::ProtectedArea> areas{
        oceanwatchai::ProtectedArea{
            .name = "Equator Reserve",
            .centre_latitude = 0.0,
            .centre_longitude = 0.0,
            .radius_km = 2.0,
        },
    };
    const auto unsorted_track = make_track({
        make_point("2026-06-16T01:00:00Z", 0.0, 0.0),
        make_point("2026-06-16T00:00:00Z", 0.0, 0.0),
    });

    const oceanwatchai::ProtectedAreaProximityAnalyzer analyzer;

    CHECK_THROWS_AS(analyzer.analyze_track(unsorted_track, areas), std::invalid_argument);
    CHECK_THROWS_AS((oceanwatchai::ProtectedAreaProximityAnalyzer{-1.0, 1.0}), std::invalid_argument);
    CHECK_THROWS_AS((oceanwatchai::ProtectedAreaProximityAnalyzer{5.0, -1.0}), std::invalid_argument);
}
