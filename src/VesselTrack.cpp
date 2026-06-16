#include "oceanwatchai/VesselTrack.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace oceanwatchai {

VesselTrack::VesselTrack(std::string vessel_id)
    : vessel_id_{std::move(vessel_id)}
{
    if (vessel_id_.empty()) {
        throw std::invalid_argument{"VesselTrack requires a non-empty vessel_id"};
    }
}

const std::string& VesselTrack::vessel_id() const noexcept
{
    return vessel_id_;
}

const std::vector<AISPoint>& VesselTrack::points() const noexcept
{
    return points_;
}

bool VesselTrack::empty() const noexcept
{
    return points_.empty();
}

std::size_t VesselTrack::size() const noexcept
{
    return points_.size();
}

void VesselTrack::add_point(AISPoint point)
{
    if (point.vessel_id != vessel_id_) {
        throw std::invalid_argument{"AISPoint vessel_id does not match VesselTrack vessel_id"};
    }

    points_.push_back(std::move(point));
}

void VesselTrack::sort_by_timestamp()
{
    std::stable_sort(points_.begin(), points_.end(), [](const AISPoint& lhs, const AISPoint& rhs) {
        return lhs.timestamp < rhs.timestamp;
    });
}

} // namespace oceanwatchai
