#include <iostream>
#include "planner/Planner.h"
#include "io/JsonLoader.h"
#include "io/JsonWriter.h"

int main(int argc, char* argv[]) {

    try {

        auto requests =
            JsonLoader::loadRequests(
                "data/requests.json"
            );

        auto engineers =
            JsonLoader::loadEngineers(
                "data/brigades.json"
            );

        Planner planner;

        const bool baseline = argc > 1 && std::string(argv[1]) == "--baseline";
        Plan plan = baseline
            ? planner.solveBaseline(requests, engineers)
            : planner.solve(requests, engineers);

        std::cout
            << "Plan created.\n";

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
