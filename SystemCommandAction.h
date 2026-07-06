/**
 * @file SystemCommandAction.h
 * @brief System-level command execution for window/tab switching and text reading
 * @author Jasnav
 */
#pragma once
#include <string>


/**
 * @class SystemCommandAction
 * @brief Provides OS-level system command helpers for Neura.
 *
 * This class is responsible for performing system-related actions such as
 * adjusting volume and brightness, taking screenshots, switching tabs or
 * windows, starting and stopping screen recordings, interacting with the
 * clipboard, searching the computer, and listening for spoken commands.
 *
 * The implementation is intended to support cross-platform behavior where
 * possible, with platform-specific logic handled in the source file.
 */

class SystemCommandAction {

public:

    /**
     * @brief Changes the system volume.
     *
     * Increases or decreases the output volume depending on the value of
     * @p delta.
     *
     * @param delta The amount to change the volume by.
     *              Positive values increase volume, negative values decrease it.
     */

    void changeVolume(int delta);

    /**
     * @brief Changes the screen brightness.
     *
     * Increases or decreases the display brightness depending on the value
     * of @p delta.
     *
     * @param delta The amount to change the brightness by.
     *              Positive values increase brightness, negative values decrease it.
     */

    void changeBrightness(int delta);

    /**
     * @brief Takes a screenshot of the current screen.
     *
     * Captures the user's current screen and saves it using the
     * platform-specific screenshot mechanism.
     */

    bool takeScreenshot();


    /**
     * @brief Closes the currently active window.
     *
     * Executes the platform-specific shortcut or command to close
     * the focused application window.
     *
     * @return true if the command was executed successfully, false otherwise.
     */

    bool closeWindow();


    /**
     * @brief Switches to another browser tab.
     *
     * Executes the platform-specific shortcut for moving between tabs
     * in the active application, typically a browser.
     *
     * @return true if the command was executed successfully, false otherwise.
     */

    bool switchTabs();

    /**
     * @brief Switches to another open application window.
     *
     * Executes the platform-specific shortcut for cycling through
     * currently open windows.
     *
     * @return true if the command was executed successfully, false otherwise.
     */
    bool switchWindows();

    /**
     * @brief Selects all text or content in the active context.
     *
     * Simulates the system shortcut commonly used for "Select All".
     */
    void selectAll();

    /**
     * @brief Copies the currently selected text or content.
     *
     * Simulates the system shortcut commonly used for copying.
     */
    void copyText();

    /**
     * @brief Pastes clipboard contents into the active context.
     *
     * Simulates the system shortcut commonly used for pasting.
     */
    void pasteText();

    /**
     * @brief Reads the currently selected text.
     *
     * Attempts to access or retrieve the text currently selected by the user.
     *
     * @return A string containing the selected text, or an empty string
     *         if no text could be read.
     */
    std::string readSelectedText();



    /**
     * @brief Searches the computer for a given query.
     *
     * Opens the platform's search interface and enters the provided query.
     *
     * @param query The search text to look up on the computer.
     */
    void searchComputer(const std::string& query);

    /**
     * @brief Listens for a spoken voice command.
     *
     * Captures and returns a voice command as text, depending on the
     * implementation of the system's speech input mechanism.
     *
     * @return The recognized spoken command as a string.
     */
    std::string listenForCommand();
};