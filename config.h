#pragma once

/**
 * @file config.h
 * @brief Application configuration: API credentials, feature permissions, and runtime settings.
 *
 * @details Google Calendar authentication uses OAuth 2.0 with a long-lived refresh token.
 * The refresh token is obtained once via the OAuth flow and stored permanently in
 * config.ini. On every run, the assistant exchanges it for a fresh short-lived
 * access token automatically — no user interaction is required after initial setup.
 * @author Meridith Shang
 */

#include <string>
#include <map>
#include <iostream>

/**
 * @author Meridith Shang
 * @brief Holds all third-party API credentials and base URLs.
 *
 * @details Fields are populated by Configuration::load() from config.ini.
 * Base URL fields have sensible defaults and should only be changed when
 * routing traffic through a proxy or a mock server during testing.
 */
class APISettings {
public:
    // ── OpenWeatherMap ────────────────────────────────────────────────────────

    std::string weatherApiKey; ///< OpenWeatherMap API key.

    // ── YouTube Data API v3 ───────────────────────────────────────────────────

    std::string youtubeApiKey; ///< YouTube Data API v3 key.

    // ── Google OAuth 2.0 ─────────────────────────────────────────────────────

    /**
     * @brief OAuth 2.0 client ID obtained from the Google Cloud Console.
     *
     * @details Created once per project under
     * APIs & Services → Credentials. Treat as semi-public:
     * it is transmitted during the token exchange but is not sufficient
     * on its own to authenticate.
     */
    std::string googleClientId;

    /**
     * @brief OAuth 2.0 client secret obtained from the Google Cloud Console.
     *
     * @warning Keep this value out of version control. Anyone with the
     * client ID and secret pair can impersonate this application.
     */
    std::string googleClientSecret;

    /**
     * @brief Long-lived OAuth 2.0 refresh token used to obtain access tokens.
     *
     * @details Acquired once via the interactive OAuth consent flow and
     * written to config.ini. It does not expire unless manually revoked
     * at <em>myaccount.google.com/permissions</em>. The assistant exchanges
     * it automatically for a fresh access token on every run.
     *
     * @warning Store securely — this token grants ongoing access to the
     * user's Google account within the approved scopes.
     */
    std::string googleRefreshToken;

    /**
     * @brief Google Calendar ID to write events to.
     *
     * Defaults to "primary", which resolves to the authenticated user's
     * main calendar. Override with a specific calendar ID (e.g.
     * "work\@group.calendar.google.com") to target a secondary calendar.
     */
    std::string googleCalendarId = "primary";

    // ── Base URLs ─────────────────────────────────────────────────────────────

    std::string weatherBaseUrl    = "https://api.openweathermap.org/data/2.5"; ///< OpenWeatherMap REST base URL.
    std::string googleCalendarUrl = "https://www.googleapis.com/calendar/v3";  ///< Google Calendar REST base URL.
    std::string googleTokenUrl    = "https://oauth2.googleapis.com/token";     ///< Google OAuth 2.0 token endpoint.
};

/**
 * @author Meridith Shang
 * @brief Tracks which optional features the user has granted consent for.
 *
 * @details Each flag guards a capability that may have privacy or cost
 * implications. Flags default to false (deny-by-default). Use
 * hasPermission() to query and requestPermission() to prompt the
 * user at runtime.
 */
class Permissions {
public:
    bool allowMediaControl   = false; ///< Permit controlling local media playback.
    bool allowWeatherLookup  = false; ///< Permit outbound calls to the weather API.
    bool allowCalendarWrite  = false; ///< Permit creating/modifying Google Calendar events.
    bool allowLocationAccess = false; ///< Permit reading the device's current location.

    /**
     * @author Meridith Shang
     * @brief Checks whether a named feature has been granted.
     *
     * @param feature Case-sensitive feature name (e.g. "allowCalendarWrite").
     * @return true if the corresponding flag is set; false if the feature
     *         is unknown or has not been granted.
     */
    bool hasPermission(const std::string& feature) const;

    /**
     * @author Meridith Shang
     * @brief Prompts the user to grant a permission and stores the result.
     *
     * @details If the user grants consent, the corresponding flag is set to
     * true and the change is persisted via Configuration::save().
     *
     * @param feature Case-sensitive feature name to request consent for.
     * @return true if the user granted the permission; false if denied
     *         or the feature name is unrecognised.
     */
    bool requestPermission(const std::string& feature);

private:
    /**
     * @author Meridith Shang
     * @brief Returns a name-to-pointer map over all permission flags.
     *
     * @details Used internally by hasPermission() and requestPermission()
     * to look up a flag by its string name without a chain of if/else
     * comparisons. Extend this map when adding new permission fields.
     *
     * @return A std::map whose keys are field name strings and whose values
     *         are pointers to the corresponding bool members.
     */
    std::map<std::string, bool*> fieldMap();
};

/**
 * @author Meridith Shang
 * @brief Top-level configuration object; owns APISettings and Permissions.
 *
 * @details Reads from and writes to an INI-style file (default: config.ini).
 * Construct with the path to the config file, then call load() before
 * accessing any settings.
 *
 */
class Configuration {
public:
    APISettings apiSettings;   ///< All third-party API credentials and base URLs.
    Permissions permissions;   ///< Feature-level permission flags.
    std::string defaultLocation; ///< Default location string used by the weather action.

    /**
     * @author Meridith Shang
     * @brief Constructs a Configuration bound to the given file path.
     *
     * @details Does not read the file — call load() explicitly after
     * construction.
     *
     * @param configFilePath Path to the INI config file.
     *                       Defaults to "config.ini" in the working directory.
     */
    explicit Configuration(const std::string& configFilePath = "config.ini");

    /**
     * @author Meridith Shang
     * @brief Parses the config file and populates all settings fields.
     *
     * @return true if the file was found and parsed without error;
     *         false if the file is missing, unreadable, or malformed.
     */
    bool load();

    /**
     * @author Meridith Shang
     * @brief Writes the current settings back to the config file.
     *
     * @details Overwrites the file at configFilePath. Useful after
     * Permissions::requestPermission() updates a flag that should persist
     * across sessions.
     *
     * @return true if the file was written successfully; false on I/O error.
     */
    bool save() const;

    /**
     * @author Meridith Shang
     * @brief Prints all configuration fields to stdout.
     *
     * @param showSecrets When false (default), sensitive values such as
     *                    googleClientSecret and googleRefreshToken are
     *                    masked with "***". Pass true only in trusted
     *                    debug contexts.
     */
    void print(bool showSecrets = false) const;

private:
    std::string configFilePath; ///< Path to the INI file; set at construction.
};