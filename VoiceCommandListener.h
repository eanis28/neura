/**
 * @file VoiceCommandListener.h
 * @brief Voice command recognition and processing thread manager
 * @author Jasnav
 */
#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <string>

#include "whisper.h"
#include "TimerAction.h"
#include "SystemCommandAction.h"
#include "config.h"
#include "AudioManager.h"
#include "APIAction.h"
#include "UI.h"
#include "APIAction.h"


/**
 * @class VoiceCommandListener
 * @brief Handles speech recognition and voice-command processing for Neura.
 *
 * This class is responsible for continuously listening to microphone input,
 * converting spoken audio into text using Whisper, and dispatching recognized
 * commands to the appropriate actions in the system.
 *
 * It also manages multi-step voice interactions such as creating calendar
 * events by prompting the user for missing details step by step.
 */

class VoiceCommandListener {

public:


/**
     * @brief Constructs a VoiceCommandListener object.
     *
     * Initializes the listener with the Whisper model path, shared audio buffer,
     * microphone mute state, and shared response object used by the UI.
     *
     * @param modelPath Path to the Whisper model file.
     * @param sharedBuf Shared audio buffer containing microphone input samples.
     * @param micMuted Shared atomic flag indicating whether the microphone is muted.
     * @param sharedResponse Shared response object used to display assistant replies.
     */
    VoiceCommandListener(const std::string& modelPath,
                         SharedAudioBuffer& sharedBuf,
                         std::atomic<bool>& micMuted,
                         SharedResponse& sharedResponse);

    /**
     * @brief Destroys the VoiceCommandListener object.
     *
     * Ensures resources such as threads and Whisper context are properly cleaned up.
     */
    ~VoiceCommandListener();

    /**
     * @brief Starts the voice command listener.
     *
     * Begins the background listening loop that captures audio, performs speech
     * recognition, and processes recognized commands.
     *
     * @return true if the listener started successfully, false otherwise.
     */

    bool start();

    /**
     * @brief Stops the voice command listener.
     *
     * Ends the background listening process and stops command recognition.
     */
    void stop();


    /**
     * @enum CalendarPromptStep
     * @brief Represents the current step in a multi-step calendar event prompt.
     *
     * This enum is used to track which piece of information the assistant is
     * currently asking the user for when building a calendar event request.
     */

    enum class CalendarPromptStep {
        NONE, /**< No calendar interaction is currently in progress. */
        ASK_TITLE, /**< The assistant is asking for the event title. */
        ASK_WHEN, /**< The assistant is asking for the date and time. */
        ASK_DURATION, /**< The assistant is asking for the duration. */
        ASK_RECURRENCE /**< The assistant is asking for the recurrence type. */
    };

    /**
     * @struct PendingCalendarRequest
     * @brief Stores information for a calendar event being created through voice prompts.
     *
     * This structure keeps track of the event details collected so far during
     * a multi-step interaction, including title, date, time, duration, and recurrence.
     */
    struct PendingCalendarRequest {
        bool active = false;
        CalendarPromptStep step = CalendarPromptStep::NONE;
        std::string title; /**< Title of the calendar event. */
        std::string date;  /**< Event date in YYYY-MM-DD format. */
        std::string time24h; /**< Event time in HH:MM 24-hour format. */
        int durationMinutes = 30; /**< Duration of the event in minutes. */
        Recurrence recurrence = Recurrence::NONE; /**< Recurrence setting for the event. */
    };

private:


    /**
     * @brief Runs the main background listening loop.
     *
     * Captures microphone input, processes audio with Whisper, and forwards
     * recognized text to the dispatcher.
     *
     * @return true if the loop ran successfully, false otherwise.
     */

    bool runLoop();


    /**
     * @brief Dispatches a recognized voice command.
     *
     * Interprets the given text and triggers the corresponding action,
     * such as timers, system commands, or calendar actions.
     *
     * @param text The recognized voice command text.
     */
    void dispatch(const std::string& text);


    /**
     * @brief Sends a response message back to the user.
     *
     * Updates the shared response object so the UI can display or speak
     * the assistant's reply.
     *
     * @param msg The message to respond with.
     */

    void respond(const std::string& msg);

    /**
     * @brief Finalizes a pending calendar event request.
     *
     * Uses the collected event details to create the calendar event through
     * the appropriate API action.
     *
     * @return true if the calendar event was created successfully, false otherwise.
     */
    bool finishPendingCalendarRequest();

    whisper_context*   m_ctx = nullptr; /**< Whisper context used for speech recognition. */
    SharedAudioBuffer& m_buf; /**< Shared audio buffer containing recorded microphone audio. */
    std::atomic<bool>& m_micMuted; /**< Shared flag indicating whether the microphone is muted. */
    SharedResponse&    m_response; /**< Shared UI response object for assistant replies. */
    std::atomic<bool>  m_running{false}; /**< Indicates whether the listener is currently running. */
    std::thread        m_thread; /**< Background thread running the listening loop. */
    std::atomic<bool> m_assistantPaused{false}; /**< Indicates whether the assistant is paused. */

    TimerAction         m_timer; /**< Handles timer-related voice commands. */
    SystemCommandAction m_sys; /**< Handles OS-level system commands. */
    Configuration       m_config{"config.ini"}; /**< Loads configuration settings from file. */

    static constexpr int WINDOW_SECONDS = 3; /**< Audio window size used for processing. */
    static constexpr int SAMPLE_RATE    = 16000; /**< Audio sample rate expected by Whisper. */

    PendingCalendarRequest m_pendingCalendar; /**< Stores the current in-progress calendar request. */
};