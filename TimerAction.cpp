#include "TimerAction.h"
#include "UI.h"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

std::atomic<bool> g_timerActive(false);
std::atomic<int>  g_timerRemaining(0);

// ── Cross-platform timer alert ────────────────────────────────────────────────
static void playTimerFinishedAlert() {
#ifdef __APPLE__
    system("afplay /System/Library/Sounds/Glass.aiff");
    system("say \"Timer finished.\" &");
#elif defined(__linux__)
    // Try paplay (PulseAudio), fall back to aplay (ALSA)
    if (system("paplay /usr/share/sounds/freedesktop/stereo/complete.oga 2>/dev/null") != 0) {
        system("aplay /usr/share/sounds/alsa/Front_Center.wav 2>/dev/null &");
    }
    // Try espeak for TTS, fall back to spd-say (speech-dispatcher)
    if (system("which espeak > /dev/null 2>&1") == 0) {
        system("espeak \"Timer finished.\" &");
    } else {
        system("spd-say \"Timer finished.\" 2>/dev/null &");
    }
#endif
}

bool TimerAction::setTimer(int seconds) {
    if (seconds <= 0) return false;

    // allow only one timer at a time
    bool expected = false;
    if (!g_timerActive.compare_exchange_strong(expected, true)) {
        return false;
    }

    g_timerRemaining.store(seconds);

    std::thread([seconds]() {
        for (int remaining = seconds; remaining > 0; --remaining) {
            g_timerRemaining.store(remaining);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        g_timerRemaining.store(0);
        g_timerActive.store(false);

        {
            std::lock_guard<std::mutex> lock(sharedResponse.mtx);
            sharedResponse.text = "Timer finished.";
            sharedResponse.updated = true;
        }

        std::cout << "[Neura] Timer finished.\n";

        playTimerFinishedAlert();
    }).detach();

    return true;
}