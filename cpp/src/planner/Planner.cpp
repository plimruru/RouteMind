#include "planner/Planner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <utility>

namespace {

constexpr double kEarthRadiusKm = 6371.0;
constexpr double kDistanceEpsilon = 1e-9;

struct Stop {
    const Request* request;
    int arrivalTime;
    int startTime;
    int finishTime;
    double distanceFromPreviousKm;
};

struct WorkingRoute {
    const Engineer* engineer;
    std::vector<const Request*> requests;
    std::vector<Stop> stops;
    double totalDistanceKm = 0.0;
    int totalTravelMinutes = 0;
    int totalWaitingMinutes = 0;
};

struct Candidate {
    std::size_t engineerIndex;
    std::size_t position;
    WorkingRoute route;
    bool activatesEngineer;
    double addedDistanceKm;
};

double radians(double degrees) {
    return degrees * 3.14159265358979323846 / 180.0;
}

double distanceKm(const Point& from, const Point& to) {
    const double dLat = radians(to.lat - from.lat);
    const double dLon = radians(to.lon - from.lon);
    const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0)
        + std::cos(radians(from.lat)) * std::cos(radians(to.lat))
        * std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    return kEarthRadiusKm * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

double speedKmH(Transport transport) {
    switch (transport) {
        case Transport::Car: return 30.0;
        case Transport::Bicycle: return 15.0;
        case Transport::PublicTransport: return 20.0;
        case Transport::Pedestrian: return 5.0;
    }
    return 5.0;
}

int travelMinutes(double kilometers, Transport transport) {
    if (kilometers <= kDistanceEpsilon) {
        return 0;
    }
    return static_cast<int>(std::ceil(kilometers / speedKmH(transport) * 60.0));
}

bool hasSkill(const Engineer& engineer, Skill skill) {
    return std::find(engineer.skills.begin(), engineer.skills.end(), skill)
        != engineer.skills.end();
}

bool compatible(const Request& request, const Engineer& engineer) {
    return hasSkill(engineer, request.requiredSkill)
        && (!request.requiredTransport || engineer.transport == *request.requiredTransport);
}

bool schedule(WorkingRoute& route) {
    route.stops.clear();
    route.totalDistanceKm = 0.0;
    route.totalTravelMinutes = 0;
    route.totalWaitingMinutes = 0;

    Point previousLocation = route.engineer->startLocation;
    int previousFinish = route.engineer->shiftStart;
    for (const Request* request : route.requests) {
        const double legDistance = distanceKm(previousLocation, request->location);
        const int arrival = previousFinish
            + travelMinutes(legDistance, route.engineer->transport);
        const int start = std::max(arrival, request->windowStart);
        const int finish = start + request->durationMinutes;

        if (start > request->windowEnd || finish > route.engineer->shiftEnd) {
            return false;
        }

        route.stops.push_back({request, arrival, start, finish, legDistance});
        route.totalDistanceKm += legDistance;
        route.totalTravelMinutes += arrival - previousFinish;
        route.totalWaitingMinutes += start - arrival;
        previousLocation = request->location;
        previousFinish = finish;
    }
    return true;
}

Route toPublicRoute(const WorkingRoute& route) {
    Route result;
    result.engineerId = route.engineer->id;
    result.totalDistanceKm = route.totalDistanceKm;
    result.totalTravelMinutes = route.totalTravelMinutes;
    for (const Stop& stop : route.stops) {
        result.requests.push_back({stop.request->id, stop.arrivalTime, stop.startTime,
                                   stop.finishTime, stop.distanceFromPreviousKm});
    }
    return result;
}

bool betterCandidate(const Candidate& left, const Candidate& right) {
    // The order is intentional: it encodes the planning objective and makes ties stable.
    if (left.activatesEngineer != right.activatesEngineer) {
        return !left.activatesEngineer;
    }
    if (std::abs(left.addedDistanceKm - right.addedDistanceKm) > kDistanceEpsilon) {
        return left.addedDistanceKm < right.addedDistanceKm;
    }
    if (left.route.totalWaitingMinutes != right.route.totalWaitingMinutes) {
        return left.route.totalWaitingMinutes < right.route.totalWaitingMinutes;
    }
    const int leftFinish = left.route.stops.empty() ? left.route.engineer->shiftStart
                                                     : left.route.stops.back().finishTime;
    const int rightFinish = right.route.stops.empty() ? right.route.engineer->shiftStart
                                                       : right.route.stops.back().finishTime;
    if (leftFinish != rightFinish) {
        return leftFinish < rightFinish;
    }
    if (left.route.engineer->id != right.route.engineer->id) {
        return left.route.engineer->id < right.route.engineer->id;
    }
    return left.position < right.position;
}

std::string unassignedReason(const Request& request,
                             const std::vector<Engineer>& engineers) {
    const bool skillExists = std::any_of(engineers.begin(), engineers.end(),
        [&request](const Engineer& engineer) { return hasSkill(engineer, request.requiredSkill); });
    if (!skillExists) {
        return "Нет исполнителя с требуемым навыком.";
    }
    const bool compatibleExists = std::any_of(engineers.begin(), engineers.end(),
        [&request](const Engineer& engineer) { return compatible(request, engineer); });
    if (!compatibleExists) {
        return "Исполнители с нужным навыком не имеют требуемый тип транспорта.";
    }
    return "Работа не помещается в доступные временные окна и смены инженеров.";
}

} // namespace

