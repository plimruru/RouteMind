#pragma once

#include "Point.h"
#include "Enums.h"

#include <string>
#include <vector>
#include <string>

struct Engineer {
    int id;

    std::string name;

    Point startLocation;

    int shiftStart;
    int shiftEnd;

    std::vector<Skill> skills;

    Transport transport;

    std::string region;
    std::string homeDistrict;
    std::vector<std::string> servedDistricts;
    std::vector<std::string> specializations;
};