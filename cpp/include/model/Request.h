#pragma once

#include "Point.h"
#include "Enums.h"

#include <optional>
#include <string>

struct Request {
    int id;

    Point location;

    int durationMinutes;

    int windowStart;
    int windowEnd;

    Priority priority;

    Skill requiredSkill;

    std::optional<Transport> requiredTransport;

    std::string region;
    std::string district;
    std::string address;
    std::string requiredSpecialization;
};