#include "io/JsonLoader.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

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


Skill parseSkill(const std::string& value)
{
    if (value == "Работы на подключение и дозаказы")
        return Skill::ConnectionWorks;

    if (value == "Локальные работы")
        return Skill::LocalWorks;

    if (value == "Аварийные работы")
        return Skill::EmergencyWorks;

    throw std::runtime_error(
        "Unknown skill: " + value
    );
}


Transport parseTransport(const std::string& value)
{
    if (value == "Автомобиль")
        return Transport::Car;

    if (value == "Велосипед")
        return Transport::Bicycle;

    if (value == "Общественный транспорт")
        return Transport::PublicTransport;

    if (value == "Пешеход")
        return Transport::Pedestrian;

    throw std::runtime_error(
        "Unknown transport: " + value
    );
}


// ------------------------------------------------------------
// Загрузка координат заявок
// ------------------------------------------------------------

std::unordered_map<std::string, Point>
loadRequestLocations(
    const std::string& path
) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open locations file: " + path
        );
    }

    json data;
    file >> data;

    std::unordered_map<std::string, Point>
        locations;

    for (const auto& item : data) {

        const std::string requestId =
            item.at("request_id")
                .get<std::string>();

        const double latitude =
            item.at("latitude")
                .get<double>();

        const double longitude =
            item.at("longitude")
                .get<double>();

        locations[requestId] =
            Point{
                latitude,
                longitude
            };
    }

    return locations;
}

std::unordered_map<std::string, Point>
loadBrigadeLocations(
    const std::string& path
) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open brigade locations file: " + path
        );
    }

    json data;
    file >> data;

    std::unordered_map<std::string, Point>
        locations;

    for (const auto& item : data) {

        const std::string address =
            item.at("address")
                .get<std::string>();

        const double latitude =
            item.at("latitude")
                .get<double>();

        const double longitude =
            item.at("longitude")
                .get<double>();

        locations[address] =
            Point{
                latitude,
                longitude
            };
    }

    return locations;
}

} // namespace


std::vector<Request>
JsonLoader::loadRequests(
    const std::string& path
) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open requests file: " + path
        );
    }

    // --------------------------------------------------------
    // Загружаем координаты отдельно
    // --------------------------------------------------------

    const auto locations =
        loadRequestLocations(
            "data/locations.json"
        );

    std::cout
        << "Loaded location mappings: "
        << locations.size()
        << "\n";

    json data;
    file >> data;

    std::vector<Request> requests;

    for (const auto& item : data) {

        Request request;

        // ----------------------------------------------------
        // ID
        // ----------------------------------------------------

        request.id =
            item.at("request_id")
                .get<std::string>();


        // ----------------------------------------------------
        // География
        // ----------------------------------------------------

        auto locationIt =
            locations.find(request.id);

        if (locationIt != locations.end()) {

            request.location =
                locationIt->second;

            request.hasLocation = true;

        } else {

            request.location =
                Point{0.0, 0.0};

            request.hasLocation = false;
        }

        // ----------------------------------------------------
        // Длительность
        // ----------------------------------------------------

        request.durationMinutes = 60;


        // ----------------------------------------------------
        // Временное окно
        // ----------------------------------------------------

        request.windowStart =
            parseTime(
                item.at("window_start")
                    .get<std::string>()
            );

        request.windowEnd =
            parseTime(
                item.at("window_end")
                    .get<std::string>()
            );


        // ----------------------------------------------------
        // Priority
        // ----------------------------------------------------

        request.priority =
            Priority::Normal;


        // ----------------------------------------------------
        // Skill
        // ----------------------------------------------------

        request.requiredSkill =
            parseSkill(
                item.at("required_skill")
                    .get<std::string>()
            );


        // ----------------------------------------------------
        // Transport
        // ----------------------------------------------------

        if (
            item.contains("required_transport") &&
            !item.at("required_transport").is_null()
        ) {

            request.requiredTransport =
                parseTransport(
                    item.at("required_transport")
                        .get<std::string>()
                );
        }


        // ----------------------------------------------------
        // Дополнительные данные
        // ----------------------------------------------------

        request.region =
            item.value("region", "");

        request.district =
            item.value("district", "");

        request.address =
            item.value("address", "");

        request.requiredSpecialization =
            item.value(
                "required_specialization",
                ""
            );


        requests.push_back(
            request
        );
    }

    int withLocation = 0;
    int withoutLocation = 0;

    for (const auto& request : requests) {
        if (request.hasLocation) {
            ++withLocation;
        } else {
            ++withoutLocation;
        }
    }

    std::cout
        << "Requests with coordinates: "
        << withLocation
        << "\n";

    std::cout
        << "Requests without coordinates: "
        << withoutLocation
        << "\n";

    return requests;
}

std::vector<Engineer>
JsonLoader::loadEngineers(
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

    const auto brigadeLocations =
        loadBrigadeLocations(
            "data/brigade_locations.json"
        );

    std::cout
        << "Loaded brigade location mappings: "
        << brigadeLocations.size()
        << "\n";

    std::vector<Engineer> engineers;

    for (const auto& item : data) {

        Engineer engineer;

        engineer.id =
            item.at("brigade_id")
                .get<std::string>();

        engineer.name =
            item.at("brigade_name")
                .get<std::string>();

        const std::string startAddress =
            item.value("start_address", "");

        auto brigadeLocationIt =
            brigadeLocations.find(startAddress);

        if (brigadeLocationIt != brigadeLocations.end()) {

            engineer.startLocation =
                brigadeLocationIt->second;

        } else {

            engineer.startLocation =
                Point{0.0, 0.0};

            std::cerr
                << "Warning: no coordinates for engineer start address: "
                << startAddress
                << "\n";
        }

        engineer.shiftStart =
            8 * 60;

        engineer.shiftEnd =
            20 * 60;

        engineer.transport =
            parseTransport(
                item.at("transport")
                    .get<std::string>()
            );

        for (const auto& skill :
             item.at("skills")) {

            engineer.skills.push_back(
                parseSkill(
                    skill.get<std::string>()
                )
            );
        }

        if (item.contains("specializations")) {

            for (const auto& specialization :
                 item.at("specializations")) {

                engineer.specializations.push_back(
                    specialization.get<std::string>()
                );
            }
        }

        engineer.region =
            item.value("region", "");

        engineer.homeDistrict =
            item.value("home_district", "");

        if (item.contains("served_districts")) {

            for (const auto& district :
                 item.at("served_districts")) {

                engineer.servedDistricts.push_back(
                    district.get<std::string>()
                );
            }
        }


        engineers.push_back(
            engineer
        );

        std::cout
            << "Engineer "
            << engineer.id
            << " start: "
            << engineer.startLocation.lat
            << ", "
            << engineer.startLocation.lon
            << "\n";
        }

    return engineers;
}