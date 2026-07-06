#pragma once

/**
 * @file Landing.h
 * @brief Landing page interface for the Neura application
 * @author Kethy
 */

/**
 * @brief Displays the application landing page with navigation options
 * 
 * Creates and runs a modal window showing the Neura landing screen with
 * buttons for Start, Tutorial, and Settings. The window is centered on
 * screen at half the desktop resolution. User can navigate to different
 * parts of the application or close to exit.
 * 
 * @return true if user clicked Start or Tutorial (proceed to main app),
 *         false if user closed the window or pressed Escape (exit app)
 */
bool runLandingPage();