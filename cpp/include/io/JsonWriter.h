#pragma once

#include "../model/Plan.h"

#include <string>

class JsonWriter {
public:

    static void writePlan(
        const Plan& plan,
        const std::string& path
    );
};