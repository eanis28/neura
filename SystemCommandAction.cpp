// SystemCommandAction.cpp

#include "SystemCommandAction.h"
#include <cstdlib>
#include <cstdio>
#include <array>
#include <string>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>
#include <filesystem>
#include <sstream>

#ifdef __APPLE__
#include <unistd.h>
#define POPEN  popen
#define PCLOSE pclose
#elif defined(__linux__)
#include <unistd.h>
#define POPEN  popen
#define PCLOSE pclose
#endif


/**
 * @brief Retrieves the current clipboard text on macOS.
 *
 * Uses the `pbpaste` command to read text currently stored
 * in the system clipboard.
 *
 * @return The clipboard contents as a string, or an empty string if unavailable.
 */

#ifdef __APPLE__
static std::string getClipboardText() {
    std::array<char, 256> buffer{};
    std::string result;
    FILE* pipe = popen("pbpaste", "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);
    return result;
}


/**
 * @brief Sets the clipboard text on macOS.
 *
 * Uses the `pbcopy` command to write the given text into
 * the system clipboard.
 *
 * @param text The text to place into the clipboard.
 */
static void setClipboardText(const std::string& text) {
    FILE* pipe = popen("pbcopy", "w");
    if (!pipe) return;

    fwrite(text.c_str(), 1, text.size(), pipe);
    pclose(pipe);
}
#endif

#if defined(__linux__)

/**
 * @brief Retrieves the current clipboard text on Linux.
 *
 * Uses the `xclip` utility to read clipboard contents,
 * falling back to `xsel` if xclip is unavailable.
 *
 * @return The clipboard contents as a string, or an empty string if unavailable.
 */

static std::string getClipboardText() {
    std::array<char, 256> buffer{};
    std::string result;
    FILE* pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
    if (!pipe) {
        pipe = popen("xsel --clipboard --output 2>/dev/null", "r");
        if (!pipe) return "";
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);
    return result;
}


/**
 * @brief Sets the clipboard text on Linux.
 *
 * Uses the `xclip` utility to write the given text into the clipboard,
 * falling back to `xsel` if xclip is unavailable.
 *
 * @param text The text to place into the clipboard.
 */

static void setClipboardText(const std::string& text) {
    FILE* pipe = popen("xclip -selection clipboard 2>/dev/null", "w");
    if (!pipe) {
        pipe = popen("xsel --clipboard --input 2>/dev/null", "w");
        if (!pipe) return;
    }

    fwrite(text.c_str(), 1, text.size(), pipe);
    pclose(pipe);
}
#endif

// ── Internal helpers ──────────────────────────────────────────────────────────

namespace {


/**
 * @brief Runs a shell command and captures its output.
 *
 * Opens a pipe to the given command, reads all text output,
 * and returns it as a string.
 *
 * @param cmd The shell command to execute.
 * @return The command output as a string, or an empty string if the command fails.
 */
std::string runCommandCapture(const char* cmd) {
    std::array<char, 256> buffer{};
    std::string result;
    FILE* pipe = POPEN(cmd, "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        result += buffer.data();
    PCLOSE(pipe);
    return result;
}

std::string getNextScreenshotPath() {
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    std::filesystem::path desktop = home
        ? std::filesystem::path(home) / "Desktop"
        : std::filesystem::current_path();
#elif defined(_WIN32)
    const char* userProfile = std::getenv("USERPROFILE");
    std::filesystem::path desktop = userProfile
        ? std::filesystem::path(userProfile) / "Desktop"
        : std::filesystem::current_path();
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path desktop = home
        ? std::filesystem::path(home) / "Desktop"
        : std::filesystem::current_path();
#endif

    std::filesystem::path base = desktop / "neura_screenshot.png";
    if (!std::filesystem::exists(base)) {
        return base.string();
    }

    for (int i = 1; i < 10000; ++i) {
        std::ostringstream name;
        name << "neura_screenshot" << i << ".png";
        std::filesystem::path candidate = desktop / name.str();
        if (!std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }

    return (desktop / "neura_screenshot_fallback.png").string();
}


/**
 * @brief Removes leading and trailing whitespace from a string.
 *
 * @param s The input string.
 * @return A trimmed copy of the input string.
 */

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}


/**
 * @brief Escapes a string for safe use in AppleScript.
 *
 * Escapes backslashes and quotation marks so the string can be
 * inserted safely into an AppleScript command.
 *
 * @param s The input string.
 * @return The escaped string.
 */
std::string escapeForAppleScript(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}


/**
 * @brief Escapes a string for safe use in a shell single-quoted argument.
 *
 * Replaces single quotes with the '"'"' sequence so the text
 * can be inserted safely into a shell command.
 *
 * @param s The input string.
 * @return The escaped string.
 */
std::string escapeForShellSingleQuoted(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\'') out += R"('"'"')";
        else out += c;
    }
    return out;
}


/**
 * @brief Runs a shell command using `std::system`.
 *
 * @param cmd The command to execute.
 * @return true if the command succeeded, false otherwise.
 */
bool runSystem(const std::string& cmd) {
    return std::system(cmd.c_str()) == 0;
}

} // namespace

