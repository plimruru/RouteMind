#include "planner/Planner.h"

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
// Простая оценка времени поездки
// ------------------------------------------------------------

int travelTimeMinutes(
    const Point& from,
    const Point& to
) {
    const double dx =
        from.lat - to.lat;

    const double dy =
        from.lon - to.lon;

    const double distance =
        std::sqrt(
            dx * dx +
            dy * dy
        );

    // Пока это очень грубая оценка.
    // Настоящий routing подключим позже.
    return static_cast<int>(
        std::ceil(distance * 60.0)
    );
}


// ------------------------------------------------------------
// Простая оценка расстояния
// ------------------------------------------------------------

double distanceKm(
    const Point& from,
    const Point& to
) {
    const double dx =
        from.lat - to.lat;

    const double dy =
        from.lon - to.lon;

    return std::sqrt(
        dx * dx +
        dy * dy
    );
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


    // 1. Проверяем skill
    if (!hasSkill(
            engineer,
            request.requiredSkill)) {

        return false;
    }


    // 2. Проверяем specialization
    if (!hasSpecialization(
            engineer,
            request.requiredSpecialization)) {

        return false;
    }


    // 3. Проверяем транспорт
    if (!transportMatches(
            engineer,
            request)) {

        return false;
    }


    // 4. Считаем время поездки
    const int travel =
        travelTimeMinutes(
            route.currentLocation,
            request.location
        );


    // 5. Время прибытия
    const int arrival =
        route.currentTime + travel;


    // 6. Если приехали раньше начала окна,
    // придется подождать.
    const int start =
        std::max(
            arrival,
            request.windowStart
        );


    // 7. Время окончания работы
    const int finish =
        start + request.durationMinutes;


    // 8. Не успеваем в окно заявки
    if (start > request.windowEnd) {
        return false;
    }


    // 9. Не успеваем закончить до конца смены
    if (finish > engineer.shiftEnd) {
        return false;
    }


    return true;
}


// ------------------------------------------------------------
// Назначение заявки
// ------------------------------------------------------------

void assignRequest(
    const Request& request,
    WorkingRoute& route
) {
    const int travel =
        travelTimeMinutes(
            route.currentLocation,
            request.location
        );


    const int arrival =
        route.currentTime + travel;


    const int start =
        std::max(
            arrival,
            request.windowStart
        );


    const int finish =
        start + request.durationMinutes;


    const double distance =
        distanceKm(
            route.currentLocation,
            request.location
        );


    ScheduledRequest scheduled;

    scheduled.requestId =
        request.id;

    scheduled.arrivalTime =
        arrival;

    scheduled.startTime =
        start;

    scheduled.finishTime =
        finish;

    scheduled.distanceFromPreviousKm =
        distance;


    route.requests.push_back(
        scheduled
    );


    route.currentLocation =
        request.location;

    route.currentTime =
        finish;

    route.totalDistanceKm +=
        distance;

    route.totalTravelMinutes +=
        travel;
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


    // Идем по заявкам
    for (const auto& request : requests) {

        bool assigned = false;


        // Ищем первую подходящую бригаду
        for (auto& route : workingRoutes) {

            if (!canAssign(
                    request,
                    route)) {

                continue;
            }


            assignRequest(
                request,
                route
            );


            assigned = true;

            break;
        }


        // Если подходящей бригады нет
        if (!assigned) {

            UnassignedRequest
                unassigned;

            unassigned.requestId =
                request.id;

            unassigned.reason =
                unassignedReason(
                    request,
                    engineers
                );


            plan.unassigned.push_back(
                unassigned
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
    plan.metrics.engineersUsed =
        static_cast<int>(
            plan.routes.size()
        );


    plan.metrics.totalDistanceKm =
        0.0;


    for (const auto& route :
         plan.routes) {

        plan.metrics.totalDistanceKm +=
            route.totalDistanceKm;
    }


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