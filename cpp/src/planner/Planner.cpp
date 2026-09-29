#include "planner/Planner.h"

#include "planner/Repairer.h"
#include "metrics/Metrics.h"
#include "planner/Scheduler.h"
#include "routing/Router.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

bool hasSkill(
    const Engineer& engineer,
    Skill required
) {
    return std::find(
        engineer.skills.begin(),
        engineer.skills.end(),
        required
    ) != engineer.skills.end();
}

bool hasSpecialization(
    const Engineer& engineer,
    const std::string& required
) {
    if (required.empty()) {
        return true;
    }

    return std::find(
        engineer.specializations.begin(),
        engineer.specializations.end(),
        required
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

struct WorkingRoute {
    const Engineer* engineer = nullptr;

    Point currentLocation{0.0, 0.0};
    int currentTime = 0;

    double totalDistanceKm = 0.0;
    int totalTravelMinutes = 0;

    std::vector<ScheduledRequest> requests;
};

bool canAssign(
    const Request& request,
    const WorkingRoute& route
) {
    const Engineer& engineer = *route.engineer;

    if (!hasSkill(engineer, request.requiredSkill)) {
        return false;
    }

    if (!hasSpecialization(
            engineer,
            request.requiredSpecialization
        )) {
        return false;
    }

    if (!transportMatches(engineer, request)) {
        return false;
    }

    return true;
}

void assignRequest(
    const Request& request,
    WorkingRoute& route,
    const ScheduleResult& schedule
) {
    ScheduledRequest scheduled;

    scheduled.requestId = request.id;
    scheduled.arrivalTime = schedule.arrivalTime;
    scheduled.startTime = schedule.startTime;
    scheduled.finishTime = schedule.finishTime;
    scheduled.distanceFromPreviousKm =
        schedule.distanceFromPreviousKm;

    route.requests.push_back(scheduled);

    route.currentLocation = request.location;
    route.currentTime = schedule.finishTime;

    route.totalDistanceKm +=
        schedule.distanceFromPreviousKm;

    route.totalTravelMinutes +=
        schedule.travelMinutes;
}

struct Candidate {
    std::size_t routeIndex = 0;
    ScheduleResult schedule;
};

struct EvaluatedRequest {
    std::size_t requestIndex = 0;
    std::vector<Candidate> candidates;
};

std::vector<Candidate> findCandidates(
    const Request& request,
    const std::vector<WorkingRoute>& routes,
    const Router& router,
    const Scheduler& scheduler
) {
    std::vector<Candidate> candidates;

    for (std::size_t routeIndex = 0;
         routeIndex < routes.size();
         ++routeIndex) {

        const WorkingRoute& route = routes[routeIndex];

        if (!canAssign(request, route)) {
            continue;
        }

        ScheduleResult schedule =
            scheduler.schedule(
                route.currentLocation,
                *route.engineer,
                request,
                route.currentTime,
                router
            );

        if (!schedule.feasible) {
            continue;
        }

        Candidate candidate;
        candidate.routeIndex = routeIndex;
        candidate.schedule = schedule;

        candidates.push_back(candidate);
    }

    return candidates;
}

bool containsRoute(
    const std::vector<Candidate>& candidates,
    std::size_t routeIndex
) {
    for (const auto& candidate : candidates) {
        if (candidate.routeIndex == routeIndex) {
            return true;
        }
    }

    return false;
}

int countFutureOptions(
    std::size_t routeIndex,
    std::size_t selectedRequestIndex,
    const std::vector<EvaluatedRequest>& evaluated
) {
    int result = 0;

    for (const auto& item : evaluated) {
        if (item.requestIndex == selectedRequestIndex) {
            continue;
        }

        if (containsRoute(item.candidates, routeIndex)) {
            ++result;
        }
    }

    return result;
}

bool betterCandidate(
    const Candidate& candidate,
    const Candidate& currentBest,
    const std::vector<EvaluatedRequest>& evaluated,
    std::size_t requestIndex
) {
    const int candidateFutureOptions =
        countFutureOptions(
            candidate.routeIndex,
            requestIndex,
            evaluated
        );

    const int bestFutureOptions =
        countFutureOptions(
            currentBest.routeIndex,
            requestIndex,
            evaluated
        );

    /*
     * Сначала отдаём предпочтение инженеру,
     * который является вариантом для большего числа
     * других ещё не назначенных заявок.
     *
     * Это оставляет дефицитных инженеров свободными
     * для следующих заявок.
     */
    if (candidateFutureOptions != bestFutureOptions) {
        return candidateFutureOptions >
               bestFutureOptions;
    }

    /*
     * Если гибкость одинаковая — выбираем более
     * дешёвый по времени вариант.
     */
    const int candidateCost =
        candidate.schedule.travelMinutes +
        (candidate.schedule.startTime -
         candidate.schedule.arrivalTime);

    const int bestCost =
        currentBest.schedule.travelMinutes +
        (currentBest.schedule.startTime -
         currentBest.schedule.arrivalTime);

    if (candidateCost != bestCost) {
        return candidateCost < bestCost;
    }

    /*
     * Финальный tie-breaker — инженер,
     * который раньше освобождается.
     */
    return candidate.schedule.finishTime <
           currentBest.schedule.finishTime;
}

std::string unassignedReason(
    const Request& request,
    const std::vector<Engineer>& engineers
) {
    bool skillFound = false;
    bool specializationFound = false;
    bool transportFound = false;
    bool allConstraintsFound = false;

    for (const auto& engineer : engineers) {
        const bool skill =
            hasSkill(
                engineer,
                request.requiredSkill
            );

        const bool specialization =
            hasSpecialization(
                engineer,
                request.requiredSpecialization
            );

        const bool transport =
            transportMatches(
                engineer,
                request
            );

        if (skill) {
            skillFound = true;
        }

        if (specialization) {
            specializationFound = true;
        }

        if (transport) {
            transportFound = true;
        }

        /*
         * Важно:
         * проверяем все ограничения одновременно
         * на одном инженере.
         */
        if (skill && specialization && transport) {
            allConstraintsFound = true;
        }
    }

    if (!skillFound) {
        return "No engineer with required skill";
    }

    if (!specializationFound) {
        return "No engineer with required specialization";
    }

    if (!transportFound) {
        return "No engineer with required transport";
    }

    if (!allConstraintsFound) {
        return
            "No engineer satisfies all "
            "skill, specialization and transport requirements";
    }

    return "No feasible time slot";
}

void addAssignedExplanation(
    std::optional<RequestExplanation>& slot,
    const Request& request,
    const Engineer& engineer,
    const ScheduleResult& schedule,
    const std::vector<EvaluatedRequest>& evaluated
) {
    RequestExplanation explanation;

    explanation.requestId = request.id;
    explanation.assigned = true;
    explanation.engineerId = engineer.id;

    explanation.reasons.push_back(
        "Selected by constrained-first scheduling"
    );

    explanation.reasons.push_back(
        "Required skill is available"
    );

    if (!request.requiredSpecialization.empty()) {
        explanation.reasons.push_back(
            "Required specialization is available"
        );
    }

    if (request.requiredTransport.has_value()) {
        explanation.reasons.push_back(
            "Required transport is available"
        );
    }

    explanation.reasons.push_back(
        "Time window is satisfied"
    );

    explanation.reasons.push_back(
        "Engineer shift is sufficient"
    );

    /*
     * Оцениваем, сколько альтернативных заявок
     * ещё могли использовать выбранного инженера.
     */
    int futureOptions = 0;

    for (const auto& item : evaluated) {
        if (item.requestIndex == 0) {
            // requestIndex = 0 может быть настоящим индексом,
            // поэтому здесь ничего не делаем.
        }

        for (const auto& candidate : item.candidates) {
            if (candidate.routeIndex == 0) {
                // Аналогично: routeIndex = 0 валиден.
            }
        }
    }

    /*
     * Для основной пользовательской диагностики достаточно
     * сообщить сам факт оптимизационного выбора.
     *
     * Детальные числовые причины будут добавлены позже
     * при необходимости.
     */
    (void)futureOptions;

    if (schedule.startTime > schedule.arrivalTime) {
        explanation.reasons.push_back(
            "Waiting for request time window"
        );
    }

    slot = explanation;
}

} // namespace

Plan Planner::solve(
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers
) {
    Plan plan;

    std::vector<WorkingRoute> workingRoutes;
    workingRoutes.reserve(engineers.size());

    for (const auto& engineer : engineers) {
        WorkingRoute route;

        route.engineer = &engineer;
        route.currentLocation = engineer.startLocation;
        route.currentTime = engineer.shiftStart;

        workingRoutes.push_back(route);
    }

    Router router;
    Scheduler scheduler;

    /*
     * Храним индексы заявок, которые ещё не назначены.
     *
     * Заявки с геолокацией будут обработаны алгоритмом.
     * Заявки без геолокации сразу попадут в unassigned.
     */
    std::vector<std::size_t> pending;

    for (std::size_t i = 0;
         i < requests.size();
         ++i) {

        const Request& request = requests[i];

        if (!request.hasLocation) {
            UnassignedRequest unassigned;

            unassigned.requestId = request.id;
            unassigned.reason = "Missing geolocation";

            plan.unassigned.push_back(unassigned);

            RequestExplanation explanation;

            explanation.requestId = request.id;
            explanation.assigned = false;
            explanation.engineerId = "";
            explanation.reasons.push_back(
                "Missing geolocation"
            );

            plan.explanations.push_back(explanation);

            continue;
        }

        pending.push_back(i);
    }

    /*
     * Основной алгоритм:
     *
     * 1. Для каждой оставшейся заявки считаем все текущие
     *    feasible-кандидаты.
     *
     * 2. Выбираем заявку с наименьшим количеством
     *    кандидатов.
     *
     * 3. Если несколько заявок одинаково ограничены,
     *    берём более узкое временное окно.
     *
     * 4. Для выбранной заявки выбираем инженера,
     *    который оставляет максимальную гибкость
     *    для остальных заявок.
     *
     * 5. При равенстве — минимизируем travel + waiting.
     */
    while (!pending.empty()) {
        std::vector<EvaluatedRequest> evaluated;
        evaluated.reserve(pending.size());

        for (const std::size_t requestIndex : pending) {
            EvaluatedRequest item;

            item.requestIndex = requestIndex;

            item.candidates =
                findCandidates(
                    requests[requestIndex],
                    workingRoutes,
                    router,
                    scheduler
                );

            evaluated.push_back(item);
        }

        /*
         * Ищем самую ограниченную заявку.
         */
        std::size_t bestEvaluatedIndex = 0;

        for (std::size_t i = 1;
             i < evaluated.size();
             ++i) {

            const Request& currentRequest =
                requests[
                    evaluated[i].requestIndex
                ];

            const Request& bestRequest =
                requests[
                    evaluated[bestEvaluatedIndex].requestIndex
                ];

            const std::size_t currentCandidateCount =
                evaluated[i].candidates.size();

            const std::size_t bestCandidateCount =
                evaluated[
                    bestEvaluatedIndex
                ].candidates.size();

            if (currentCandidateCount <
                bestCandidateCount) {

                bestEvaluatedIndex = i;
                continue;
            }

            if (currentCandidateCount >
                bestCandidateCount) {

                continue;
            }

            // При перепланировании срочные заявки обслуживаются раньше
            // остальных, если число доступных исполнителей одинаково.
            if (currentRequest.priority != bestRequest.priority) {
                if (currentRequest.priority == Priority::Urgent) {
                    bestEvaluatedIndex = i;
                }
                continue;
            }

            /*
             * При одинаковом числе кандидатов
             * сначала обрабатываем более узкое окно.
             */
            const int currentWindow =
                currentRequest.windowEnd -
                currentRequest.windowStart;

            const int bestWindow =
                bestRequest.windowEnd -
                bestRequest.windowStart;

            if (currentWindow < bestWindow) {
                bestEvaluatedIndex = i;
                continue;
            }

            if (currentWindow > bestWindow) {
                continue;
            }

            /*
             * При одинаковом окне сначала обслуживаем
             * более длинную заявку.
             */
            if (currentRequest.durationMinutes >
                bestRequest.durationMinutes) {

                bestEvaluatedIndex = i;
            }
        }

        EvaluatedRequest selected =
            evaluated[bestEvaluatedIndex];

        const std::size_t requestIndex =
            selected.requestIndex;

        const Request& request =
            requests[requestIndex];

        /*
         * Если кандидатов нет уже в текущем состоянии,
         * дальнейшее добавление заявок этот слот не улучшит.
         */
        if (selected.candidates.empty()) {
            UnassignedRequest unassigned;

            unassigned.requestId = request.id;
            unassigned.reason =
                unassignedReason(
                    request,
                    engineers
                );

            plan.unassigned.push_back(unassigned);

            RequestExplanation explanation;

            explanation.requestId = request.id;
            explanation.assigned = false;
            explanation.engineerId = "";
            explanation.reasons.push_back(
                unassigned.reason
            );

            /*
             * Отдельно отмечаем, что заявка была проверена
             * уже после применения предыдущих назначений.
             */
            explanation.reasons.push_back(
                "No feasible engineer in current route state"
            );

            plan.explanations.push_back(explanation);

            pending.erase(
                std::remove(
                    pending.begin(),
                    pending.end(),
                    requestIndex
                ),
                pending.end()
            );

            continue;
        }

        /*
         * Выбираем конкретного инженера.
         */
        Candidate bestCandidate =
            selected.candidates.front();

        for (std::size_t i = 1;
             i < selected.candidates.size();
             ++i) {

            const Candidate& candidate =
                selected.candidates[i];

            if (betterCandidate(
                    candidate,
                    bestCandidate,
                    evaluated,
                    requestIndex
                )) {

                bestCandidate = candidate;
            }
        }

        WorkingRoute& selectedRoute =
            workingRoutes[
                bestCandidate.routeIndex
            ];

        assignRequest(
            request,
            selectedRoute,
            bestCandidate.schedule
        );

        RequestExplanation explanation;

        explanation.requestId = request.id;
        explanation.assigned = true;
        explanation.engineerId =
            selectedRoute.engineer->id;

        explanation.reasons.push_back(
            "Selected by constrained-first scheduling"
        );

        explanation.reasons.push_back(
            "Required skill is available"
        );

        if (!request.requiredSpecialization.empty()) {
            explanation.reasons.push_back(
                "Required specialization is available"
            );
        }

        if (request.requiredTransport.has_value()) {
            explanation.reasons.push_back(
                "Required transport is available"
            );
        }

        explanation.reasons.push_back(
            "Time window is satisfied"
        );

        explanation.reasons.push_back(
            "Engineer shift is sufficient"
        );

        explanation.reasons.push_back(
            "Engineer selected while preserving "
            "flexibility for other requests"
        );

        if (bestCandidate.schedule.startTime >
            bestCandidate.schedule.arrivalTime) {

            explanation.reasons.push_back(
                "Waiting for request time window"
            );
        }

        plan.explanations.push_back(explanation);

        pending.erase(
            std::remove(
                pending.begin(),
                pending.end(),
                requestIndex
            ),
            pending.end()
        );
    }

    /*
     * Формируем финальные маршруты.
     */
    for (const auto& working : workingRoutes) {
        if (working.requests.empty()) {
            continue;
        }

        Route route;

        route.engineerId =
            working.engineer->id;

        route.requests =
            working.requests;

        route.totalDistanceKm =
            working.totalDistanceKm;

        route.totalTravelMinutes =
            working.totalTravelMinutes;

        plan.routes.push_back(route);
    }

    Repairer repairer;
    repairer.repair(
        plan,
        requests,
        engineers
    );

    calculateMetrics(plan);

    return plan;
}

Plan Planner::replan(
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers,
    const Event& event
) {
    std::vector<Request> updatedRequests = requests;
    std::vector<Engineer> updatedEngineers = engineers;

    if (event.type == EventType::UrgentRequest) {
        if (event.newRequest.has_value()) {
            updatedRequests.push_back(
                event.newRequest.value()
            );
        }
    }
    else if (event.type == EventType::CancelRequest) {
        if (event.requestId.has_value()) {
            updatedRequests.erase(
                std::remove_if(
                    updatedRequests.begin(),
                    updatedRequests.end(),
                    [&](const Request& request) {
                        return request.id ==
                               event.requestId.value();
                    }
                ),
                updatedRequests.end()
            );
        }
    }

    else if (event.type == EventType::EngineerUnavailable) {
        if (event.engineerId.has_value()) {
            updatedEngineers.erase(
                std::remove_if(
                    updatedEngineers.begin(),
                    updatedEngineers.end(),
                    [&](const Engineer& engineer) {
                        return engineer.id == event.engineerId.value();
                    }
                ),
                updatedEngineers.end()
            );
        }
    }

    return solve(
        updatedRequests,
        updatedEngineers
    );
}

Plan Planner::solveBaseline(
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers
) {
    Plan plan;
    std::vector<WorkingRoute> routes;
    routes.reserve(engineers.size());
    for (const auto& engineer : engineers) {
        routes.push_back({&engineer, engineer.startLocation, engineer.shiftStart});
    }

    Router router;
    Scheduler scheduler;
    for (const auto& request : requests) {
        if (!request.hasLocation) {
            plan.unassigned.push_back({request.id, "Missing geolocation"});
            continue;
        }
        const auto candidates = findCandidates(request, routes, router, scheduler);
        if (candidates.empty()) {
            plan.unassigned.push_back({request.id, unassignedReason(request, engineers)});
            continue;
        }
        // findCandidates сохраняет порядок бригад во входном наборе.
        assignRequest(request, routes[candidates.front().routeIndex], candidates.front().schedule);
    }

    for (const auto& working : routes) {
        if (working.requests.empty()) continue;
        Route route;
        route.engineerId = working.engineer->id;
        route.requests = working.requests;
        route.totalDistanceKm = working.totalDistanceKm;
        route.totalTravelMinutes = working.totalTravelMinutes;
        plan.routes.push_back(route);
    }
    calculateMetrics(plan);
    return plan;
}
