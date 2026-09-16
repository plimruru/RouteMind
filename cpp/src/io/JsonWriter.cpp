#include "io/JsonWriter.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

using json = nlohmann::json;


void JsonWriter::writePlan(
    const Plan& plan,
    const std::string& path
) {
    json output;

    output["routes"] = json::array();
    output["unassigned"] = json::array();

    for (const auto& route : plan.routes) {
        json routeJson;

        routeJson["engineer_id"] =
            route.engineerId;

        routeJson["total_distance_km"] =
            route.totalDistanceKm;

        routeJson["total_travel_minutes"] =
            route.totalTravelMinutes;

        routeJson["requests"] = json::array();

        for (const auto& request :
             route.requests) {

            json requestJson;

            requestJson["request_id"] =
                request.requestId;

            requestJson["arrival_time"] =
                request.arrivalTime;

            requestJson["start_time"] =
                request.startTime;

            requestJson["finish_time"] =
                request.finishTime;

            requestJson["distance_from_previous_km"] =
                request.distanceFromPreviousKm;

            routeJson["requests"].push_back(
                requestJson
            );
        }

        output["routes"].push_back(
            routeJson
        );
    }

    for (const auto& request :
         plan.unassigned) {

        json requestJson;

        requestJson["request_id"] =
            request.requestId;

        requestJson["reason"] =
            request.reason;

        output["unassigned"].push_back(
            requestJson
        );
    }

    output["metrics"]["engineers_used"] =
        plan.metrics.engineersUsed;

    output["metrics"]["total_distance_km"] =
        plan.metrics.totalDistanceKm;


    std::ofstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open output file: " + path
        );
    }

    file << output.dump(4);
}