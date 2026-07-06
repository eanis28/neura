#include "APIAction.h"
#include "json.hpp"

using json = nlohmann::json;

APIAction::APIAction(const Configuration& cfg, CalendarEvent ev)
    : Action(cfg), event(std::move(ev)) {}

std::string APIAction::describe() const {
    std::string desc = "API: Add calendar event \"" + event.title
                     + "\" on " + event.startDateTimeISO;

    switch (event.recurrence) {
        case Recurrence::DAILY:    desc += " (repeats daily)";          break;
        case Recurrence::WEEKDAYS: desc += " (repeats every weekday)";  break;
        case Recurrence::WEEKLY:   desc += " (repeats weekly)";         break;
        case Recurrence::BIWEEKLY: desc += " (repeats every 2 weeks)";  break;
        case Recurrence::MONTHLY:  desc += " (repeats monthly)";        break;
        case Recurrence::YEARLY:   desc += " (repeats yearly)";         break;
        case Recurrence::NONE:     break;
    }

    return desc;
}

// Maps a Recurrence enum value to its iCalendar RRULE string
std::string APIAction::buildRRule(Recurrence r) {
    switch (r) {
        case Recurrence::DAILY:    return "RRULE:FREQ=DAILY";
        case Recurrence::WEEKDAYS: return "RRULE:FREQ=WEEKLY;BYDAY=MO,TU,WE,TH,FR";
        case Recurrence::WEEKLY:   return "RRULE:FREQ=WEEKLY";
        case Recurrence::BIWEEKLY: return "RRULE:FREQ=WEEKLY;INTERVAL=2";
        case Recurrence::MONTHLY:  return "RRULE:FREQ=MONTHLY";
        case Recurrence::YEARLY:   return "RRULE:FREQ=YEARLY";
        case Recurrence::NONE:     return "";
    }
    return "";
}

// Exchanges the stored refresh token for a fresh access token.
// Called automatically at the start of every execute() — the user
// never needs to do anything after the one-time setup.
std::string APIAction::refreshAccessToken() const {
    if (config.apiSettings.googleClientId.empty() ||
        config.apiSettings.googleClientSecret.empty() ||
        config.apiSettings.googleRefreshToken.empty()) {
        log("APIAction: googleClientId, googleClientSecret, or googleRefreshToken "
            "is missing from config.ini. See GOOGLE_API_SETUP.md.");
        return "";
    }

    std::string body =
        "client_id="      + urlEncode(config.apiSettings.googleClientId)     +
        "&client_secret=" + urlEncode(config.apiSettings.googleClientSecret) +
        "&refresh_token=" + urlEncode(config.apiSettings.googleRefreshToken) +
        "&grant_type=refresh_token";

    log("Refreshing Google access token...");

    // No Bearer token here — credentials are in the POST body for this exchange
    std::string response = httpPost(
        config.apiSettings.googleTokenUrl,
        "",
        body,
        "application/x-www-form-urlencoded"
    );

    if (response.empty()) {
        log("refreshAccessToken: no response from Google token endpoint.");
        return "";
    }

    try {
        auto j = json::parse(response);

        if (j.contains("error")) {
            log("refreshAccessToken: Google error — " +
                j.value("error_description", j.value("error", std::string("unknown"))));
            return "";
        }

        std::string token = j.value("access_token", "");
        if (token.empty())
            log("refreshAccessToken: response had no access_token field.");
        else
            log("refreshAccessToken: token obtained successfully.");

        return token;

    } catch (const std::exception& e) {
        log("refreshAccessToken: JSON parse error — " + std::string(e.what()));
        return "";
    }
}

// Serializes the CalendarEvent to the JSON body expected by the Google Calendar API.
// Optional fields (location, notes, recurrence) are omitted when not set.
std::string APIAction::buildEventJson() const {
    json payload = {
        {"summary", event.title},
        {"start", {
            {"dateTime", event.startDateTimeISO},
            {"timeZone", event.timeZone}
        }},
        {"end", {
            {"dateTime", event.endDateTimeISO},
            {"timeZone", event.timeZone}
        }}
    };

    if (event.location) payload["location"]    = *event.location;
    if (event.notes)    payload["description"] = *event.notes;

    // recurrence is a JSON array of RRULE strings; omit the key entirely for non-recurring events
    std::string rrule = buildRRule(event.recurrence);
    if (!rrule.empty())
        payload["recurrence"] = json::array({rrule});

    return payload.dump();
}

// Returns false and logs the first missing/malformed required field
bool APIAction::validateEvent() const {
    if (event.title.empty()) {
        log("APIAction: event title is required."); return false;
    }
    if (event.startDateTimeISO.empty()) {
        log("APIAction: start date/time is required."); return false;
    }
    if (event.startDateTimeISO.size() < 19) {
        log("APIAction: startDateTimeISO must be YYYY-MM-DDTHH:MM:SS format.");
        return false;
    }
    if (event.endDateTimeISO.empty()) {
        log("APIAction: end date/time is required."); return false;
    }
    return true;
}

// Creates the calendar event via the Google Calendar API.
// Returns true and populates createdEventId on success.
bool APIAction::execute() {
    if (!config.permissions.allowCalendarWrite) {
        log("CalendarWrite permission denied.");
        return false;
    }
    if (!validateEvent()) return false;

    std::string accessToken = refreshAccessToken();
    if (accessToken.empty()) {
        log("APIAction: could not obtain access token. Check config.ini credentials.");
        return false;
    }

    // Target the configured calendar's events collection
    std::string url = config.apiSettings.googleCalendarUrl
                    + "/calendars/"
                    + urlEncode(config.apiSettings.googleCalendarId)
                    + "/events";

    std::string eventJson = buildEventJson();
    log("POST " + url);
    log("Payload: " + eventJson);

    std::string response = httpPost(url, accessToken, eventJson);
    if (response.empty()) {
        log("APIAction: empty response from Google Calendar API.");
        return false;
    }

    log("Raw response: " + response);

    try {
        auto j = json::parse(response);
        if (j.contains("error")) {
            log("APIAction: Google API error " +
                std::to_string(j["error"].value("code", 0)) +
                " — " + j["error"].value("message", std::string("unknown")));
            return false;
        }
        createdEventId = j.value("id", "");
    } catch (const std::exception& e) {
        log("APIAction: could not parse response — " + std::string(e.what()));
        return false;
    }

    // A successful creation always returns an ID; treat its absence as a soft failure
    if (createdEventId.empty()) {
        log("APIAction: response did not contain an event ID.");
        return false;
    }

    log("APIAction: event created. ID = " + createdEventId);
    return true;
}