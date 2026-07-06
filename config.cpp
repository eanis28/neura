/**
 * config.cpp
 *
 * Hand-rolled config.ini parser. Supported format:
 */

#include "config.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>

// ── Helpers ───────────────────────────────────────────────────────

// Strips leading and trailing whitespace from a string
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Accepts "true", "1", or "yes" (case-insensitive) as true; everything else is false
static bool parseBool(const std::string& v) {
    std::string low = v;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    return (low == "true" || low == "1" || low == "yes");
}

// Parses an .ini file into a nested map: section → key → value.
// Blank lines and lines starting with # or ; are skipped.
static std::map<std::string, std::map<std::string, std::string>>
parseIni(const std::string& path) {
    std::map<std::string, std::map<std::string, std::string>> result;
    std::ifstream file(path);
    if (!file.is_open()) return result;

    std::string line, section;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue; // skip malformed lines with no '='

        result[section][trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return result;
}

// ── Permissions ───────────────────────────────────────────────────

// Returns a name → bool* map used by hasPermission/requestPermission to
// look up and mutate permission flags without a chain of if/else branches
std::map<std::string, bool*> Permissions::fieldMap() {
    return {
        {"mediaControl",   &allowMediaControl},
        {"weatherLookup",  &allowWeatherLookup},
        {"calendarWrite",  &allowCalendarWrite},
        {"locationAccess", &allowLocationAccess},
    };
}

bool Permissions::hasPermission(const std::string& feature) const {
    auto map = const_cast<Permissions*>(this)->fieldMap();
    auto it  = map.find(feature);
    return it != map.end() && *(it->second);
}

// Prompts the user to grant a permission at runtime if it isn't already set
bool Permissions::requestPermission(const std::string& feature) {
    if (hasPermission(feature)) return true;

    std::cout << "[Permission] Allow \"" << feature << "\"? (y/n): " << std::flush;
    char answer;
    std::cin >> answer;
    bool granted = (answer == 'y' || answer == 'Y');

    auto map = fieldMap();
    if (map.count(feature)) *(map[feature]) = granted;
    return granted;
}

// ── Configuration ─────────────────────────────────────────────────

Configuration::Configuration(const std::string& path)
    : configFilePath(path) {}

// Reads config.ini and populates all settings; returns false if the file can't be opened
bool Configuration::load() {
    auto ini = parseIni(configFilePath);
    if (ini.empty()) {
        std::cerr << "[Config] Could not open: " << configFilePath << "\n";
        return false;
    }

    // Each key is only written if present — missing keys keep their default values
    auto& api = ini["APISettings"];
    if (api.count("weatherApiKey"))      apiSettings.weatherApiKey      = api["weatherApiKey"];
    if (api.count("youtubeApiKey"))      apiSettings.youtubeApiKey      = api["youtubeApiKey"];
    if (api.count("googleClientId"))     apiSettings.googleClientId     = api["googleClientId"];
    if (api.count("googleClientSecret")) apiSettings.googleClientSecret = api["googleClientSecret"];
    if (api.count("googleRefreshToken")) apiSettings.googleRefreshToken = api["googleRefreshToken"];
    if (api.count("googleCalendarId"))   apiSettings.googleCalendarId   = api["googleCalendarId"];
    if (api.count("weatherBaseUrl"))     apiSettings.weatherBaseUrl     = api["weatherBaseUrl"];
    if (api.count("googleCalendarUrl"))  apiSettings.googleCalendarUrl  = api["googleCalendarUrl"];
    if (api.count("googleTokenUrl"))     apiSettings.googleTokenUrl     = api["googleTokenUrl"];

    auto& perm = ini["Permissions"];
    if (perm.count("allowMediaControl"))   permissions.allowMediaControl   = parseBool(perm["allowMediaControl"]);
    if (perm.count("allowWeatherLookup"))  permissions.allowWeatherLookup  = parseBool(perm["allowWeatherLookup"]);
    if (perm.count("allowCalendarWrite"))  permissions.allowCalendarWrite  = parseBool(perm["allowCalendarWrite"]);
    if (perm.count("allowLocationAccess")) permissions.allowLocationAccess = parseBool(perm["allowLocationAccess"]);

    auto& gen = ini["General"];
    if (gen.count("defaultLocation")) defaultLocation = gen["defaultLocation"];

    return true;
}

// Writes the current configuration back to configFilePath, overwriting any existing file
bool Configuration::save() const {
    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        std::cerr << "[Config] Could not write: " << configFilePath << "\n";
        return false;
    }

    file << "[APISettings]\n"
         << "weatherApiKey       = " << apiSettings.weatherApiKey      << "\n"
         << "youtubeApiKey       = " << apiSettings.youtubeApiKey      << "\n"
         << "googleClientId      = " << apiSettings.googleClientId     << "\n"
         << "googleClientSecret  = " << apiSettings.googleClientSecret << "\n"
         << "googleRefreshToken  = " << apiSettings.googleRefreshToken << "\n"
         << "googleCalendarId    = " << apiSettings.googleCalendarId   << "\n"
         << "weatherBaseUrl      = " << apiSettings.weatherBaseUrl     << "\n"
         << "googleCalendarUrl   = " << apiSettings.googleCalendarUrl  << "\n"
         << "googleTokenUrl      = " << apiSettings.googleTokenUrl     << "\n\n"
         << "[Permissions]\n"
         << "allowMediaControl   = " << (permissions.allowMediaControl   ? "true" : "false") << "\n"
         << "allowWeatherLookup  = " << (permissions.allowWeatherLookup  ? "true" : "false") << "\n"
         << "allowCalendarWrite  = " << (permissions.allowCalendarWrite  ? "true" : "false") << "\n"
         << "allowLocationAccess = " << (permissions.allowLocationAccess ? "true" : "false") << "\n\n"
         << "[General]\n"
         << "defaultLocation     = " << defaultLocation << "\n";

    return true;
}

// Prints a human-readable summary; pass showSecrets=true to include keys and tokens
void Configuration::print(bool showSecrets) const {
    std::cout << "=== Configuration ===\n"
              << "Config file    : " << configFilePath               << "\n"
              << "Default loc    : " << defaultLocation              << "\n"
              << "Calendar ID    : " << apiSettings.googleCalendarId << "\n";
    if (showSecrets) {
        std::cout << "Weather key    : " << apiSettings.weatherApiKey      << "\n"
                  << "Google ID      : " << apiSettings.googleClientId     << "\n"
                  << "Refresh token  : " << apiSettings.googleRefreshToken << "\n";
    }
    std::cout << "Permissions    : "
              << "media="    << permissions.allowMediaControl
              << " weather=" << permissions.allowWeatherLookup
              << " calendar="<< permissions.allowCalendarWrite
              << " location="<< permissions.allowLocationAccess << "\n"
              << "=====================\n";
}