/**
 * @file UI.h
 * @brief Main UI interface declaration and rendering loop
 * @author Erina
 */
#pragma once

#include <string>
#include <mutex>
#include <atomic>

/**
@brief Thread-safe container for passing responses between threads.

This structure is used to safely share text responses (such as voice command outputs)
between worker threads (e.g., voice listener) and the main UI thread.

It uses a mutex to ensure synchronized access and a flag to indicate when
new data is available.

@author Erina
*/
struct SharedResponse {
    std::mutex mtx;
    std::string text;
    bool updated = false;
};


/**
@brief Global shared response object.

Used across multiple modules to communicate responses safely.
*/
extern SharedResponse sharedResponse;

/**
@brief Indicates whether Neura is currently speaking.

This flag is used to synchronize UI visuals and audio state.
*/
extern std::atomic<bool> g_neuraSpeak;    // true while Neura is speaking

/**
@brief Represents the current gesture state detected by the system.
Values: 0 = None, 1 = Moving, 2 = Selecting, 3 = Cancel, 4 = Yes, 5 = No */
extern std::atomic<int>  g_gestureState;  // 0=none, 1=Moving, 2=Selecting, etc.

/**
@brief Flag used to cancel ongoing speech.
When set to true, text-to-speech output will be interrupted.
*/
extern std::atomic<bool> g_cancelSpeech;  // set to true to stop TTS mid-speech

/**
@brief Starts the tutorial overlay sequence.
Initializes and displays the step-by-step tutorial UI.
The tutorial walks the user through gestures and controls.
@return void
@author Erina
*/
void startTutorial();

/**
@brief Cancels any ongoing speech output.
Stops text-to-speech playback and terminates any associated
system-level audio processes across supported platforms.
@return void
@author Jasnav
*/
void cancelSpeech();   // stops TTS and kills search on all platforms

/**
@brief Runs the main UI loop.
Initializes all UI components including window rendering,
audio input, camera feed, gesture handling, and voice commands.
This function blocks until the UI window is closed.
@return bool Returns true if the user clicked the "back" button, otherwise false.
@author Erina
*/
bool runUI();


/**
@brief Opens the Neura Quick Reference window.

Creates and displays a separate UI window that presents a structured,
aesthetically formatted overview of all assistant capabilities.

This includes:
- Voice commands (e.g., screenshot, timer, volume, brightness)
- System controls (tab switching, window switching, recording, time)
- Gesture controls (cursor movement, drag, cancel, yes/no)
- Advanced features (weather, media playback, calendar events, search)
- General usage tips

The window runs independently of the main UI and does not block or
interrupt the primary application flow.

@return void

@author Erina
*/
void runQuickReference();