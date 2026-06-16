#pragma once

#include "oceanwatchai/AISPoint.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace oceanwatchai {

// Ordered AIS observations for one vessel.
// sort_by_timestamp uses the AISPoint timestamp string, which is suitable for
// ISO-8601 timestamps such as 2026-06-16T05:00:00Z.
class VesselTrack {
public:
    explicit VesselTrack(std::string vessel_id);

    [[nodiscard]] const std::string& vessel_id() const noexcept;
    [[nodiscard]] const std::vector<AISPoint>& points() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    void add_point(AISPoint point);
    void sort_by_timestamp();

private:
    std::string vessel_id_;
    std::vector<AISPoint> points_;
};

} // namespace oceanwatchai
