#pragma once

#include "Request.h"

#include <optional>
#include <string>

enum class EventType {
    UrgentRequest,
    CancelRequest,
    EngineerUnavailable
};


struct Event {

    EventType type;

    int time;

    std::optional<std::string>
        requestId;

    std::optional<std::string>
        engineerId;

    std::optional<Request>
        newRequest;
};