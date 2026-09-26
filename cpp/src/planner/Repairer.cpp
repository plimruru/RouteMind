#include "planner/Repairer.h"

#include "planner/Scheduler.h"
#include "routing/Router.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct BuildResult {
    bool feasible = false;
    Route route;
};

struct RepairCandidate {
    bool found = false;

    int requestIndex = -1;

    int routeIndex = -1;
    int position = -1;

    int sourceRouteIndex = -1;
    int destinationRouteIndex = -1;
    int victimPosition = -1;

    Route rebuiltRoute;
    Route rebuiltSourceRoute;
    Route rebuiltDestinationRoute;

    double deltaDistanceKm =
        std::numeric_limits<double>::max();
};

struct ChainCandidate {
    bool found = false;

    int targetUnassignedIndex = -1;

    int firstRouteIndex = -1;
    int firstInsertPosition = -1;
    int victimPosition = -1;

    int secondRouteIndex = -1;
    int secondInsertPosition = -1;

    Route rebuiltFirstRoute;
    Route rebuiltSecondRoute;

    double deltaDistanceKm =
        std::numeric_limits<double>::max();
};


// ============================================================
// Compatibility
// ============================================================

bool hasSkill(
    const Engineer& engineer,
    Skill skill
) {
    return std::find(
        engineer.skills.begin(),
        engineer.skills.end(),
        skill
    ) != engineer.skills.end();
}


bool hasSpecialization(
    const Engineer& engineer,
    const std::string& specialization
) {
    if (specialization.empty()) {
        return true;
    }

    return std::find(
        engineer.specializations.begin(),
        engineer.specializations.end(),
        specialization
    ) != engineer.specializations.end();
}


bool transportMatches(
    const Engineer& engineer,
    const Request& request
) {
    if (!request.requiredTransport.has_value()) {
        return true;
    }

    return engineer.transport ==
           request.requiredTransport.value();
}


bool compatible(
    const Engineer& engineer,
    const Request& request
) {
    if (!hasSkill(engineer, request.requiredSkill)) {
        return false;
    }

    if (!hasSpecialization(
            engineer,
            request.requiredSpecialization)) {
        return false;
    }

    if (!transportMatches(engineer, request)) {
        return false;
    }

    return true;
}


// ============================================================
// Find helpers
// ============================================================

const Request* findRequest(
    const std::vector<Request>& requests,
    const std::string& id
) {
    for (const Request& request : requests) {
        if (request.id == id) {
            return &request;
        }
    }

    return nullptr;
}


const Engineer* findEngineer(
    const std::vector<Engineer>& engineers,
    const std::string& id
) {
    for (const Engineer& engineer : engineers) {
        if (engineer.id == id) {
            return &engineer;
        }
    }

    return nullptr;
}


// ============================================================
// Convert route -> request pointers
// ============================================================

std::vector<Request*> routeRequests(
    const Route& route,
    const std::vector<Request>& requests
) {
    std::vector<Request*> result;

    for (const ScheduledRequest& scheduled :
         route.requests) {

        for (const Request& request : requests) {
            if (request.id == scheduled.requestId) {
                result.push_back(
                    const_cast<Request*>(&request)
                );
                break;
            }
        }
    }

    return result;
}


// ============================================================
// Build route from scratch
// ============================================================

BuildResult buildRoute(
    const Engineer& engineer,
    const std::vector<Request*>& requests
) {
    BuildResult result;

    result.route.engineerId = engineer.id;
    result.route.totalDistanceKm = 0.0;
    result.route.totalTravelMinutes = 0;

    if (requests.empty()) {
        result.feasible = true;
        return result;
    }

    Router router;
    Scheduler scheduler;

    Point currentPoint = engineer.startLocation;
    int currentTime = engineer.shiftStart;

    for (const Request* request : requests) {

        if (request == nullptr) {
            result.feasible = false;
            return result;
        }

        // Never repair a request without coordinates.
        if (!request->hasLocation) {
            result.feasible = false;
            return result;
        }

        // Never move a request to an incompatible engineer.
        if (!compatible(engineer, *request)) {
            result.feasible = false;
            return result;
        }

        const ScheduleResult scheduled =
            scheduler.schedule(
                currentPoint,
                engineer,
                *request,
                currentTime,
                router
            );

        if (!scheduled.feasible) {
            result.feasible = false;
            return result;
        }

        ScheduledRequest scheduledRequest;

        scheduledRequest.requestId =
            request->id;

        scheduledRequest.arrivalTime =
            scheduled.arrivalTime;

        scheduledRequest.startTime =
            scheduled.startTime;

        scheduledRequest.finishTime =
            scheduled.finishTime;

        scheduledRequest.distanceFromPreviousKm =
            scheduled.distanceFromPreviousKm;

        result.route.requests.push_back(
            scheduledRequest
        );

        result.route.totalDistanceKm +=
            scheduled.distanceFromPreviousKm;

        result.route.totalTravelMinutes +=
            scheduled.travelMinutes;

        currentPoint =
            request->location;

        currentTime =
            scheduled.finishTime;
    }

    result.feasible = true;
    return result;
}


