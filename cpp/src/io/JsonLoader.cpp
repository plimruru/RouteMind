#include "io/JsonLoader.h"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

int parseTime(const std::string& value)
    {
        // Формат:
        // 2026-08-17T18:00:00

        int hour =
            std::stoi(value.substr(11, 2));

        int minute =
            std::stoi(value.substr(14, 2));

        return hour * 60 + minute;
    }

}

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

        request.id = item.at("request_id").get<std::string>();

        request.windowStart =
            parseTime(item.at("window_start").get<std::string>());

        request.windowEnd =
            parseTime(item.at("window_end").get<std::string>());

        std::string skill =
            item.at("required_skill");

        if (skill == "Локальные работы") {
            request.requiredSkill = Skill::LocalWorks;
        }
        else if (skill == "Работы на подключение и дозаказы") {
            request.requiredSkill = Skill::ConnectionWorks;
        }
        else if (skill == "Аварийные работы") {
            request.requiredSkill = Skill::EmergencyWorks;
        }
        else {
            throw std::runtime_error(
                "Unknown skill: " + skill
            );
        }

        request.region =
            item.at("region").get<std::string>();

        request.district =
            item.at("district").get<std::string>();

        request.address =
            item.at("address").get<std::string>();

        request.requiredSpecialization =
            item.at("required_specialization").get<std::string>();

        if (item.contains("required_transport")
            && !item.at("required_transport").is_null()) {

            std::string transport =
                item.at("required_transport");

            if (transport == "Автомобиль") {
                request.requiredTransport =
                    Transport::Car;
            }
            else if (transport == "Пешеход") {
                request.requiredTransport =
                    Transport::Pedestrian;
            }
            else if (transport == "Велосипед") {
                request.requiredTransport =
                    Transport::Bicycle;
            }
            else if (transport == "Общественный транспорт") {
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

        engineer.id =
            item.at("brigade_id").get<std::string>();

        engineer.name =
            item.at("brigade_name").get<std::string>();

        std::string transport =
            item.at("transport");

        if (transport == "Автомобиль") {
            engineer.transport = Transport::Car;
        }
        else if (transport == "Пешеход") {
            engineer.transport = Transport::Pedestrian;
        }
        else if (transport == "Велосипед") {
            engineer.transport = Transport::Bicycle;
        }
        else if (transport == "Общественный транспорт") {
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

            if (skillName == "Локальные работы") {
                engineer.skills.push_back(
                    Skill::LocalWorks
                );
            }
            else if (skillName == "Работы на подключение и дозаказы") {
                engineer.skills.push_back(
                    Skill::ConnectionWorks
                );
            }
            else if (skillName == "Аварийные работы") {
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

        engineer.region =
            item.at("region").get<std::string>();

        engineer.homeDistrict =
            item.at("home_district").get<std::string>();

        for (const auto& district :
            item.at("served_districts")) {

            engineer.servedDistricts.push_back(
                district.get<std::string>()
            );
        }
        
        for (const auto& specialization :
            item.at("specializations")) {

            engineer.specializations.push_back(
                specialization.get<std::string>()
            );
        }

        engineer.shiftStart = 8 * 60;
        engineer.shiftEnd = 20 * 60;

        engineers.push_back(engineer);
    }

    return engineers;
}