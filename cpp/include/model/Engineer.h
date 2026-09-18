#pragma once

#include "Point.h"
#include "Enums.h"

#include <string>
#include <vector>

struct Engineer {
    std::string id;

    std::string name;

    Point startLocation;

    int shiftStart;
    int shiftEnd;

    std::vector<Skill> skills;

    std::vector<std::string> specializations;

    Transport transport;

    std::string region;
    std::string homeDistrict;
    std::vector<std::string> servedDistricts;

};