// ============================================================
// Direct insertion
// ============================================================

bool findBestInsertion(
    const Plan& plan,
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers,
    RepairCandidate& best
) {
    best = RepairCandidate{};

    double bestDelta =
        std::numeric_limits<double>::max();

    for (int unassignedIndex = 0;
         unassignedIndex <
         static_cast<int>(plan.unassigned.size());
         ++unassignedIndex) {

        const std::string& requestId =
            plan.unassigned[unassignedIndex].requestId;

        const Request* request =
            findRequest(requests, requestId);

        if (request == nullptr) {
            continue;
        }

        if (!request->hasLocation) {
            continue;
        }

        for (int routeIndex = 0;
             routeIndex <
             static_cast<int>(plan.routes.size());
             ++routeIndex) {

            const Route& route =
                plan.routes[routeIndex];

            const Engineer* engineer =
                findEngineer(
                    engineers,
                    route.engineerId
                );

            if (engineer == nullptr) {
                continue;
            }

            if (!compatible(*engineer, *request)) {
                continue;
            }

            std::vector<Request*> currentRequests =
                routeRequests(
                    route,
                    requests
                );

            for (int position = 0;
                 position <=
                 static_cast<int>(
                     currentRequests.size()
                 );
                 ++position) {

                std::vector<Request*> candidate =
                    currentRequests;

                candidate.insert(
                    candidate.begin() + position,
                    const_cast<Request*>(request)
                );

                BuildResult rebuilt =
                    buildRoute(
                        *engineer,
                        candidate
                    );

                if (!rebuilt.feasible) {
                    continue;
                }

                const double delta =
                    rebuilt.route.totalDistanceKm -
                    route.totalDistanceKm;

                if (!best.found ||
                    delta < bestDelta) {

                    best.found = true;
                    best.requestIndex =
                        unassignedIndex;

                    best.routeIndex =
                        routeIndex;

                    best.position =
                        position;

                    best.rebuiltRoute =
                        rebuilt.route;

                    best.deltaDistanceKm =
                        delta;

                    bestDelta =
                        delta;
                }
            }
        }
    }

    return best.found;
}


// ============================================================
// One-swap
//
// A = unassigned request
// B = existing request
//
// A goes to B's route.
// B goes to another route.
// ============================================================

