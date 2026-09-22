#pragma once

#include "Point.h"
#include "Enums.h"

#include <optional>
#include <string>

struct Request {
    std::string id;

    Point location{0.0, 0.0};

    bool hasLocation = false;

    int durationMinutes = 60;

    int windowStart = 0;
    int windowEnd = 24 * 60;

    Priority priority = Priority::Normal;

    Skill requiredSkill = Skill::LocalWorks;

    std::optional<Transport> requiredTransport;

    std::string region;
    std::string district;
    std::string address;
    std::string requiredSpecialization;
    std::string description;
};