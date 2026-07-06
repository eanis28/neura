#pragma once
#include <string>
#include <vector>
#include "UIHelpers.h"
#include "UIWidgets.h"

/**
 * @file Settings.h
 * @brief User settings management and settings page interface
 * @author Kethy
 */

/**
 * @struct ColourPreset
 * @brief Represents an RGB color preset with a display name
 * 
 * Used to populate the color picker in the settings page. Each preset
 * defines an accent color that can be applied throughout the UI.
 */
struct ColourPreset { 
    const char* name; ///< Display name for this color
    int r;            ///< Red component (0-255)
    int g;            ///< Green component (0-255)
    int b;            ///< Blue component (0-255)
};

extern const ColourPreset UI_COLOUR_PRESETS[]; ///< All available accent colors
extern const int          UI_COLOUR_COUNT;     ///< Number of color presets
extern const char*        ACCENT_NAMES[];      ///< Display names for each TTS accent
extern const int          ACCENT_COUNT;        ///< Number of available accents

/**
 * @struct AppSettings
 * @brief Stores all user-configurable application settings
 * 
 * Settings are persisted to and loaded from a plain-text .ini file using
 * simple key=value format. The global instance g_settings is loaded once
 * at startup and written back when the user saves changes in the settings UI.
 */
struct AppSettings {
    int volume      = 75;  ///< System audio volume (0-100)
    int accentIndex = 0;   ///< Index into ACCENT_NAMES for TTS voice accent
    int colorIndex  = 0;   ///< Index into UI_COLOUR_PRESETS for UI accent color
    int cameraIndex = 0;   ///< Camera device index for OpenCV (-1 = auto-detect)
    int micIndex    = -1;  ///< Microphone device index (-1 = system default)

    /**
     * @brief Loads settings from an .ini file
     * 
     * Reads key=value pairs from the specified file. If the file doesn't exist
     * or a field is missing, default values are retained. Invalid values are
     * silently ignored.
     * 
     * @param path Path to the settings file (default: "settings.ini")
     */
    void load(const std::string& path = "settings.ini");

    /**
     * @brief Saves current settings to an .ini file
     * 
     * Writes all settings as key=value pairs to the specified file, overwriting
     * any existing content.
     * 
     * @param path Path to the settings file (default: "settings.ini")
     */
    void save(const std::string& path = "settings.ini") const;
};

extern AppSettings g_settings; ///< Global settings instance shared across the application

/**
 * @brief Displays the settings page and handles user interaction
 * 
 * Opens a modal window showing all configurable settings including volume,
 * voice accent, UI color, microphone selection, and Google OAuth tokens.
 * The window blocks until the user closes it. Changes are only persisted
 * if the user clicks Save or Save & Exit.
 * 
 * @return true if the user saved changes, false if they cancelled or closed
 */
bool runSettingsPage();