#include "metrics/Metrics.h"

void calculateMetrics(Plan& plan) {

    plan.metrics.engineersUsed = 0;

    plan.metrics.totalDistanceKm = 0.0;

    for (const auto& route : plan.routes) {

        if (route.requests.empty()) {
            continue;
        }

        ++plan.metrics.engineersUsed;

        plan.metrics.totalDistanceKm +=
            route.totalDistanceKm;
    }
}