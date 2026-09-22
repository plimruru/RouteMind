#pragma once

#include "model/Engineer.h"
#include "model/Request.h"
#include "routing/Router.h"

#include <string>

struct ScheduleResult {
    bool feasible = false;

    int arrivalTime = 0;
    int startTime = 0;
    int finishTime = 0;

    double distanceFromPreviousKm = 0.0;
    int travelMinutes = 0;

    std::string reason;
};

class Scheduler {
public:
    ScheduleResult schedule(
        const Point& from,
        const Engineer& engineer,
        const Request& request,
        int currentTime,
        const Router& router
    ) const;
};