// ── Volume & display ──────────────────────────────────────────────────────────

/**
 * @brief Changes the system output volume.
 *
 * On macOS, adjusts the current output volume directly.
 * On Linux, uses `pactl` (PulseAudio/PipeWire) to adjust the sink volume.
 *
 * @param delta The amount to change the volume by.
 *              Positive values increase volume and negative values decrease it.
 */

void SystemCommandAction::changeVolume(int delta) {
#ifdef __APPLE__
    std::string cmd =
        "osascript -e 'set volume output volume ((output volume of (get volume settings)) + "
        + std::to_string(delta) + ")'";
    std::system(cmd.c_str());

#elif defined(__linux__)
    std::string sign = (delta >= 0) ? "+" : "-";
    std::string cmd = "pactl set-sink-volume @DEFAULT_SINK@ "
                      + sign + std::to_string(std::abs(delta)) + "%";
    std::system(cmd.c_str());
#else
    (void)delta;
#endif
}

/**
 * @brief Changes the screen brightness.
 *
 * On macOS, simulates brightness key presses.
 * On Linux, uses `brightnessctl` to adjust backlight brightness.
 *
 * @param delta The amount to change the brightness by.
 *              Positive values increase brightness and negative values decrease it.
 */
void SystemCommandAction::changeBrightness(int delta) {
#ifdef __APPLE__
    int steps = std::max(1, (delta < 0 ? -delta : delta) / 10);
    for (int i = 0; i < steps; i++) {
        if (delta > 0)
            std::system("osascript -e 'tell application \"System Events\" to key code 144'");
        else
            std::system("osascript -e 'tell application \"System Events\" to key code 145'");
    }

#elif defined(__linux__)
    std::string sign = (delta >= 0) ? "+" : "-";
    std::string cmd = "brightnessctl set "
                      + sign + std::to_string(std::abs(delta)) + "% 2>/dev/null";
    std::system(cmd.c_str());
#else
    (void)delta;
#endif
}

// ── Window / screen ───────────────────────────────────────────────────────────

/**
 * @brief Takes a screenshot of the current screen.
 *
 * Saves a screenshot to the user's desktop using the
 * platform-specific screenshot method.
 */

bool SystemCommandAction::takeScreenshot() {
    std::string outputPath = getNextScreenshotPath();

#ifdef __APPLE__
    std::string cmd = "screencapture \"" + outputPath + "\"";
    bool ok = (std::system(cmd.c_str()) == 0);
    return ok && std::filesystem::exists(outputPath);

#elif defined(_WIN32)
    std::string safePath = outputPath;
    std::replace(safePath.begin(), safePath.end(), '\\', '/');

    std::string cmd =
        "powershell -NoProfile -Command \""
        "Add-Type -AssemblyName System.Windows.Forms; "
        "Add-Type -AssemblyName System.Drawing; "
        "$bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen; "
        "$bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height; "
        "$g = [System.Drawing.Graphics]::FromImage($bmp); "
        "$g.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bmp.Size); "
        "$path = '" + safePath + "'; "
        "$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); "
        "$g.Dispose(); "
        "$bmp.Dispose();\"";

    bool ok = (std::system(cmd.c_str()) == 0);
    return ok && std::filesystem::exists(outputPath);

#else
    return false;
#endif
}


/**
 * @brief Closes the currently active window.
 *
 * On macOS, avoids closing the Neura app itself when it is frontmost.
 * On Linux, uses `xdotool` to send Alt+F4 to the active window.
 *
 * @return true if the window close command succeeded, false otherwise.
 */

bool SystemCommandAction::closeWindow() {
#ifdef __APPLE__
    const char* script = R"APPLESCRIPT(
tell application "System Events"
    try
        set frontProc to first application process whose frontmost is true
        set frontName to name of frontProc

        if frontName is "Neura" then
            return "NEURA_FRONTMOST"
        end if

        keystroke "w" using command down
        return "OK"
    on error errMsg
        return "FAIL: " & errMsg
    end try
end tell
)APPLESCRIPT";

    std::string command = std::string("osascript <<'EOF'\n") + script + "\nEOF";
    std::string result = trim(runCommandCapture(command.c_str()));
    return result == "OK";

#elif defined(__linux__)
    return runSystem("xdotool getactivewindow key alt+F4 2>/dev/null");
#else
    return false;
#endif
}


/**
 * @brief Switches to the next browser tab.
 *
 * On macOS, this implementation specifically targets Safari.
 * On Linux, simulates Ctrl+Tab using `xdotool`.
 *
 * @return true if the tab switch command succeeded, false otherwise.
 */

bool SystemCommandAction::switchTabs() {
#ifdef __APPLE__
    const char* script = R"APPLESCRIPT(
tell application "System Events"
    try
        set frontProc to first application process whose frontmost is true
        set frontName to name of frontProc

        if frontName is "Neura" then
            return "NO"
        end if

        -- Most macOS browsers support Cmd+Shift+]
        keystroke "]" using {command down, shift down}
        return "OK"
    on error errMsg
        return "FAIL: " & errMsg
    end try
