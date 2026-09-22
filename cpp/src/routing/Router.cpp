#include "routing/Router.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double EARTH_RADIUS_KM = 6371.0;

double toRadians(double degrees) {
    return degrees * 3.14159265358979323846 / 180.0;
}

double haversineDistance(
    const Point& from,
    const Point& to
) {
    const double lat1 = toRadians(from.lat);
    const double lat2 = toRadians(to.lat);

    const double dLat =
        toRadians(to.lat - from.lat);

    const double dLon =
        toRadians(to.lon - from.lon);

    const double a =
        std::sin(dLat / 2.0) *
        std::sin(dLat / 2.0) +

        std::cos(lat1) *
        std::cos(lat2) *
        std::sin(dLon / 2.0) *
        std::sin(dLon / 2.0);

    const double c =
        2.0 * std::atan2(
            std::sqrt(a),
            std::sqrt(1.0 - a)
        );

    return EARTH_RADIUS_KM * c;
}

double speedKmh(Transport transport) {
    switch (transport) {
        case Transport::Car:
            return 50.0;

        case Transport::Bicycle:
            return 15.0;

        case Transport::Pedestrian:
            return 5.0;

        case Transport::PublicTransport:
            return 30.0;
    }

    return 30.0;
}

} // namespace

TravelInfo Router::getTravel(
    const Point& from,
    const Point& to,
    Transport transport
) const {

    const double distance =
        haversineDistance(from, to);

    const double speed =
        speedKmh(transport);

    const double hours =
        distance / speed;

    const int minutes =
        static_cast<int>(
            std::ceil(hours * 60.0)
        );

    return {
        distance,
        std::max(1, minutes)
    };
}