#include "VisionInput.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstdlib>

/**
 * @file VisionInput.cpp
 * @brief Implementation of hand gesture recognition subprocess manager
 * @author Kethy
 */


#define POPEN  _popen
#define PCLOSE _pclose
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

VisionInput::VisionInput() : pipe(nullptr), pid(-1) {}

VisionInput::~VisionInput() {
    // Ensure subprocess is terminated before destruction
    stopListening();
}

bool VisionInput::startListening() {
    // Safety: stop any existing listener first
    stopListening();

    pipe = popen("./venv/bin/python3 detect.py", "r");
    if (!pipe) {
        std::cerr << "Failed to start detect.py" << std::endl;
        return false;
    }

    pid = -1;

#ifdef __APPLE__
    FILE* pg = popen("pgrep -n -f detect.py", "r");
#else
    // Linux: pgrep -n finds the newest matching process
    FILE* pg = popen("pgrep -n -f detect.py", "r");
#endif
    if (pg) {
        char buf[32];
        if (fgets(buf, sizeof(buf), pg))
            pid = static_cast<int>(std::atoi(buf));
        pclose(pg);
    }

    return true;
}

void VisionInput::stopListening() {
    if (!pipe) return;

    // Terminate the Python process before closing pipe to prevent hang
    if (pid > 0) {
        kill(pid, SIGTERM);
        pid = -1;
    }

    pclose(pipe);
    pipe = nullptr;
}

bool VisionInput::getInput(std::string& gesture, float& nx, float& ny) {
    if (!pipe) return false;

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), pipe) == nullptr)
        return false;

    // Strip trailing newline/carriage return characters
    std::string line(buffer);
    std::size_t end = line.find_last_not_of("\n\r");
    if (end == std::string::npos)
        line.clear();
    else
        line.erase(end + 1);

    // Parse space-separated: gesture_name x y
    std::istringstream ss(line);
    ss >> gesture >> nx >> ny;

    return !gesture.empty();
}