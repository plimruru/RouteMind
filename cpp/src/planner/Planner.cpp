#include "planner/Planner.h"

#include "metrics/Metrics.h"
#include "planner/Scheduler.h"
#include "routing/Router.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

// ------------------------------------------------------------
// Проверка навыка
// ------------------------------------------------------------

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


// ------------------------------------------------------------
// Проверка специализации
// ------------------------------------------------------------

bool hasSpecialization(
    const Engineer& engineer,
    const std::string& required
) {
    // Если специализация не указана,
    // считаем, что дополнительного требования нет.
    if (required.empty()) {
        return true;
    }

    return std::find(
        engineer.specializations.begin(),
        engineer.specializations.end(),
        required
    ) != engineer.specializations.end();
}


// ------------------------------------------------------------
// Проверка транспорта
// ------------------------------------------------------------

bool transportMatches(
    const Engineer& engineer,
    const Request& request
) {
    // Если заявка не требует конкретный транспорт,
    // подходит любой транспорт.
    if (!request.requiredTransport.has_value()) {
        return true;
    }

    return engineer.transport ==
           request.requiredTransport.value();
}

// ------------------------------------------------------------
// Внутреннее состояние маршрута
// ------------------------------------------------------------

struct WorkingRoute {

    const Engineer* engineer = nullptr;

    Point currentLocation{
        0.0,
        0.0
    };

    int currentTime = 0;

    double totalDistanceKm = 0.0;

    int totalTravelMinutes = 0;

    std::vector<ScheduledRequest> requests;
};

// ------------------------------------------------------------
// Можно ли назначить заявку на маршрут
// ------------------------------------------------------------

bool canAssign(
    const Request& request,
    const WorkingRoute& route
) {
    const Engineer& engineer =
        *route.engineer;

    if (!hasSkill(
            engineer,
            request.requiredSkill)) {

        return false;
    }

    if (!hasSpecialization(
            engineer,
            request.requiredSpecialization)) {

        return false;
    }

    if (!transportMatches(
            engineer,
            request)) {

        return false;
    }

    return true;
}

// ------------------------------------------------------------
// Назначение заявки
// ------------------------------------------------------------

void assignRequest(
    const Request& request,
    WorkingRoute& route,
    const ScheduleResult& schedule
) {
    ScheduledRequest scheduled;

    scheduled.requestId =
        request.id;

    scheduled.arrivalTime =
        schedule.arrivalTime;

    scheduled.startTime =
        schedule.startTime;

    scheduled.finishTime =
        schedule.finishTime;

    scheduled.distanceFromPreviousKm =
        schedule.distanceFromPreviousKm;

    route.requests.push_back(
        scheduled
    );

    route.currentLocation =
        request.location;

    route.currentTime =
        schedule.finishTime;

    route.totalDistanceKm +=
        schedule.distanceFromPreviousKm;

    route.totalTravelMinutes +=
        schedule.travelMinutes;
}

// ------------------------------------------------------------
// Причина, почему заявку не удалось назначить
// ------------------------------------------------------------

std::string unassignedReason(
    const Request& request,
    const std::vector<Engineer>& engineers
) {
    bool skillFound = false;

    bool specializationFound = false;

    bool transportFound = false;


    for (const auto& engineer : engineers) {

        if (hasSkill(
                engineer,
                request.requiredSkill)) {

            skillFound = true;
        }


        if (hasSpecialization(
                engineer,
                request.requiredSpecialization)) {

            specializationFound = true;
        }


        if (transportMatches(
                engineer,
                request)) {

            transportFound = true;
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


    return "No feasible time slot";
}

} // namespace


// ============================================================
// Основной планировщик
// ============================================================

Plan Planner::solve(
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers
) {
    Plan plan;


    std::vector<WorkingRoute>
        workingRoutes;


    // Создаем рабочий маршрут для каждой бригады
    for (const auto& engineer : engineers) {

        WorkingRoute route;

        route.engineer =
            &engineer;

        route.currentLocation =
            engineer.startLocation;

        route.currentTime =
            engineer.shiftStart;


        workingRoutes.push_back(
            route
        );
    }

    Router router;
    Scheduler scheduler;

    // Идем по заявкам
    for (const auto& request : requests) {

        if (!request.hasLocation) {

            UnassignedRequest unassigned;

            unassigned.requestId =
                request.id;

            unassigned.reason =
                "Missing geolocation";

            plan.unassigned.push_back(
                unassigned
            );

            RequestExplanation explanation;

            explanation.requestId =
                request.id;

            explanation.assigned = false;

            explanation.engineerId =
                "";

            explanation.reasons.push_back(
                "Missing geolocation"
            );

            plan.explanations.push_back(
                explanation
            );

            continue;
        }

        bool assigned = false;

        // Ищем первую подходящую бригаду
        for (auto& route : workingRoutes) {

            if (!canAssign(
                    request,
                    route)) {

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

            assignRequest(
                request,
                route,
                schedule
            );

            RequestExplanation explanation;

            explanation.requestId =
                request.id;

            explanation.assigned = true;

            explanation.engineerId =
                route.engineer->id;

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

            plan.explanations.push_back(
                explanation
            );

            assigned = true;

            break;
        }


        // Если подходящей бригады нет
        if (!assigned) {

            const std::string reason =
                unassignedReason(
                    request,
                    engineers
                );

            UnassignedRequest unassigned;

            unassigned.requestId =
                request.id;

            unassigned.reason =
                reason;

            plan.unassigned.push_back(
                unassigned
            );


            RequestExplanation explanation;

            explanation.requestId =
                request.id;

            explanation.assigned = false;

            explanation.engineerId =
                "";

            explanation.reasons.push_back(
                reason
            );

            plan.explanations.push_back(
                explanation
            );
        }
    }


    // Переносим рабочие маршруты
    // в итоговый Plan
    for (const auto& working :
         workingRoutes) {

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


        plan.routes.push_back(
            route
        );
    }


    // Метрики
    calculateMetrics(plan);

    return plan;
}


// ============================================================
// Replanning
// ============================================================

Plan Planner::replan(
    const std::vector<Request>& requests,
    const std::vector<Engineer>& engineers,
    const Event& event
) {
    std::vector<Request>
        updatedRequests =
            requests;


    // --------------------------------------------------------
    // Срочная новая заявка
    // --------------------------------------------------------

    if (event.type ==
        EventType::UrgentRequest) {

        if (event.newRequest.has_value()) {

            updatedRequests.push_back(
                event.newRequest.value()
            );
        }
    }


    // --------------------------------------------------------
    // Отмена заявки
    // --------------------------------------------------------

    else if (
        event.type ==
        EventType::CancelRequest
    ) {

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


    // --------------------------------------------------------
    // Пока EngineerUnavailable
    // просто пересчитываем план.
    // Более сложную логику добавим позже.
    // --------------------------------------------------------

    return solve(
        updatedRequests,
        engineers
    );
}