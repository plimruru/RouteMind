#pragma once

#include "Point.h"
#include "Enums.h"

#include <string>
#include <vector>

struct Engineer {
    int id;

    std::string name;

    Point startLocation;

    int shiftStart;
    int shiftEnd;

    std::vector<Skill> skills;

    Transport transport;
};