/**
 * @file TimeAction.h
 * @brief Declaration of the TimeAction class for retrieving the current time.
 *
 * This file defines the TimeAction class, which provides functionality
 * for retrieving the current local time and returning it as a formatted string.
 * It interfaces with weather, browser, and system action modules, as well
 * as the project configuration.
 *
 * @author Eliza Anis
 */

#pragma once
#include <string>
#include "WeatherAction.h"
#include "BrowserAction.h"
#include "SystemAction.h"
#include "config.h"

/**
 * @brief Handles retrieval of the current time.
 *
 * The TimeAction class provides a simple interface for obtaining the current
 * local time as a human-readable string. It is intended to be used as part
 * of a larger action-based system where different modules handle distinct
 * assistant capabilities.
 *
 * @author Eliza Anis
 */
class TimeAction {
public:

    /**
     * @brief Returns the current local time as a formatted string.
     *
     * Retrieves the system's current local time and formats it into a
     * human-readable string (e.g., "3:45 PM"). 
     *
     * @return A std::string representing the current local time.
     */
    std::string getCurrentTimeString() const;
};