Plan Planner::solve(
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers
) {
    Plan plan{};

    std::vector<WorkingRoute> routes;
    routes.reserve(engineers.size());
    for (const Engineer& engineer : engineers) {
        WorkingRoute route{};
        route.engineer = &engineer;
        routes.push_back(std::move(route));
    }

    // The candidate count is computed before routes change, so the request ordering
    // depends only on input data and is reproducible.
    std::vector<const Request*> orderedRequests;
    orderedRequests.reserve(requests.size());
    for (const Request& request : requests) {
        orderedRequests.push_back(&request);
    }
    std::stable_sort(orderedRequests.begin(), orderedRequests.end(),
        [&engineers](const Request* left, const Request* right) {
            if (left->priority != right->priority) {
                return left->priority == Priority::Urgent;
            }
            if (left->windowEnd != right->windowEnd) {
                return left->windowEnd < right->windowEnd;
            }
            const auto countCandidates = [&engineers](const Request& request) {
                return std::count_if(engineers.begin(), engineers.end(),
                    [&request](const Engineer& engineer) { return compatible(request, engineer); });
            };
            const auto leftCount = countCandidates(*left);
            const auto rightCount = countCandidates(*right);
            if (leftCount != rightCount) {
                return leftCount < rightCount;
            }
            return left->id < right->id;
        });

    for (const Request* request : orderedRequests) {
        bool found = false;
        Candidate best{};
        for (std::size_t engineerIndex = 0; engineerIndex < routes.size(); ++engineerIndex) {
            const WorkingRoute& current = routes[engineerIndex];
            if (!compatible(*request, *current.engineer)) {
                continue;
            }
            for (std::size_t position = 0; position <= current.requests.size(); ++position) {
                WorkingRoute proposed = current;
                proposed.requests.insert(proposed.requests.begin() + static_cast<long>(position), request);
                if (!schedule(proposed)) {
                    continue;
                }
                Candidate candidate{engineerIndex, position, std::move(proposed),
                                    current.requests.empty(), 0.0};
                candidate.addedDistanceKm = candidate.route.totalDistanceKm - current.totalDistanceKm;
                if (!found || betterCandidate(candidate, best)) {
                    best = std::move(candidate);
                    found = true;
                }
            }
        }
        if (found) {
            routes[best.engineerIndex] = std::move(best.route);
        } else {
            plan.unassigned.push_back({request->id, unassignedReason(*request, engineers)});
        }
    }

    for (const WorkingRoute& route : routes) {
        if (!route.requests.empty()) {
            plan.metrics.totalDistanceKm += route.totalDistanceKm;
            ++plan.metrics.engineersUsed;
            plan.routes.push_back(toPublicRoute(route));
        }
    }

    return plan;
}

Plan Planner::replan(
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers,
    const Event& event
) {
    (void)requests;
    (void)engineers;
    (void)event;
    Plan plan{};

    return plan;
}