bool findBestSwap(
    const Plan& plan,
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers,
    RepairCandidate& best
) {
    best = RepairCandidate{};

    double bestDelta =
        std::numeric_limits<double>::max();

    for (int unassignedIndex = 0;
         unassignedIndex <
         static_cast<int>(plan.unassigned.size());
         ++unassignedIndex) {

        const Request* target =
            findRequest(
                requests,
                plan.unassigned[
                    unassignedIndex
                ].requestId
            );

        if (target == nullptr) {
            continue;
        }

        if (!target->hasLocation) {
            continue;
        }

        for (int sourceRouteIndex = 0;
             sourceRouteIndex <
             static_cast<int>(plan.routes.size());
             ++sourceRouteIndex) {

            const Route& sourceRoute =
                plan.routes[sourceRouteIndex];

            const Engineer* sourceEngineer =
                findEngineer(
                    engineers,
                    sourceRoute.engineerId
                );

            if (sourceEngineer == nullptr) {
                continue;
            }

            if (!compatible(
                    *sourceEngineer,
                    *target)) {
                continue;
            }

            std::vector<Request*> sourceRequests =
                routeRequests(
                    sourceRoute,
                    requests
                );

            for (int victimPosition = 0;
                 victimPosition <
                 static_cast<int>(
                     sourceRequests.size()
                 );
                 ++victimPosition) {

                Request* victim =
                    sourceRequests[victimPosition];

                if (victim == nullptr) {
                    continue;
                }

                std::vector<Request*> withoutVictim =
                    sourceRequests;

                withoutVictim.erase(
                    withoutVictim.begin() +
                    victimPosition
                );

                for (int targetPosition = 0;
                     targetPosition <=
                     static_cast<int>(
                         withoutVictim.size()
                     );
                     ++targetPosition) {

                    std::vector<Request*> candidateSource =
                        withoutVictim;

                    candidateSource.insert(
                        candidateSource.begin() +
                        targetPosition,
                        const_cast<Request*>(target)
                    );

                    BuildResult rebuiltSource =
                        buildRoute(
                            *sourceEngineer,
                            candidateSource
                        );

                    if (!rebuiltSource.feasible) {
                        continue;
                    }

                    for (int destinationRouteIndex = 0;
                         destinationRouteIndex <
                         static_cast<int>(
                             plan.routes.size()
                         );
                         ++destinationRouteIndex) {

                        if (destinationRouteIndex ==
                            sourceRouteIndex) {
                            continue;
                        }

                        const Route& destinationRoute =
                            plan.routes[
                                destinationRouteIndex
                            ];

                        const Engineer* destinationEngineer =
                            findEngineer(
                                engineers,
                                destinationRoute.engineerId
                            );

                        if (destinationEngineer == nullptr) {
                            continue;
                        }

                        if (!compatible(
                                *destinationEngineer,
                                *victim)) {
                            continue;
                        }

                        std::vector<Request*> destinationRequests =
                            routeRequests(
                                destinationRoute,
                                requests
                            );

                        for (int insertPosition = 0;
                             insertPosition <=
                             static_cast<int>(
                                 destinationRequests.size()
                             );
                             ++insertPosition) {

                            std::vector<Request*> candidateDestination =
                                destinationRequests;

                            candidateDestination.insert(
                                candidateDestination.begin() +
                                insertPosition,
                                victim
                            );

                            BuildResult rebuiltDestination =
                                buildRoute(
                                    *destinationEngineer,
                                    candidateDestination
                                );

                            if (!rebuiltDestination.feasible) {
                                continue;
                            }

                            const double oldDistance =
                                sourceRoute.totalDistanceKm +
                                destinationRoute.totalDistanceKm;

                            const double newDistance =
                                rebuiltSource.route.totalDistanceKm +
                                rebuiltDestination.route.totalDistanceKm;

                            const double delta =
                                newDistance - oldDistance;

                            if (!best.found ||
                                delta < bestDelta) {

                                best.found = true;

                                best.requestIndex =
                                    unassignedIndex;

                                best.sourceRouteIndex =
                                    sourceRouteIndex;

                                best.destinationRouteIndex =
                                    destinationRouteIndex;

                                best.victimPosition =
                                    victimPosition;

                                best.rebuiltSourceRoute =
                                    rebuiltSource.route;

                                best.rebuiltDestinationRoute =
                                    rebuiltDestination.route;

                                best.deltaDistanceKm =
                                    delta;

                                bestDelta =
                                    delta;
                            }
                        }
                    }
                }
            }
        }
    }

    return best.found;
}


// ============================================================
// Chain repair depth 2
//
// Target A is unassigned.
//
// First route:
//
//     ... B ...
//
// becomes:
//
//     ... A ...
//
// B is displaced.
//
// Second route:
//
//     ... ...
//
// becomes:
//
//     ... B ...
//
// Therefore:
//
//     A -> Engineer 1
//     B -> Engineer 2
//
// No assigned request disappears.
// ============================================================

