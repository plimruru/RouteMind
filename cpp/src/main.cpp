#include <iostream>
#include <map>
#include <algorithm>

#include "planner/Planner.h"
#include "planner/Scheduler.h"
#include "routing/Router.h"
#include "io/JsonLoader.h"
#include "io/JsonWriter.h"

int main() {

    try {

        std::cout << "Starting planner...\n";

        auto requests =
            JsonLoader::loadRequests(
                "data/requests.json"
            );

        std::cout
            << "Loaded requests: "
            << requests.size()
            << "\n";

        if (!requests.empty()) {

            std::cout
                << "First request location: "
                << requests.front().location.lat
                << ", "
                << requests.front().location.lon
                << "\n";
        }

        auto engineers =
            JsonLoader::loadEngineers(
                "data/brigades.json"
            );

        std::cout
            << "Loaded engineers: "
            << engineers.size()
            << "\n";


        Planner planner;

        Plan plan =
            planner.solve(
                requests,
                engineers
            );

        {
        Scheduler scheduler;
        Router router;

        std::cout << "\n=== Final time-failure diagnostics ===\n";

        for (const auto& unassigned : plan.unassigned) {
            if (
                unassigned.reason !=
                "No feasible time slot"
            ) {
                continue;
            }

            auto requestIt = std::find_if(
                requests.begin(),
                requests.end(),
                [&](const Request& request) {
                    return request.id == unassigned.requestId;
                }
            );

            if (requestIt == requests.end()) {
                continue;
            }

            const Request& request = *requestIt;

            int compatibleEngineers = 0;
            int freshFeasibleEngineers = 0;

            std::string firstFailureReason;

            for (const auto& engineer : engineers) {
                const bool skillOk =
                    std::find(
                        engineer.skills.begin(),
                        engineer.skills.end(),
                        request.requiredSkill
                    ) != engineer.skills.end();

                const bool specializationOk =
                    request.requiredSpecialization.empty() ||
                    std::find(
                        engineer.specializations.begin(),
                        engineer.specializations.end(),
                        request.requiredSpecialization
                    ) != engineer.specializations.end();

                const bool transportOk =
                    !request.requiredTransport.has_value() ||
                    engineer.transport ==
                        *request.requiredTransport;

                if (
                    !skillOk ||
                    !specializationOk ||
                    !transportOk
                ) {
                    continue;
                }

                ++compatibleEngineers;

                const ScheduleResult result =
                    scheduler.schedule(
                        engineer.startLocation,
                        engineer,
                        request,
                        engineer.shiftStart,
                        router
                    );

                if (result.feasible) {
                    ++freshFeasibleEngineers;
                } else if (firstFailureReason.empty()) {
                    firstFailureReason =
                        result.reason;
                }
            }

            std::cout
                << "\nRequest: "
                << request.id
                << "\n";

            std::cout
                << "  Window: "
                << request.windowStart
                << "-"
                << request.windowEnd
                << "\n";

            std::cout
                << "  Duration: "
                << request.durationMinutes
                << " min\n";

            std::cout
                << "  Compatible engineers: "
                << compatibleEngineers
                << "\n";

            std::cout
                << "  Fresh feasible engineers: "
                << freshFeasibleEngineers
                << "\n";

            if (!firstFailureReason.empty()) {
                std::cout
                    << "  Example failure: "
                    << firstFailureReason
                    << "\n";
            }

            if (freshFeasibleEngineers > 0) {
                std::cout
                    << "  CLASSIFICATION: "
                    << "POTENTIALLY RECOVERABLE\n";
            } else {
                std::cout
                    << "  CLASSIFICATION: "
                    << "INTRINSICALLY TIME-INFEASIBLE\n";
            }
        }
    }


        std::cout
            << "\nPlan created.\n";

        std::cout
            << "Routes: "
            << plan.routes.size()
            << "\n";

        std::cout
            << "Unassigned requests: "
            << plan.unassigned.size()
            << "\n";


        std::cout
            << "\nMetrics:\n";

        std::cout
            << "Engineers used: "
            << plan.metrics.engineersUsed
            << "\n";

        std::cout
            << "Total distance: "
            << plan.metrics.totalDistanceKm
            << " km\n";


        std::cout
            << "\nExplanations: "
            << plan.explanations.size()
            << "\n";

        std::map<std::string, int> reasonCounts;

        for (const auto& item : plan.unassigned) {
            ++reasonCounts[item.reason];
        }

        std::cout << "\nUnassigned reasons:\n";

        for (const auto& [reason, count] : reasonCounts) {
            std::cout << count << " " << reason << "\n";
        }

        JsonWriter::writePlan(
            plan,
            "output/plan.json"
        );

        std::cout
            << "\nPlan written to "
               "output/plan.json\n";


    }
    catch (const std::exception& e) {

        std::cerr
            << "ERROR: "
            << e.what()
            << "\n";

        return 1;
    }

    return 0;
}