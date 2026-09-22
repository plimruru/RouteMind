#pragma once

#include "model/Engineer.h"
#include "model/Request.h"

#include <string>
#include <vector>

struct CheckResult {
    bool feasible;

    std::vector<std::string> reasons;
};

class Constraints {
public:
    CheckResult check(
        const Engineer& engineer,
        const Request& request
    ) const;

private:
    bool hasSkill(
        const Engineer& engineer,
        Skill requiredSkill
    ) const;

    bool hasRequiredTransport(
        const Engineer& engineer,
        const Request& request
    ) const;
};