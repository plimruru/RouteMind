#pragma once

#include "../model/Request.h"
#include "../model/Engineer.h"
#include "../model/Event.h"
#include "../model/Plan.h"

#include <vector>

class Planner {
public:

    Plan solve(
        const std::vector<Request>& requests,
        const std::vector<Engineer>& engineers
    );

    Plan replan(
        const std::vector<Request>& requests,
        const std::vector<Engineer>& engineers,
        const Event& event
    );
};