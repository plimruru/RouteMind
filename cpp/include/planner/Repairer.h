#pragma once

#include "model/Engineer.h"
#include "model/Plan.h"
#include "model/Request.h"

#include <vector>

class Repairer {
public:
    void repair(
        Plan& plan,
        const std::vector<Request>& requests,
        const std::vector<Engineer>& engineers
    ) const;
};