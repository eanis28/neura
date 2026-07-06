#pragma once

/**
 * @file MouseOutput.h
 * @brief Cross-platform mouse control and keyboard input simulation
 * @author Kethy
 */

/**
 * @class MouseOutput
 * @brief Provides platform-agnostic mouse and keyboard control via system APIs
 * 
 * This class encapsulates platform-specific mouse movement, clicking, dragging,
 * and keyboard input functionality. It uses Core Graphics on macOS, SendInput
 * on Windows, and XTest on Linux to simulate user input events. Screen dimensions
 * are automatically detected on construction for coordinate mapping.
 */
class MouseOutput {
private:
    float screenW;      ///< Screen width in pixels
    float screenH;      ///< Screen height in pixels
    float lastMouseX;   ///< Last known mouse X position
    float lastMouseY;   ///< Last known mouse Y position
    bool isDragging;    ///< Whether mouse button is currently held down

public:
    /**
     * @brief Constructor - detects screen dimensions for the primary display
     * 
     * Initializes screen width and height using platform-specific APIs:
     * CGDisplay on macOS, GetSystemMetrics on Windows, X11 on Linux.
     * Sets initial mouse position to (0, 0) and dragging state to false.
     */
    MouseOutput();

    /**
     * @brief Moves the mouse cursor to the specified screen coordinates
     * 
     * @param x Absolute X coordinate in pixels
     * @param y Absolute Y coordinate in pixels
     */
    void moveMouse(float x, float y);

    /**
     * @brief Simulates left mouse button press at the given position
     * 
     * Moves cursor to (x, y) and sends a mouse down event. Sets dragging
     * state to true.
     * 
     * @param x Absolute X coordinate in pixels
     * @param y Absolute Y coordinate in pixels
     */
    void mouseDown(float x, float y);

    /**
     * @brief Simulates left mouse button release at the given position
     * 
     * Moves cursor to (x, y) and sends a mouse up event. Sets dragging
     * state to false.
     * 
     * @param x Absolute X coordinate in pixels
     * @param y Absolute Y coordinate in pixels
     */
    void mouseUp(float x, float y);

    /**
     * @brief Simulates mouse drag movement with button held down
     * 
     * Moves cursor to (x, y) while maintaining left button press state.
     * Used during active drag operations.
     * 
     * @param x Absolute X coordinate in pixels
     * @param y Absolute Y coordinate in pixels
     */
    void mouseDrag(float x, float y);

    /**
     * @brief Simulates pressing the Escape key
     * 
     * Sends both key down and key up events for the Escape key using
     * platform-specific keyboard event APIs.
     */
    void pressEscape();

    /**
     * @brief Gets the current dragging state
     * 
     * @return true if mouse button is held down, false otherwise
     */
    bool getIsDragging() const;

    /**
     * @brief Updates the stored last mouse position
     * 
     * @param x X coordinate to store
     * @param y Y coordinate to store
     */
    void setLastPos(float x, float y);

    /**
     * @brief Gets the last stored X coordinate
     * 
     * @return Last mouse X position in pixels
     */
    float getLastX() const;

    /**
     * @brief Gets the last stored Y coordinate
     * 
     * @return Last mouse Y position in pixels
     */
    float getLastY() const;

    /**
     * @brief Gets the screen width
     * 
     * @return Primary display width in pixels
     */
    float getScreenW() const;

    /**
     * @brief Gets the screen height
     * 
     * @return Primary display height in pixels
     */
    float getScreenH() const;
};