#include "io/JsonLoader.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

using json = nlohmann::json;


std::vector<Request> JsonLoader::loadRequests(
    const std::string& path
) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open requests file: " + path
        );
    }

    json data;
    file >> data;

    std::vector<Request> requests;

    for (const auto& item : data) {
        Request request;

        request.id = item.at("id");

        request.location.lat =
            item.at("lat");

        request.location.lon =
            item.at("lon");

        request.durationMinutes =
            item.at("duration_minutes");

        request.windowStart =
            item.at("window_start");

        request.windowEnd =
            item.at("window_end");

        std::string priority =
            item.at("priority");

        if (priority == "urgent") {
            request.priority = Priority::Urgent;
        } else {
            request.priority = Priority::Normal;
        }

        std::string skill =
            item.at("required_skill");

        if (skill == "LocalWorks") {
            request.requiredSkill = Skill::LocalWorks;
        }
        else if (skill == "ConnectionWorks") {
            request.requiredSkill = Skill::ConnectionWorks;
        }
        else if (skill == "EmergencyWorks") {
            request.requiredSkill = Skill::EmergencyWorks;
        }
        else {
            throw std::runtime_error(
                "Unknown skill: " + skill
            );
        }

        if (item.contains("required_transport")
            && !item.at("required_transport").is_null()) {

            std::string transport =
                item.at("required_transport");

            if (transport == "Car") {
                request.requiredTransport =
                    Transport::Car;
            }
            else if (transport == "Pedestrian") {
                request.requiredTransport =
                    Transport::Pedestrian;
            }
            else if (transport == "Bicycle") {
                request.requiredTransport =
                    Transport::Bicycle;
            }
            else if (transport == "PublicTransport") {
                request.requiredTransport =
                    Transport::PublicTransport;
            }
            else {
                throw std::runtime_error(
                    "Unknown transport: " + transport
                );
            }
        }

        requests.push_back(request);
    }

    return requests;
}


std::vector<Engineer> JsonLoader::loadEngineers(
    const std::string& path
) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open engineers file: " + path
        );
    }

    json data;
    file >> data;

    std::vector<Engineer> engineers;

    for (const auto& item : data) {
        Engineer engineer;

        engineer.id = item.at("id");

        engineer.name =
            item.at("name");

        engineer.startLocation.lat =
            item.at("lat");

        engineer.startLocation.lon =
            item.at("lon");

        engineer.shiftStart =
            item.at("shift_start");

        engineer.shiftEnd =
            item.at("shift_end");

        std::string transport =
            item.at("transport");

        if (transport == "Car") {
            engineer.transport = Transport::Car;
        }
        else if (transport == "Pedestrian") {
            engineer.transport = Transport::Pedestrian;
        }
        else if (transport == "Bicycle") {
            engineer.transport = Transport::Bicycle;
        }
        else if (transport == "PublicTransport") {
            engineer.transport = Transport::PublicTransport;
        }
        else {
            throw std::runtime_error(
                "Unknown transport: " + transport
            );
        }

        for (const auto& skill :
             item.at("skills")) {

            std::string skillName = skill;

            if (skillName == "LocalWorks") {
                engineer.skills.push_back(
                    Skill::LocalWorks
                );
            }
            else if (skillName == "ConnectionWorks") {
                engineer.skills.push_back(
                    Skill::ConnectionWorks
                );
            }
            else if (skillName == "EmergencyWorks") {
                engineer.skills.push_back(
                    Skill::EmergencyWorks
                );
            }
            else {
                throw std::runtime_error(
                    "Unknown skill: " + skillName
                );
            }
        }

        engineers.push_back(engineer);
    }

    return engineers;
}