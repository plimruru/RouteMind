#pragma once

#include "Route.h"
#include "model/Request.h"

#include <string>
#include <vector>

struct UnassignedRequest {
    std::string requestId;
    std::string reason;
};

struct RequestExplanation {
    std::string requestId;

    bool assigned = false;

    std::string engineerId;

    std::vector<std::string> reasons;
};

struct PlanMetrics {
    int engineersUsed = 0;

    double totalDistanceKm = 0.0;
};

struct Plan {
    std::vector<Route> routes;

    std::vector<UnassignedRequest> unassigned;

    std::vector<RequestExplanation> explanations;

    PlanMetrics metrics;
};