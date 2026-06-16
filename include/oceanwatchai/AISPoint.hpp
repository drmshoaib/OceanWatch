#pragma once

#include <string>

namespace oceanwatchai {

// A single AIS observation as reported by a vessel at a point in time.
// Timestamps are expected to use an ISO-8601 sortable representation.
struct AISPoint {
    std::string vessel_id;
    std::string timestamp;
    double latitude{};
    double longitude{};
    double speed_knots{};
    double course_deg{};
};

} // namespace oceanwatchai
