#include "Landing.h"
#include "UI.h"
#include "VisionInput.h"
#include "MouseOutput.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <cmath>
#include <chrono>
#include "Settings.h"

/**
 * @file main.cpp
 * @brief Entry point and main control loop for the Neura application
 * @author Kethy
 */

/**
 * @brief Main application entry point
 * 
 * Orchestrates the application lifecycle through a loop of:
 * 1. Display landing page (start/settings/tutorial selection)
 * 2. Launch gesture recognition subprocess (detect.py)
 * 3. Run background thread for hand gesture to mouse control
 * 4. Display main UI with assistant features
 * 5. Clean up and return to landing or exit
 * 
 * The gesture recognition thread translates hand gestures into mouse events:
 * - POINTING: Move cursor
 * - TWO_FINGERS: Click (if stationary) or drag (if moving)
 * - OPEN_PALM: Cancel operations, release drag, close system UI
 * - THUMBS_UP/DOWN: Acknowledged but not yet mapped to actions
 * 
 * @return 0 on successful exit
 */
int main() {
    g_settings.load();

    while (true) {
        // Show landing page - exit if user closes window
        if (!runLandingPage()) break;

        // Start Python subprocess for gesture detection
        VisionInput vision;
        if (!vision.startListening()) break;

        // Background thread: read gestures and control mouse
        std::thread visionThread([&vision]() {
            MouseOutput mouse;
            std::string gesture, lastGesture = "NONE";
            float nx, ny;  // Normalized coordinates from vision (0.0-1.0)
            float dragStartX = 0, dragStartY = 0;

            // Two-finger gesture state: differentiate click vs drag
            float twoFingerStartX  = 0, twoFingerStartY = 0;
            bool  twoFingerPending = false;  // Waiting to see if stationary or moving
            auto  twoFingerStart   = std::chrono::steady_clock::now();

            constexpr float CLICK_MOVE_THRESHOLD = 0.006f;  // Max movement to still count as click
            constexpr int   CLICK_WAIT_MS        = 100;      // Wait time before triggering click

            while (vision.getInput(gesture, nx, ny)) {
                // Convert normalized coords to screen pixels (flip X for natural mirroring)
                float mouseX = (1.0f - nx) * mouse.getScreenW();
                float mouseY = ny * mouse.getScreenH();

                if (gesture == "THUMBS_UP") {
                    std::cout << "YES detected!" << std::endl;

                } else if (gesture == "THUMBS_DOWN") {
                    std::cout << "NO detected!" << std::endl;

                } else if (gesture == "OPEN_PALM" && lastGesture != "OPEN_PALM") {
                    // Cancel all active operations
                    twoFingerPending = false;
                    
                    // If dragging, smoothly return to drag start position and release
                    if (mouse.getIsDragging()) {
                        float cx = mouse.getLastX(), cy = mouse.getLastY();
                        constexpr int STEPS = 20;
                        for (int i = 1; i <= STEPS; i++) {
                            float t = static_cast<float>(i) / STEPS;
                            mouse.mouseDrag(cx + t * (dragStartX - cx),
                                            cy + t * (dragStartY - cy));
                        }
                        mouse.mouseUp(dragStartX, dragStartY);
                    }
                    
                    // Cancel TTS and close platform-specific system UI
                    cancelSpeech();
#ifdef __APPLE__
                    std::system("pkill -x screencapture 2>/dev/null");
                    // Close Spotlight only if it's the active window
                    std::system("osascript -e 'tell application \"System Events\"' "
                                "-e 'set f to name of first process whose frontmost is true' "
                                "-e 'if f is \"Spotlight\" then key code 53' "
                                "-e 'end tell' 2>/dev/null &");
#else
                    std::system("pkill -x krunner 2>/dev/null");
                    std::system("xdotool key super 2>/dev/null &");
#endif
                    std::cout << "CANCEL" << std::endl;

                } else if (gesture == "TWO_FINGERS") {
                    if (nx > 0 && ny > 0) {
                        // Fresh entry into two-finger gesture
                        if (lastGesture != "TWO_FINGERS") {
                            twoFingerStartX  = nx;
                            twoFingerStartY  = ny;
                            twoFingerPending = true;
                            twoFingerStart   = std::chrono::steady_clock::now();
                            dragStartX = mouse.getLastX();
                            dragStartY = mouse.getLastY();
                        }

                        // Recovery: if stuck in neither pending nor dragging, restart
                        if (!twoFingerPending && !mouse.getIsDragging()) {
                            twoFingerStartX  = nx;
                            twoFingerStartY  = ny;
                            twoFingerPending = true;
                            twoFingerStart   = std::chrono::steady_clock::now();
                            dragStartX = mouse.getLastX();
                            dragStartY = mouse.getLastY();
                        }

                        // Check if hand moved or time elapsed
                        float moved = std::hypot(nx - twoFingerStartX,
                                                 ny - twoFingerStartY);
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - twoFingerStart).count();

                        if (twoFingerPending) {
                            if (moved > CLICK_MOVE_THRESHOLD) {
                                // Hand moved: initiate drag
                                twoFingerPending = false;
                                if (!mouse.getIsDragging()) {
                                    mouse.mouseDown(dragStartX, dragStartY);
                                    std::cout << "GRAB" << std::endl;
                                }
                            } else if (elapsed > CLICK_WAIT_MS) {
                                // Hand stationary: trigger click
                                twoFingerPending = false;
                                mouse.mouseDown(mouse.getLastX(), mouse.getLastY());
                                mouse.mouseUp(mouse.getLastX(), mouse.getLastY());
                                std::cout << "CLICK" << std::endl;
                            }
                        } else {
                            // Already committed to drag - continue dragging
                            if (mouse.getIsDragging()) {
                                mouse.mouseDrag(mouseX, mouseY);
                                mouse.setLastPos(mouseX, mouseY);
                            }
                        }
                    }

                } else if (gesture == "POINTING") {
                    // Pointing gesture: move cursor only
                    twoFingerPending = false;
                    if (mouse.getIsDragging()) {
                        mouse.mouseUp(mouse.getLastX(), mouse.getLastY());
                        std::cout << "DROP" << std::endl;
                    }
                    if (nx > 0 && ny > 0) {
                        mouse.moveMouse(mouseX, mouseY);
                        mouse.setLastPos(mouseX, mouseY);
                    }

                } else {
                    // Unknown gesture: cancel any active operations
                    twoFingerPending = false;
                    if (mouse.getIsDragging()) {
                        mouse.mouseUp(mouse.getLastX(), mouse.getLastY());
                        std::cout << "DROP" << std::endl;
                    }
                }

                // Update global gesture state for UI rendering
                if      (gesture == "POINTING")    g_gestureState.store(1);
                else if (gesture == "TWO_FINGERS") g_gestureState.store(2);
                else if (gesture == "OPEN_PALM")   g_gestureState.store(3);
                else if (gesture == "THUMBS_UP")   g_gestureState.store(4);
                else if (gesture == "THUMBS_DOWN") g_gestureState.store(5);
                else                               g_gestureState.store(0);

                lastGesture = gesture;
            }
            
            // Vision input ended - clear gesture state
            g_gestureState.store(0);
        });

        // Run main UI - blocks until user closes or clicks back
        bool wentBack = runUI();

        // Clean up: stop vision subprocess and wait for thread to finish
        vision.stopListening();
        visionThread.join();

        // If user exited (not back to landing), break outer loop
        if (!wentBack) break;
    }

    return 0;
}