#pragma once

#include "model/Point.h"
#include "model/Enums.h"

struct TravelInfo {
    double distanceKm = 0.0;
    int travelMinutes = 0;
};

class Router {
public:
    TravelInfo getTravel(
        const Point& from,
        const Point& to,
        Transport transport
    ) const;
};