#pragma once

#include <string>
#include <vector>

struct ScheduledRequest {
    std::string requestId;

    int arrivalTime;
    int startTime;
    int finishTime;

    double distanceFromPreviousKm;
};

struct Route {
    std::string engineerId;

    std::vector<ScheduledRequest> requests;

    double totalDistanceKm;
    int totalTravelMinutes;
};