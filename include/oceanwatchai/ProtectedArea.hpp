#pragma once

#include <string>

namespace oceanwatchai {

struct ProtectedArea {
    std::string name;
    double centre_latitude{};
    double centre_longitude{};
    double radius_km{};
};

} // namespace oceanwatchai
