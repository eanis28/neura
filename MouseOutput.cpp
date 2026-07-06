#include "MouseOutput.h"
#include <iostream>

/**
 * @file MouseOutput.cpp
 * @brief Implementation of cross-platform mouse and keyboard control
 * @author Kethy
 */

// ── Platform-specific includes ────────────────────────────────────────────────

#if defined(__APPLE__)
    #include <ApplicationServices/ApplicationServices.h>

#else
    #include <X11/Xlib.h>
    #include <X11/extensions/XTest.h>
    #include <X11/keysym.h>

    /**
     * @brief Returns singleton X11 display connection for Linux
     * 
     * Opens the display connection once and reuses it for all X11 operations.
     * 
     * @return Pointer to X11 Display, or nullptr if connection failed
     */
    static Display* getDisplay() {
        static Display* dpy = XOpenDisplay(nullptr);
        return dpy;
    }
#endif

MouseOutput::MouseOutput() : lastMouseX(0), lastMouseY(0), isDragging(false) {
#if defined(__APPLE__)
    // Query primary display dimensions using Core Graphics
    CGDirectDisplayID display = CGMainDisplayID();
    screenW = (float)CGDisplayPixelsWide(display);
    screenH = (float)CGDisplayPixelsHigh(display);

#else
    // Query screen dimensions via X11, fallback to 1920x1080 if unavailable
    Display* dpy = getDisplay();
    if (dpy) {
        Screen* scr = DefaultScreenOfDisplay(dpy);
        screenW = (float)WidthOfScreen(scr);
        screenH = (float)HeightOfScreen(scr);
    } else {
        screenW = 1920.f;
        screenH = 1080.f;
    }
#endif
}

void MouseOutput::moveMouse(float x, float y) {
#if defined(__APPLE__)
    CGPoint point = CGPointMake(x, y);
    CGEventRef event = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, point, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
#else
    Display* dpy = getDisplay();
    if (dpy) {
        XTestFakeMotionEvent(dpy, -1, (int)x, (int)y, CurrentTime);
        XFlush(dpy);  // Ensure event is sent immediately
    }
#endif
}

void MouseOutput::mouseDown(float x, float y) {
#if defined(__APPLE__)
    CGPoint point = CGPointMake(x, y);
    CGEventRef event = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, point, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);

#else
    Display* dpy = getDisplay();
    if (dpy) {
        XTestFakeMotionEvent(dpy, -1, (int)x, (int)y, CurrentTime);
        XTestFakeButtonEvent(dpy, 1, True, CurrentTime);  // Button 1 = left mouse
        XFlush(dpy);
    }
#endif
    isDragging = true;
}

void MouseOutput::mouseUp(float x, float y) {
#if defined(__APPLE__)
    CGPoint point = CGPointMake(x, y);
    CGEventRef event = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, point, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);

#else
    Display* dpy = getDisplay();
    if (dpy) {
        XTestFakeMotionEvent(dpy, -1, (int)x, (int)y, CurrentTime);
        XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
        XFlush(dpy);
    }
#endif
    isDragging = false;
}

void MouseOutput::mouseDrag(float x, float y) {
#if defined(__APPLE__)
    CGPoint point = CGPointMake(x, y);
    CGEventRef event = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDragged, point, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);

#else
    Display* dpy = getDisplay();
    if (dpy) {
        XTestFakeMotionEvent(dpy, -1, (int)x, (int)y, CurrentTime);
        XFlush(dpy);
    }
#endif
}

void MouseOutput::pressEscape() {
#if defined(__APPLE__)
    // Keycode 53 is Escape on macOS
    CGEventRef escDown = CGEventCreateKeyboardEvent(NULL, 53, true);
    CGEventRef escUp   = CGEventCreateKeyboardEvent(NULL, 53, false);
    CGEventPost(kCGHIDEventTap, escDown);
    CGEventPost(kCGHIDEventTap, escUp);
    CFRelease(escDown);
    CFRelease(escUp);

#else
    Display* dpy = getDisplay();
    if (dpy) {
        KeyCode esc = XKeysymToKeycode(dpy, XK_Escape);
        XTestFakeKeyEvent(dpy, esc, True,  CurrentTime);  // Press
        XTestFakeKeyEvent(dpy, esc, False, CurrentTime);  // Release
        XFlush(dpy);
    }
#endif
}

// ── Getters / Setters ─────────────────────────────────────────────────────────

bool  MouseOutput::getIsDragging() const        { return isDragging; }
void  MouseOutput::setLastPos(float x, float y) { lastMouseX = x; lastMouseY = y; }
float MouseOutput::getLastX()  const            { return lastMouseX; }
float MouseOutput::getLastY()  const            { return lastMouseY; }
float MouseOutput::getScreenW() const           { return screenW; }
float MouseOutput::getScreenH() const           { return screenH; }