#pragma once

#include <vector>

struct ScheduledRequest {
    int requestId;

    int arrivalTime;
    int startTime;
    int finishTime;

    double distanceFromPreviousKm;
};

struct Route {
    int engineerId;

    std::vector<ScheduledRequest> requests;

    double totalDistanceKm;
    int totalTravelMinutes;
};