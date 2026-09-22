#include "planner/Scheduler.h"

#include <algorithm>

ScheduleResult Scheduler::schedule(
    const Point& from,
    const Engineer& engineer,
    const Request& request,
    int currentTime,
    const Router& router
) const {

    const TravelInfo travel =
        router.getTravel(
            from,
            request.location,
            engineer.transport
        );

    const int arrival =
        currentTime + travel.travelMinutes;

    const int start =
        std::max(
            arrival,
            request.windowStart
        );

    const int finish =
        start + request.durationMinutes;

    // Даже если приехали в окно,
    // закончить работу нужно до его конца.
    if (finish > request.windowEnd) {

        return {
            false,
            arrival,
            start,
            finish,
            travel.distanceKm,
            travel.travelMinutes,
            "Cannot finish service inside request time window"
        };
    }

    if (finish > engineer.shiftEnd) {

        return {
            false,
            arrival,
            start,
            finish,
            travel.distanceKm,
            travel.travelMinutes,
            "Service exceeds engineer shift"
        };
    }

    return {
        true,
        arrival,
        start,
        finish,
        travel.distanceKm,
        travel.travelMinutes,
        ""
    };
}