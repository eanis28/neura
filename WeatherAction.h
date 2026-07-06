#pragma once

/**
 * @file WeatherAction.h
 * @brief Action that fetches current weather conditions via OpenWeatherMap.
 *
 * @details Requires weatherApiKey to be set in config.ini.
 * If no location is provided at construction, the action resolves one
 * automatically: first from config.defaultLocation, then by falling
 * back to IP-based geolocation as a last resort.
 * @author Meridith Shang
 */

#include "action.h"
#include <string>
#include <optional>

/**
 * @brief Holds the weather data returned by a successful WeatherAction::execute() call.
 *
 * @details Populated by WeatherAction::parseResponse() and retrievable
 * via WeatherAction::getResult(). Temperature is provided in both
 * Celsius and Fahrenheit for convenience; all other measurements use
 * SI-adjacent units as noted per field.
 * @author Meridith Shang
 */
struct WeatherData {
    std::string location;                      ///< Resolved city/location name as returned by the API.
    double      temperatureCelsius    = 0.0;   ///< Current temperature in degrees Celsius.
    double      temperatureFahrenheit = 0.0;   ///< Current temperature in degrees Fahrenheit.
    int         humidity              = 0;     ///< Relative humidity as a percentage (0–100).
    double      windSpeedKph          = 0.0;   ///< Wind speed in kilometres per hour.
    std::string condition;                     ///< Human-readable condition string (e.g. "Partly cloudy").
    std::string iconCode;                      ///< OpenWeatherMap icon code (e.g. "02d"); use to build icon URLs.
    std::string fetchedAt;                     ///< ISO 8601 UTC timestamp of when the data was retrieved.
};

/**
 * @author Meridith Shang
 * @brief Fetches and exposes current weather for a given or resolved location.
 *
 * @details Inherits HTTP helpers from Action. On execute(), the action:
 * - Resolves the target location via resolveLocation() if none was
 *    provided at construction.
 * - Builds and issues an OpenWeatherMap Current Weather API request
 *    using httpGet().
 * - Parses the JSON response body into a WeatherData struct via
 *    parseResponse() and stores it as lastResult.
 *
 */
class WeatherAction : public Action {
public:
    /**
     * @author Meridith Shang
     * @brief Constructs a WeatherAction targeting the specified location.
     *
     * @details If location is empty, the target is deferred to
     * resolveLocation() at execute() time, which consults
     * config.defaultLocation and then IP geolocation.
     *
     * @param cfg      Application configuration (must contain weatherApiKey).
     * @param location City name or location string to fetch weather for.
     *                 Pass an empty string to use automatic resolution.
     */
    explicit WeatherAction(const Configuration& cfg,
                           const std::string&   location = "");

    /**
     * @author Meridith Shang
     * @brief Resolves the location, calls the OpenWeatherMap API, and stores the result.
     *
     * @return true if weather data was fetched and parsed successfully;
     *         false if the API key is missing, the request failed, or
     *         the response could not be parsed.
     */
    bool execute() override;

    /**
     * @author Meridith Shang
     * @brief Returns a human-readable description of the action.
     *
     * @return A string of the form "Fetch weather for <location>",
     *         using the resolved location if available, otherwise the
     *         raw constructor argument.
     */
    std::string describe() const override;

    /**
     * @author Meridith Shang
     * @brief Returns the weather data from the last successful execute() call.
     *
     * @return A std::optional<WeatherData> containing the result, or
     *         std::nullopt if execute() has not been called or failed.
     */
    std::optional<WeatherData> getResult() const { return lastResult; }

    /**
     * @author Meridith Shang
     * @brief Returns a formatted single-line weather summary.
     *
     * @details Composes a human-readable string from lastResult, e.g.
     * "Toronto: Partly cloudy, 14°C (57°F), humidity 60%, wind 18 km/h".
     * Returns an empty string if no result is available.
     *
     * @return A formatted weather summary, or an empty string if
     *         execute() has not been called or failed.
     */
    std::string getSummary() const;

private:
    std::string                targetLocation; ///< Location supplied at construction; may be empty.
    std::optional<WeatherData> lastResult;     ///< Populated after a successful execute() call.

    /**
     * @author Meridith Shang
     * @brief Determines the location to query when none was provided at construction.
     *
     * @details Resolution order:
     * - config.defaultLocation, if non-empty.
     * - IP-based geolocation via an external lookup service, as a last resort.
     *
     * @return A non-empty location string, or an empty string if all
     *         resolution strategies fail.
     */
    std::string resolveLocation() const;

    /**
     * @author Meridith Shang
     * @brief Parses an OpenWeatherMap JSON response into a WeatherData struct.
     *
     * @details Extracts temperature (Kelvin → Celsius + Fahrenheit), humidity,
     * wind speed, condition text, icon code, and stamps fetchedAt with the
     * current UTC time.
     *
     * @param jsonBody The raw JSON response body returned by the API.
     * @param location The resolved location string to store in WeatherData::location.
     * @return A fully populated WeatherData struct.
     * @throws std::runtime_error if required JSON fields are missing or malformed.
     */
    WeatherData parseResponse(const std::string& jsonBody,
                              const std::string& location) const;
};