#pragma once

#include "Request.h"

#include <optional>

enum class EventType {
    UrgentRequest,
    CancelRequest,
    EngineerUnavailable
};

struct Event {
    EventType type;

    int time;

    std::optional<int> requestId;
    std::optional<int> engineerId;

    std::optional<Request> newRequest;
};