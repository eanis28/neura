#pragma once

/**
 * @file SystemAction.h
 * @brief OS-level media playback control via simulated media keys.
 *
 * Provides a cross-platform mechanism for pausing, playing, or toggling
 * media playback by dispatching the appropriate OS-level command for the
 * current platform
 * All platform branching is contained within sendMediaKey() to keep the
 * rest of the class portable.
 * @author Meridith Shang
 */

#include "action.h"

/**
 * @author Meridith Shang
 * @enum MediaCommand
 * @brief Identifies the media key action to send to the OS.
 *
 * @var MediaCommand::PLAY
 *      Start / resume playback.
 * @var MediaCommand::PAUSE
 *      Pause playback.
 * @var MediaCommand::TOGGLE
 *      Toggle between play and pause (default).
 */
enum class MediaCommand {
    PLAY,
    PAUSE,
    TOGGLE
};

/**
 * @author Meridith Shang
 * @class SystemAction
 * @brief Action that controls OS-level media playback.
 *
 * Inherits from Action and implements execute() by calling the platform
 * appropriate shell command that simulates a media key press. The active
 * media player is detected at execution time and logged for diagnostics.
 *
 */
class SystemAction : public Action {
public:
    /**
     * @author Meridith Shang
     * @brief Constructs a SystemAction with the given configuration and command.
     *
     * @param cfg  Application-wide configuration forwarded to the base Action.
     * @param cmd  The media command to issue on execute().
     *             Defaults to MediaCommand::TOGGLE.
     */
    explicit SystemAction(const Configuration& cfg,
                          MediaCommand cmd = MediaCommand::TOGGLE);

    /**
     * @author Meridith Shang
     * @brief Executes the configured media key command on the host OS.
     *
     * Detects the active media player, selects the correct platform command,
     * and dispatches it via sendMediaKey(). Logs the outcome.
     *
     * @return true if the OS command was dispatched successfully, false if the command failed or the platform is unsupported.
     */
    bool execute() override;

    /**
     * @author Meridith Shang
     * @brief Returns a human-readable description of this action.
     *
     * Includes the command type (PLAY / PAUSE / TOGGLE) and, where
     * available, the name of the active media player.
     *
     * @return Descriptive string, e.g. "SystemAction: TOGGLE (Spotify)".
     */
    std::string describe() const override;

    /**
     * @author Meridith Shang
     * @brief Replaces the media command that will be sent on the next execute().
     *
     * @param cmd  The new MediaCommand to use.
     */
    void setCommand(MediaCommand cmd) { command = cmd; }

private:
    MediaCommand command; ///< The media command to dispatch on execute().

    /**
     * @author Meridith Shang
     * @brief Dispatches the platform-appropriate media key command.
     *
     * Selects exactly one of the three provided command strings based on
     * the compiled target platform and runs it via std::system().
     *
     * @param linuxCmd  Shell command for Linux,   e.g. "xdotool key XF86AudioPlay".
     * @param macCmd    Shell command for macOS,   e.g. "osascript -e '...'".
     * @param winCmd    Shell command for Windows, e.g. "powershell -Command '...'".
     *
     * @return true  if std::system() returns 0 (success).
     * @return false on non-zero exit code or unsupported platform.
     */
    bool sendMediaKey(const std::string& linuxCmd,
                      const std::string& macCmd,
                      const std::string& winCmd) const;

    /**
     * @author Meridith Shang
     * @brief Sends the OS-level "Play" media key.
     *
     * Convenience wrapper around sendMediaKey() with pre-built
     * platform command strings for the play action.
     *
     * @return true on success, false on failure.
     */
    bool sendMediaKeyPlay() const;

    /**
     * @author Meridith Shang
     * @brief Sends the OS-level "Pause" media key.
     *
     * Convenience wrapper around sendMediaKey() with pre-built
     * platform command strings for the pause action.
     *
     * @return true on success, false on failure.
     */
    bool sendMediaKeyPause() const;

    /**
     * @author Meridith Shang
     * @brief Probes the OS for the currently active media player.
     *
     * Used for diagnostic logging inside describe() and execute().
     * Detection strategy is platform-specific (e.g. D-Bus on Linux,
     * AppleScript on macOS, WMI on Windows).
     *
     * @return Name of the active player (e.g. "Spotify", "VLC"),
     *         or an empty string if none could be detected.
     */
    std::string detectActivePlayer() const;
};