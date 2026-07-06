/**
 * @file UIWidgets.h
 * @brief Reusable UI widget components and drawing utilities
 * @author Kethy
 */
#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "UIHelpers.h"

/**
@brief Dropdown UI component styled as a pill shape.

Provides a selectable list of options with a collapsible interface.
Supports scrolling when the number of options exceeds visible capacity.

@author Kethy
*/
struct PillDropdown {
    float cx, cy, w, h;
    std::vector<std::string> options;
    int  index = 0, scrollOffset = 0;
    bool open  = false;
    static constexpr int   MAX_VIS = 5;
    static constexpr float ITEM_H  = 36.f;

    /**
    @brief Initializes the dropdown.

    @param x Center X position.
    @param y Center Y position.
    @param width Width of the dropdown.
    @param height Height of the dropdown.
    @param opts List of options.
    @param idx Initial selected index.
    @return void
    */
    void init(float x, float y, float width, float height,
              const std::vector<std::string>& opts, int idx);

    /**
    @brief Checks if a point hits the dropdown header.

    @param p Mouse position.
    @return bool True if header is clicked.
    */
    bool hitHeader(sf::Vector2f p) const;

    /**
    @brief Handles mouse release events.

    Selects an option or toggles dropdown state.

    @param p Mouse position.
    @return bool True if interaction occurred.
    */
    bool onRelease(sf::Vector2f p);

    /**
    @brief Handles scroll input for dropdown list.

    @param p Mouse position.
    @param delta Scroll delta value.
    @return void
    */
    void onScroll(sf::Vector2f p, float delta);

    /**
    @brief Draws the dropdown header.

    @param win Render window.
    @param font Font used for text.
    @return void
    */
    void drawHeader(sf::RenderWindow& win, const sf::Font& font) const;

    /**
    @brief Draws the dropdown list when open.

    @param win Render window.
    @param font Font used for text.
    @return void
    */
    void drawList(sf::RenderWindow& win, const sf::Font& font) const;
};

/**
@brief Pill-shaped text input field.

Supports cursor movement, text selection, clipboard operations,
placeholder text, and double-click selection.

@author Kethy
*/
struct PillTextField {
    float cx, cy, w, h;
    std::string text, placeholder;
    bool focused   = false;
    int  cursorPos = 0;

    // selection: selStart == selEnd means no selection
    int  selStart  = 0;
    int  selEnd    = 0;

    sf::Clock blinkClock;

    // double-click detection
    sf::Clock lastClickClock;
    bool      clickArmed = false;

    /**
    @brief Initializes the text field.

    @param x Center X position.
    @param y Center Y position.
    @param width Width of the field.
    @param height Height of the field.
    @param initial Initial text value.
    @param ph Placeholder text.
    @return void
    */
    void init(float x, float y, float width, float height,
              const std::string& initial, const std::string& ph = "");

    /**
    @brief Checks if a point is inside the text field.

    @param p Mouse position.
    @return bool True if inside.
    */
    bool hit(sf::Vector2f p) const;

    /**
    @brief Sets focus state of the field.

    @param f True to focus, false to unfocus.
    @return void
    */
    void setFocus(bool f);

    /**
    @brief Handles mouse click behavior.

    Supports double-click to select all text.

    @return void
    */
    void onClick();

    /**
    @brief Deletes currently selected text.

    @return void
    */
    void deleteSelection();

    /**
    @brief Checks if text is selected.

    @return bool True if selection exists.
    */
    bool hasSelection() const { return selStart != selEnd; }

    /**
    @brief Selects all text.

    @return void
    */
    void selectAll()          { selStart = 0; selEnd = (int)text.size(); cursorPos = selEnd; }

    /**
    @brief Clears current selection.

    @return void
    */
    void clearSelection()     { selStart = selEnd = cursorPos; }

    /**
    @brief Handles text input events.

    @param u Unicode character entered.
    @return void
    */
    void onTextEntered(uint32_t u);

    /**
    @brief Handles key press events.

    Supports navigation, deletion, and clipboard shortcuts.

    @param key Key pressed.
    @param modifier Modifier key (Ctrl/Cmd).
    @return void
    */
    void onKeyPressed(sf::Keyboard::Key key, bool modifier);

    /**
    @brief Draws the text field.

    @param win Render window.
    @param font Font used for text.
    @param accent Accent color for focus state.
    @return void
    */
    void draw(sf::RenderWindow& win, const sf::Font& font, sf::Color accent);
};

/**
@brief Horizontal slider UI component.

Allows selection of a value between 0 and 100 via dragging.

@author Kethy
*/
struct Slider {
    float cx, cy, w;
    int   value    = 0;
    bool  dragging = false;

    /**
    @brief Initializes the slider.

    @param x X position.
    @param y Y position.
    @param width Width of the slider.
    @param v Initial value.
    @return void
    */
    void init(float x, float y, float width, int v);

    /**
    @brief Computes the thumb X position.

    @return float X coordinate of the slider thumb.
    */
    float thumbX() const;

    /**
    @brief Checks if a point hits the slider area.

    @param p Mouse position.
    @return bool True if within interaction zone.
    */
    bool  hitArea(sf::Vector2f p) const;

    /**
    @brief Updates value based on X position.

    @param px Mouse X coordinate.
    @return void
    */
    void  applyX(float px);

    /**
    @brief Draws the slider.

    @param win Render window.
    @return void
    */
    void  draw(sf::RenderWindow& win) const;
};

/**
@brief Circular button UI component.

Typically used for navigation (e.g., back button).

@author Kethy
*/
struct CircleButton {
    float cx, cy, r;

    /**
    @brief Checks if a point is inside the button.

    @param p Mouse position.
    @return bool True if inside.
    */
    bool hit(sf::Vector2f p) const;

    /**
    @brief Draws the button.

    @param win Render window.
    @param font Font used for icon/text.
    @param mouse Current mouse position (for hover effect).
    @return void
    */
    void draw(sf::RenderWindow& win, const sf::Font& font, sf::Vector2f mouse) const;
};