#pragma once

#include "Point.h"
#include "Enums.h"

#include <optional>

struct Request {
    int id;

    Point location;

    int durationMinutes;

    int windowStart;
    int windowEnd;

    Priority priority;

    Skill requiredSkill;

    std::optional<Transport> requiredTransport;
};