#pragma once

#include "Route.h"

#include <string>
#include <vector>

struct UnassignedRequest {
    std::string requestId;
    std::string reason;
};

struct PlanMetrics {
    int engineersUsed;
    double totalDistanceKm;
};

struct Plan {
    std::vector<Route> routes;

    std::vector<UnassignedRequest> unassigned;

    PlanMetrics metrics;
};