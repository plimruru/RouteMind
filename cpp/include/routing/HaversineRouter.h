#pragma once

#include "routing/Router.h"
#include "routing/TravelInfo.h"
#include "model/Point.h"
#include "model/Enums.h"

class HaversineRouter : public Router {
public:
    TravelInfo getTravelInfo(
        const Point& from,
        const Point& to,
        Transport transport
    ) const override;

private:
    double calculateDistanceKm(
        const Point& from,
        const Point& to
    ) const;

    double getSpeedKmh(Transport transport) const;
};