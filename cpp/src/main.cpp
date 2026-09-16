#include "planner/Planner.h"
#include "../include/io/JsonLoader.h"
#include "../include/io/JsonWriter.h"
#include "../include/model/Request.h"
#include "../include/model/Engineer.h"

int main() {

    auto requests =
        JsonLoader::loadRequests("data/requests.json");

    auto engineers =
        JsonLoader::loadEngineers("data/engineers.json");

    Planner planner;

    Plan plan =
        planner.solve(requests, engineers);

    JsonWriter::writePlan(
        plan,
        "output/plan.json"
    );

    return 0;
}