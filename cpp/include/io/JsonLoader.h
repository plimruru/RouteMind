#pragma once

#include "../model/Request.h"
#include "../model/Engineer.h"

#include <string>
#include <vector>

class JsonLoader {
public:

    static std::vector<Request> loadRequests(
        const std::string& path
    );

    static std::vector<Engineer> loadEngineers(
        const std::string& path
    );
};