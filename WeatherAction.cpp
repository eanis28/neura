#include "WeatherAction.h"
#include "json.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

using json = nlohmann::json;

WeatherAction::WeatherAction(const Configuration& cfg, const std::string& loc)
    : Action(cfg), targetLocation(loc) {}

std::string WeatherAction::describe() const {
    std::string loc = targetLocation.empty() ? "current location" : targetLocation;
    return "Weather: Fetch conditions for \"" + loc + "\"";
}

bool WeatherAction::execute() {
    if (!config.permissions.allowWeatherLookup) {
        log("WeatherLookup permission denied.");
        return false;
    }
    if (config.apiSettings.weatherApiKey.empty()) {
        log("WeatherAction: weatherApiKey is not configured.");
        return false;
    }

    // Use the explicit location if provided, otherwise try to resolve one automatically
    std::string loc = targetLocation.empty() ? resolveLocation() : targetLocation;
    if (loc.empty()) {
        log("WeatherAction: could not determine a location.");
        return false;
    }

    std::string url = config.apiSettings.weatherBaseUrl
                    + "/weather?q=" + urlEncode(loc)
                    + "&appid=" + config.apiSettings.weatherApiKey
                    + "&units=metric"; // API returns Celsius; we convert to F in parseResponse

    log("GET " + url);
    std::string body = httpGet(url);
    if (body.empty()) {
        log("WeatherAction: empty response from API.");
        return false;
    }

    try {
        lastResult = parseResponse(body, loc);
    } catch (const std::exception& e) {
        log("WeatherAction: JSON parse error — " + std::string(e.what()));
        return false;
    }

    log("WeatherAction: success — " + getSummary());
    return true;
}

// Returns a single-line human-readable summary, or "" if no result is cached yet
std::string WeatherAction::getSummary() const {
    if (!lastResult) return "";
    const WeatherData& d = *lastResult;
    std::ostringstream ss;
    ss << d.location << ": " << d.condition
       << ", " << std::fixed << std::setprecision(1)
       << d.temperatureCelsius << "C"
       << " (" << d.temperatureFahrenheit << "F)"
       << ", humidity " << d.humidity << "%"
       << ", wind " << d.windSpeedKph << " km/h";
    return ss.str();
}

// Resolves a location in priority order: config default → IP geolocation fallback
std::string WeatherAction::resolveLocation() const {
    if (!config.defaultLocation.empty()) return config.defaultLocation;

    // ip-api.com requires no API key and is used purely as a best-effort fallback
    std::string geoBody = httpGet("http://ip-api.com/json");
    if (!geoBody.empty()) {
        try {
            auto j = json::parse(geoBody);
            if (j.contains("city")) return j["city"].get<std::string>();
        } catch (...) {}
    }
    return "";
}

// Parses an OpenWeatherMap /weather JSON response into a WeatherData struct.
// Wind speed is converted from m/s to km/h; Fahrenheit is derived from Celsius.
WeatherData WeatherAction::parseResponse(const std::string& jsonBody,
                                          const std::string& location) const {
    auto j = json::parse(jsonBody);

    WeatherData d;
    d.location              = location;
    d.temperatureCelsius    = j["main"]["temp"].get<double>();
    d.temperatureFahrenheit = d.temperatureCelsius * 9.0 / 5.0 + 32.0;
    d.humidity              = j["main"]["humidity"].get<int>();
    d.windSpeedKph          = j["wind"]["speed"].get<double>() * 3.6; // m/s → km/h

    // "weather" is an array; only the first element is used
    if (j.contains("weather") && j["weather"].is_array() && !j["weather"].empty()) {
        d.condition = j["weather"][0]["description"].get<std::string>();
        d.iconCode  = j["weather"][0]["icon"].get<std::string>();
    }

    // Record fetch time in UTC so callers can check data freshness
    std::time_t now = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    d.fetchedAt = ts;

    return d;
}