end tell
)APPLESCRIPT";

    std::string command = std::string("osascript <<'EOF'\n") + script + "\nEOF";
    std::string result = trim(runCommandCapture(command.c_str()));
    return result == "OK";

#elif defined(__linux__)
    return runSystem("xdotool key ctrl+Tab 2>/dev/null");
#else
    return false;
#endif
}


/**
 * @brief Switches to another open application window.
 *
 * On macOS, uses Command+Tab behavior.
 * On Linux, simulates Alt+Tab using `xdotool`.
 *
 * @return true if the window switch command succeeded, false otherwise.
 */

bool SystemCommandAction::switchWindows() {
#ifdef __APPLE__
    const char* script = R"APPLESCRIPT(
tell application "System Events"
    try
        set frontProc to first application process whose frontmost is true
        set frontName to name of frontProc

        if frontName is "Neura" then
            return "NO"
        end if

        key code 48 using {command down}
        return "OK"
    on error errMsg
        return "FAIL: " & errMsg
    end try
end tell
)APPLESCRIPT";

    std::string command = std::string("osascript <<'EOF'\n") + script + "\nEOF";
    std::string result = trim(runCommandCapture(command.c_str()));
    return result == "OK";

#elif defined(__linux__)
    return runSystem("xdotool key alt+Tab 2>/dev/null");
#else
    return false;
#endif
}


// ── Clipboard / text ──────────────────────────────────────────────────────────


/**
 * @brief Selects all text or content in the active application.
 *
 * Simulates the standard Select All keyboard shortcut.
 */

void SystemCommandAction::selectAll() {
#ifdef __APPLE__
    std::system("osascript -e 'tell application \"System Events\" to keystroke \"a\" using command down'");
#elif defined(__linux__)
    std::system("xdotool key ctrl+a 2>/dev/null");
#endif
}


/**
 * @brief Copies the currently selected text or content.
 *
 * Simulates the standard Copy keyboard shortcut.
 */

void SystemCommandAction::copyText() {
#ifdef __APPLE__
    std::system("osascript -e 'tell application \"System Events\" to keystroke \"c\" using command down'");
#elif defined(__linux__)
    std::system("xdotool key ctrl+c 2>/dev/null");
#endif
}


/**
 * @brief Pastes the current clipboard contents.
 *
 * Simulates the standard Paste keyboard shortcut.
 */

void SystemCommandAction::pasteText() {
#ifdef __APPLE__
    std::system("osascript -e 'tell application \"System Events\" to keystroke \"v\" using command down'");
#elif defined(__linux__)
    std::system("xdotool key ctrl+v 2>/dev/null");
#endif
}


/**
 * @brief Reads the currently selected text.
 *
 * Temporarily copies the current selection into the clipboard,
 * reads it, and then restores the previous clipboard contents.
 *
 * @return The selected text as a string, or an empty string if nothing was read.
 */

std::string SystemCommandAction::readSelectedText() {
    std::string oldClipboard = getClipboardText();

#if defined(__APPLE__)
    std::system(
        "osascript -e 'tell application \"System Events\" to keystroke \"c\" using command down'"
    );
#elif defined(__linux__)
    std::system("xdotool key ctrl+c 2>/dev/null");
#endif

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    std::string selected = getClipboardText();

    // restore original clipboard
    setClipboardText(oldClipboard);

    selected = trim(selected);

    if (selected.empty()) return "";

    return selected;
}

// ── Search computer ───────────────────────────────────────────────────────────

/**
 * @brief Opens the system search interface and types a query.
 *
 * On macOS, opens Spotlight search.
 * On Linux, opens the desktop search via Super key and types the query.
 *
 * @param query The text to search for on the computer.
 */


void SystemCommandAction::searchComputer(const std::string& query) {
#ifdef __APPLE__
    std::string safeQuery = escapeForAppleScript(query);
    std::string cmd =
        "osascript -e 'tell application \"System Events\" to keystroke space using command down' "
        "-e 'delay 0.3' "
        "-e 'tell application \"System Events\" to keystroke \"" + safeQuery + "\"'";
    std::system(cmd.c_str());

#elif defined(__linux__)
    // Open desktop search (works on GNOME/KDE) then type the query
    std::string safeQuery = escapeForShellSingleQuoted(query);
    std::string cmd =
        "xdotool key super 2>/dev/null; "
        "sleep 0.3; "
        "xdotool type --clearmodifiers '" + safeQuery + "' 2>/dev/null";
    std::system(cmd.c_str());
#else
    (void)query;
#endif
}

// ── Voice input ───────────────────────────────────────────────────────────────

/**
 * @brief Listens for a voice command using the Python voice listener script.
 *
 * Runs the external Python script responsible for capturing
 * and recognizing spoken commands.
 *
 * @return The recognized command as a trimmed string, or an empty string if unavailable.
 */

std::string SystemCommandAction::listenForCommand() {
#ifdef __APPLE__
    return trim(runCommandCapture("./neura-venv/bin/python3 ./voice_listener.py 2>/dev/null"));
#elif defined(__linux__)
    return trim(runCommandCapture("./neura-venv/bin/python3 ./voice_listener.py 2>/dev/null"));
#else
    return "";
#endif
}