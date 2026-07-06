#pragma once
#include <string>
#include <cstdio>
#include <sys/types.h>

/**
 * @file VisionInput.h
 * @brief Interface for hand gesture recognition via Python subprocess
 * @author Kethy
 */

/**
 * @class VisionInput
 * @brief Manages a Python subprocess for real-time hand gesture detection
 * 
 * This class spawns and communicates with a Python script (detect.py) that
 * performs hand gesture recognition using the webcam. Gesture data is read
 * line-by-line from the subprocess's stdout, providing gesture names and
 * normalized hand position coordinates. The subprocess is automatically
 * terminated on destruction or when explicitly stopped.
 */
class VisionInput {
private:
    FILE* pipe;  ///< Pipe to read stdout from the Python subprocess
    pid_t pid;   ///< Process ID of the running Python script (-1 if not running)

public:
    /**
     * @brief Constructor - initializes with no active subprocess
     */
    VisionInput();

    /**
     * @brief Destructor - ensures subprocess is terminated before cleanup
     */
    ~VisionInput();

    /**
     * @brief Starts the gesture detection subprocess
     * 
     * Launches detect.py using the platform-specific Python interpreter from
     * the virtual environment. If a subprocess is already running, it is stopped
     * first. On macOS/Linux, uses pgrep to obtain the process ID for later
     * termination. On Windows, uses PowerShell to find the python process.
     * 
     * @return true if subprocess was successfully started, false on failure
     */
    bool startListening();

    /**
     * @brief Stops the gesture detection subprocess
     * 
     * Sends SIGTERM (Unix) or Stop-Process (Windows) to terminate the Python
     * script, then closes the pipe. Safe to call multiple times or when no
     * subprocess is running.
     */
    void stopListening();

    /**
     * @brief Reads the next gesture detection result from the subprocess
     * 
     * Reads one line from the Python script's stdout. Each line contains
     * space-separated values: gesture_name normalized_x normalized_y.
     * Coordinates are in the range [0.0, 1.0] representing relative position
     * on screen.
     * 
     * @param gesture Output parameter for the detected gesture name
     * @param nx Output parameter for normalized X coordinate (0.0 to 1.0)
     * @param ny Output parameter for normalized Y coordinate (0.0 to 1.0)
     * @return true if a gesture was successfully read, false if pipe is closed or read failed
     */
    bool getInput(std::string& gesture, float& nx, float& ny);
};