#include "SystemAction.h"
#include <cstdlib>
#include <cstdio>

SystemAction::SystemAction(const Configuration& cfg, MediaCommand cmd)
    : Action(cfg), command(cmd) {}

std::string SystemAction::describe() const {
    switch (command) {
        case MediaCommand::PLAY:   return "System: Play media";
        case MediaCommand::PAUSE:  return "System: Pause media";
        case MediaCommand::TOGGLE: return "System: Toggle play/pause";
    }
    return "System: Unknown media command";
}

bool SystemAction::execute() {
    if (!config.permissions.allowMediaControl) {
        log("MediaControl permission denied.");
        return false;
    }
    log("Executing: " + describe() + " (player: " + detectActivePlayer() + ")");
    switch (command) {
        case MediaCommand::PLAY:   return sendMediaKeyPlay();
        case MediaCommand::PAUSE:  return sendMediaKeyPause();
        // TOGGLE tries pause first; falls back to play if pause fails
        case MediaCommand::TOGGLE: return sendMediaKeyPause() || sendMediaKeyPlay();
    }
    return false;
}

// Dispatches to the correct platform command; winCmd is accepted but currently unused
bool SystemAction::sendMediaKey(const std::string& linuxCmd,
                                 const std::string& macCmd,
                                 const std::string& winCmd) const {
#if defined(__APPLE__)
    return std::system(macCmd.c_str()) == 0;
#else
    return std::system(linuxCmd.c_str()) == 0;
#endif
}

// Linux: sends XF86AudioPlay via xdotool; macOS: tells Spotify directly via osascript
bool SystemAction::sendMediaKeyPlay() const {
    log("Sending PLAY media key.");
    return sendMediaKey(
        "xdotool key XF86AudioPlay",
        "osascript -e 'tell application \"Spotify\" to play'",
        "powershell -Command \"(New-Object -ComObject WScript.Shell).SendKeys([char]0xB3)\""
    );
}

// Linux: sends XF86AudioPause via xdotool; macOS: tells Spotify directly via osascript
bool SystemAction::sendMediaKeyPause() const {
    log("Sending PAUSE media key.");
    return sendMediaKey(
        "xdotool key XF86AudioPause",
        "osascript -e 'tell application \"Spotify\" to pause'",
        "powershell -Command \"(New-Object -ComObject WScript.Shell).SendKeys([char]0xB3)\""
    );
}

// Queries the first MPRIS-capable player on the D-Bus session (Linux only);
// returns "unknown" if none is found or the query fails
std::string SystemAction::detectActivePlayer() const {
#if defined(__APPLE__)
    return "unknown (macOS — osascript query not yet implemented)";
#else
    FILE* pipe = popen("qdbus | grep mpris | head -1 2>/dev/null", "r");
    if (!pipe) return "unknown";
    char buf[256] = {};
    fgets(buf, sizeof(buf), pipe);
    pclose(pipe);
    std::string result(buf);
    result.erase(result.find_last_not_of(" \t\r\n") + 1); // strip trailing whitespace
    return result.empty() ? "unknown" : result;
#endif
}