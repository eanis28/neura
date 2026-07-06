#pragma once

/**
 * @file APIAction.h
 * @brief Adds an event to Google Calendar (with optional recurrence).
 *
 * @details Auth: OAuth 2.0 with refresh token stored in config.ini.
 * The assistant automatically exchanges the refresh token for a fresh
 * access token on every run — no manual steps after initial setup (unless refresh token expires).
 * @author Meridith Shang
 */

#include "action.h"
#include <string>
#include <optional>

/**
 * @author Meridith Shang
 * @brief Recurrence frequency options for a calendar event.
 *
 * Controls whether and how often an event repeats. When set to anything
 * other than NONE, a corresponding RRULE is appended to the API request.
 */
enum class Recurrence {
    NONE,       ///< One-time event — no RRULE added.
    DAILY,      ///< Repeats every calendar day.
    WEEKDAYS,   ///< Repeats Monday–Friday only.
    WEEKLY,     ///< Repeats on the same day every week.
    BIWEEKLY,   ///< Repeats on the same day every two weeks.
    MONTHLY,    ///< Repeats on the same date every month.
    YEARLY      ///< Repeats on the same date every year.
};

/**
 * @author Meridith Shang
 * @brief Plain data structure describing a Google Calendar event.
 *
 * All date/time strings must be ISO 8601 combined date-time format
 * (YYYY-MM-DDTHH:MM:SS). The timeZone field must be a valid
 * IANA time zone identifier (e.g. "America/Toronto").
 */
struct CalendarEvent {
    std::string title;             ///< Display title / summary of the event.
    std::string startDateTimeISO;  ///< Event start time, e.g. @c "2026-03-10T14:00:00".
    std::string endDateTimeISO;    ///< Event end time,   e.g. @c "2026-03-10T14:30:00".
    std::string timeZone;          ///< IANA time zone,   e.g. @c "America/Toronto".

    std::optional<std::string> location; ///< Optional physical or virtual location string.
    std::optional<std::string> notes;    ///< Optional free-text description / notes.

    Recurrence recurrence = Recurrence::NONE; ///< Repeat frequency; defaults to a one-time event.
};

/**
 * @author Meridith Shang
 * @brief Action that creates a Google Calendar event via the Calendar REST API.
 *
 * @details Inherits from Action and implements execute() to POST the event
 * to Google Calendar. OAuth 2.0 authentication is handled transparently:
 * the stored refresh token is exchanged for a short-lived access token on
 * every call to execute().
 *
 */
class APIAction : public Action {
public:
    /**
     * @author Meridith Shang
     * @brief Constructs an APIAction with a configuration and an event to create.
     *
     * @param cfg   Application configuration (must contain googleRefreshToken
     *              and related OAuth credentials).
     * @param event The calendar event to be created on execute().
     */
    APIAction(const Configuration& cfg, CalendarEvent event);

    /**
     * @author Meridith Shang
     * @brief Authenticates, validates, and POSTs the event to Google Calendar.
     *
     * @details Internally calls refreshAccessToken(), validateEvent(),
     * buildEventJson(), and buildRRule(). On success, stores the
     * newly created event's ID (retrievable via getCreatedEventId()).
     *
     * @return true if the event was created successfully; false otherwise.
     */
    bool execute() override;

    /**
     * @author Meridith Shang
     * @brief Returns a human-readable summary of the action.
     *
     * @return A string describing the event title, time, and recurrence.
     */
    std::string describe() const override;

    /**
     * @author Meridith Shang
     * @brief Returns the Google Calendar event ID assigned after a successful execute().
     *
     * @return The created event ID string, or an empty string if execute()
     *         has not been called or failed.
     */
    std::string getCreatedEventId() const { return createdEventId; }

private:
    CalendarEvent event;         ///< The event payload to be sent to the API.
    std::string   createdEventId; ///< Populated with the server-assigned ID after creation.

    /**
     * @author Meridith Shang
     * @brief Exchanges the stored OAuth refresh token for a fresh access token.
     *
     * @return A valid Bearer access token string.
     * @throws std::runtime_error if the token exchange request fails.
     */
    std::string refreshAccessToken() const;

    /**
     * @author Meridith Shang
     * @brief Converts a Recurrence value to a Google Calendar RRULE string.
     *
     * @param r The recurrence frequency to convert.
     * @return The corresponding RRULE string (e.g. "RRULE:FREQ=WEEKLY"),
     *         or an empty string when r is Recurrence::NONE.
     */
    static std::string buildRRule(Recurrence r);

    /**
     * @author Meridith Shang
     * @brief Serialises the stored CalendarEvent into Google Calendar API JSON.
     *
     * @details Includes start/end times, time zone, optional location and notes,
     * and the RRULE produced by buildRRule() when recurrence is set.
     *
     * @return A JSON string ready to be used as the HTTP request body.
     */
    std::string buildEventJson() const;

    /**
     * @author Meridith Shang
     * @brief Validates that all required CalendarEvent fields are populated.
     *
     * @details Checks that title, startDateTimeISO, endDateTimeISO,
     * and timeZone are non-empty, and that the end time is after the start time.
     *
     * @return true if the event passes all validation checks; false otherwise.
     */
    bool validateEvent() const;
};