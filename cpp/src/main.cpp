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
            << "Plan created.\n";

        std::cout
            << "Routes: "
            << plan.routes.size()
            << "\n";

        std::cout
            << "Unassigned requests: "
            << plan.unassigned.size()
            << "\n";


        JsonWriter::writePlan(
            plan,
            "output/plan.json"
        );

        std::cout
            << "Plan written to output/plan.json\n";


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