bool findBestChain2(
    const Plan& plan,
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers,
    ChainCandidate& best
) {
    best = ChainCandidate{};

    double bestDelta =
        std::numeric_limits<double>::max();

    int targetsChecked = 0;
    int compatibleFirstRoutes = 0;
    int firstRouteFeasible = 0;
    int compatibleSecondRoutes = 0;
    int secondRouteFeasible = 0;

    std::cout
        << "\n=== Chain repair diagnostics ===\n";

    for (int unassignedIndex = 0;
         unassignedIndex <
         static_cast<int>(plan.unassigned.size());
         ++unassignedIndex) {

        const Request* target =
            findRequest(
                requests,
                plan.unassigned[
                    unassignedIndex
                ].requestId
            );

            ++targetsChecked;

            std::cout
                << "\nChain target: "
                << target->id
                << "\n";

        if (target == nullptr) {
            continue;
        }

        if (!target->hasLocation) {
            continue;
        }

        // --------------------------------------------------------
        // First route: target A
        // --------------------------------------------------------

        for (int firstRouteIndex = 0;
             firstRouteIndex <
             static_cast<int>(plan.routes.size());
             ++firstRouteIndex) {

            const Route& firstRoute =
                plan.routes[firstRouteIndex];

            if (firstRoute.requests.empty()) {
                continue;
            }

            const Engineer* firstEngineer =
                findEngineer(
                    engineers,
                    firstRoute.engineerId
                );

            if (firstEngineer == nullptr) {
                continue;
            }

            if (!compatible(
                    *firstEngineer,
                    *target)) {
                continue;
            }
            ++compatibleFirstRoutes;

            std::vector<Request*> firstRequests =
                routeRequests(
                    firstRoute,
                    requests
                );

            // ----------------------------------------------------
            // Pick victim B.
            // ----------------------------------------------------

            for (int victimPosition = 0;
                 victimPosition <
                 static_cast<int>(
                     firstRequests.size()
                 );
                 ++victimPosition) {

                Request* victim =
                    firstRequests[victimPosition];

                if (victim == nullptr) {
                    continue;
                }

                // Remove B from route #1.
                std::vector<Request*> withoutVictim =
                    firstRequests;

                withoutVictim.erase(
                    withoutVictim.begin() +
                    victimPosition
                );

                // ------------------------------------------------
                // Insert A at every possible position.
                // ------------------------------------------------

                for (int targetPosition = 0;
                     targetPosition <=
                     static_cast<int>(
                         withoutVictim.size()
                     );
                     ++targetPosition) {

                    std::vector<Request*> candidateFirst =
                        withoutVictim;

                    candidateFirst.insert(
                        candidateFirst.begin() +
                        targetPosition,
                        const_cast<Request*>(target)
                    );

                    BuildResult rebuiltFirst =
                        buildRoute(
                            *firstEngineer,
                            candidateFirst
                        );

                    if (!rebuiltFirst.feasible) {
                        continue;
                    }

                    ++firstRouteFeasible;

                    std::cout
                        << "  First route feasible: "
                        << firstEngineer->id
                        << ", victim="
                        << victim->id
                        << "\n";

                    // ------------------------------------------------
                    // Second route: move B somewhere else.
                    // ------------------------------------------------

                    for (int secondRouteIndex = 0;
                         secondRouteIndex <
                         static_cast<int>(
                             plan.routes.size()
                         );
                         ++secondRouteIndex) {

                        if (secondRouteIndex ==
                            firstRouteIndex) {
                            continue;
                        }

                        const Route& secondRoute =
                            plan.routes[
                                secondRouteIndex
                            ];

                        const Engineer* secondEngineer =
                            findEngineer(
                                engineers,
                                secondRoute.engineerId
                            );

                        if (secondEngineer == nullptr) {
                            continue;
                        }

                        if (!compatible(
                                *secondEngineer,
                                *victim)) {
                            continue;
                        }

                        ++compatibleSecondRoutes;

                        std::vector<Request*> secondRequests =
                            routeRequests(
                                secondRoute,
                                requests
                            );

                        for (int insertPosition = 0;
                             insertPosition <=
                             static_cast<int>(
                                 secondRequests.size()
                             );
                             ++insertPosition) {

                            std::vector<Request*> candidateSecond =
                                secondRequests;

                            candidateSecond.insert(
                                candidateSecond.begin() +
                                insertPosition,
                                victim
                            );

                            BuildResult rebuiltSecond =
                                buildRoute(
                                    *secondEngineer,
                                    candidateSecond
                                );

                            if (!rebuiltSecond.feasible) {
                                continue;
                            }

                            ++secondRouteFeasible;

                            std::cout
                                << "    Second route feasible: "
                                << secondEngineer->id
                                << ", victim="
                                << victim->id
                                << "\n";

                            const double oldDistance =
                                firstRoute.totalDistanceKm +
                                secondRoute.totalDistanceKm;

                            const double newDistance =
                                rebuiltFirst.route.totalDistanceKm +
                                rebuiltSecond.route.totalDistanceKm;

                            const double delta =
                                newDistance - oldDistance;

                            if (!best.found ||
                                delta < bestDelta) {

                                best.found = true;

                                best.targetUnassignedIndex =
                                    unassignedIndex;

                                best.firstRouteIndex =
                                    firstRouteIndex;

                                best.firstInsertPosition =
                                    targetPosition;

                                best.victimPosition =
                                    victimPosition;

                                best.secondRouteIndex =
                                    secondRouteIndex;

                                best.secondInsertPosition =
                                    insertPosition;

                                best.rebuiltFirstRoute =
                                    rebuiltFirst.route;

                                best.rebuiltSecondRoute =
                                    rebuiltSecond.route;

                                best.deltaDistanceKm =
                                    delta;

                                bestDelta =
                                    delta;
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout
        << "\nChain diagnostics summary:\n"
        << "  targets checked: "
        << targetsChecked
        << "\n"
        << "  compatible first routes: "
        << compatibleFirstRoutes
        << "\n"
        << "  first route feasible: "
        << firstRouteFeasible
        << "\n"
        << "  compatible second routes: "
        << compatibleSecondRoutes
        << "\n"
        << "  second route feasible: "
        << secondRouteFeasible
        << "\n"
        << "  chain found: "
        << (best.found ? "YES" : "NO")
        << "\n";

    if (best.found) {
        std::cout
            << "  best delta distance: "
            << best.deltaDistanceKm
            << " km\n";
    }

    return best.found;
}


// ============================================================
// Remove unassigned request
// ============================================================

void removeFromUnassigned(
    Plan& plan,
    int index
) {
    if (index < 0 ||
        index >=
        static_cast<int>(
            plan.unassigned.size()
        )) {
        return;
    }

    plan.unassigned.erase(
        plan.unassigned.begin() + index
    );
}


// ============================================================
// Update explanation
// ============================================================

void updateExplanation(
    Plan& plan,
    const std::string& requestId,
    const std::string& engineerId
) {
    for (auto& explanation :
         plan.explanations) {

        if (explanation.requestId !=
            requestId) {
            continue;
        }

        explanation.assigned = true;
        explanation.engineerId = engineerId;
        explanation.reasons.clear();

        return;
    }
}

} // namespace


// ============================================================
// Repairer
// ============================================================

void Repairer::repair(
    Plan& plan,
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers
) const {

    while (true) {

        // ========================================================
        // STEP 1
        // Direct insertion
        // ========================================================

        RepairCandidate insertion;

        if (findBestInsertion(
                plan,
                requests,
                engineers,
                insertion)) {

            plan.routes[
                insertion.routeIndex
            ] = insertion.rebuiltRoute;

            const int requestIndex =
                insertion.requestIndex;

            const std::string requestId =
                plan.unassigned[
                    requestIndex
                ].requestId;

            const std::string engineerId =
                plan.routes[
                    insertion.routeIndex
                ].engineerId;

            removeFromUnassigned(
                plan,
                requestIndex
            );

            updateExplanation(
                plan,
                requestId,
                engineerId
            );

            continue;
        }


        // ========================================================
        // STEP 2
        // One swap
        // ========================================================

        RepairCandidate swap;

        if (findBestSwap(
                plan,
                requests,
                engineers,
                swap)) {

            plan.routes[
                swap.sourceRouteIndex
            ] = swap.rebuiltSourceRoute;

            plan.routes[
                swap.destinationRouteIndex
            ] = swap.rebuiltDestinationRoute;

            const int requestIndex =
                swap.requestIndex;

            const std::string requestId =
                plan.unassigned[
                    requestIndex
                ].requestId;

            const std::string engineerId =
                plan.routes[
                    swap.sourceRouteIndex
                ].engineerId;

            removeFromUnassigned(
                plan,
                requestIndex
            );

            updateExplanation(
                plan,
                requestId,
                engineerId
            );

            continue;
        }


        // ========================================================
        // STEP 3
        // Chain repair depth 2
        //
        // A -> Engineer 1
        // B -> Engineer 2
        // ========================================================

        ChainCandidate chain;

        if (findBestChain2(
                plan,
                requests,
                engineers,
                chain)) {

            const int requestIndex =
                chain.targetUnassignedIndex;

            const std::string requestId =
                plan.unassigned[
                    requestIndex
                ].requestId;

            plan.routes[
                chain.firstRouteIndex
            ] = chain.rebuiltFirstRoute;

            plan.routes[
                chain.secondRouteIndex
            ] = chain.rebuiltSecondRoute;

            const std::string engineerId =
                plan.routes[
                    chain.firstRouteIndex
                ].engineerId;

            removeFromUnassigned(
                plan,
                requestIndex
            );

            updateExplanation(
                plan,
                requestId,
                engineerId
            );

            continue;
        }


        // ========================================================
        // Nothing else can be repaired.
        // ========================================================

        break;
    }
}