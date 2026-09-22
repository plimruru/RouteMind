#include <iostream>

#include "planner/Planner.h"
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