/**
 * @file TimerAction.h
 * @brief Declaration of the TimerAction class and associated global timer state.
 *
 * This file defines the TimerAction class, which provides functionality for
 * setting a countdown timer. It also declares two global atomic variables used
 * to track the timer's active state and remaining time across threads.
 *
 * @author Eliza Anis
 */

#pragma once
#include <atomic>

/**
 * @brief Indicates whether a countdown timer is currently active.
 *
 * This global atomic flag is set to true when a timer is running and false
 * when no timer is active. It is used to prevent multiple concurrent timers.
 */
extern std::atomic<bool> g_timerActive;

/**
 * @brief Stores the remaining time in seconds for the active countdown timer.
 *
 * This global atomic integer is decremented each second while a timer is
 * running. A value of zero indicates the timer has expired.
 */
extern std::atomic<int> g_timerRemaining;

/**
 * @brief Handles setting and managing a countdown timer.
 *
 * The TimerAction class provides an interface for initiating a countdown timer
 * of a specified duration. It uses global atomic variables to safely track
 * timer state across threads, ensuring only one timer runs at a time.
 *
 * @author Eliza Anis
 */
class TimerAction {
public:

    /**
     * @brief Sets a countdown timer for the specified duration.
     *
     * Attempts to start a new countdown timer for the given number of seconds.
     * If a timer is already running (i.e., g_timerActive is true), the request
     * is rejected and false is returned. Otherwise, the timer is started in a
     * background thread, decrementing g_timerRemaining each second until it
     * reaches zero.
     *
     * @param seconds The duration of the timer in seconds. Must be a positive integer.
     * @return true if the timer was successfully started, false if a timer is already active.
     */
    bool setTimer(int seconds);
};