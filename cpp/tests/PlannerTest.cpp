#include "planner/Planner.h"

#include <cassert>
#include <vector>

namespace {

Engineer engineer(int id, std::vector<Skill> skills, Transport transport) {
    return {id, "Engineer", {55.75, 37.61}, 8 * 60, 18 * 60, std::move(skills), transport};
}

Request request(int id, int windowStart, int windowEnd, Priority priority,
                Skill skill, std::optional<Transport> transport = std::nullopt) {
    return {id, {55.75, 37.61}, 30, windowStart, windowEnd, priority, skill, transport};
}

} // namespace

int main() {
    Planner planner;
    const std::vector<Engineer> engineers{
        engineer(10, {Skill::LocalWorks}, Transport::Car),
        engineer(20, {Skill::ConnectionWorks}, Transport::Bicycle),
    };
    const std::vector<Request> requests{
        request(1, 10 * 60, 11 * 60, Priority::Normal, Skill::LocalWorks),
        request(2, 8 * 60, 9 * 60, Priority::Urgent, Skill::LocalWorks),
        request(3, 8 * 60, 9 * 60, Priority::Normal, Skill::EmergencyWorks),
        request(4, 8 * 60, 9 * 60, Priority::Normal, Skill::ConnectionWorks, Transport::Car),
    };

    const Plan plan = planner.solve(requests, engineers);

    // The urgent request is planned first, but the later job can be inserted after it.
    assert(plan.routes.size() == 1);
    assert(plan.routes.front().engineerId == 10);
    assert(plan.routes.front().requests.size() == 2);
    assert(plan.routes.front().requests[0].requestId == 2);
    assert(plan.routes.front().requests[1].requestId == 1);
    assert(plan.metrics.engineersUsed == 1);

    assert(plan.unassigned.size() == 2);
    assert(plan.unassigned[0].requestId == 3);
    assert(plan.unassigned[0].reason == "Нет исполнителя с требуемым навыком.");
    assert(plan.unassigned[1].requestId == 4);
    assert(plan.unassigned[1].reason == "Исполнители с нужным навыком не имеют требуемый тип транспорта.");
}
