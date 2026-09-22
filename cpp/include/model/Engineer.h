#pragma once

#include "Point.h"
#include "Enums.h"

#include <string>
#include <vector>

struct Engineer {
    std::string id;
    std::string name;

    Point startLocation{0.0, 0.0};

    int shiftStart = 8 * 60;
    int shiftEnd = 20 * 60;

    std::vector<Skill> skills;

    std::vector<std::string> specializations;

    Transport transport = Transport::Car;

    std::string region;
    std::string homeDistrict;
    std::vector<std::string> servedDistricts;

};