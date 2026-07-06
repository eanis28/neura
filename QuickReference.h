/**
 * @file QuickReference.h
 * @brief Quick reference guide UI for available commands and gestures
 * @author Erina
 */
#pragma once

/**
@brief Opens the Neura Quick Reference window.

Launches a separate, non-blocking window that displays a structured
overview of all assistant capabilities, including:

- Voice commands (e.g., time, screenshot, volume, brightness)
- System controls (tabs, windows, recording, reading text)
- Gesture interactions (cursor movement, drag, cancel, yes/no)
- Advanced features (weather, media playback, calendar events, search)
- General usage tips and shortcuts

This window runs independently of the main UI and does not interrupt
or close the primary assistant interface.

@return void

@author Erina
*/
void runQuickReference();