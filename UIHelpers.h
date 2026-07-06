/**
 * @file UIHelpers.h
 * @brief Helper functions for UI rendering and layout calculations
 * @author Kethy
 */
#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

/**
@brief Calculates the origin required to center an SFML Text object.

This function computes the center point of the text's bounding box,
allowing it to be positioned using its visual center instead of the
default top-left origin.

@param t The SFML Text object to center.
@return sf::Vector2f The origin coordinates for centering the text.

@author Kethy
*/
sf::Vector2f centeredOrigin(const sf::Text& t);

/**
@brief Creates a pill-shaped (capsule) convex shape.

Generates a smooth capsule shape using two semicircles connected by
straight edges. The shape is centered at the specified coordinates.

@param cx X-coordinate of the center.
@param cy Y-coordinate of the center.
@param w Width of the pill.
@param h Height of the pill.
@return sf::ConvexShape A convex shape representing the pill.

@author Kethy
*/
sf::ConvexShape makePill(float cx, float cy, float w, float h);

/**
@brief Checks if a point lies within a pill's bounding box.

Performs a simple bounding box collision test for a pill-shaped UI element.
Note that this does not account for curved edges, only the rectangular bounds.

@param cx X-coordinate of the pill center.
@param cy Y-coordinate of the pill center.
@param w Width of the pill.
@param h Height of the pill.
@param p The point to test.
@return bool True if the point is inside the bounding box, false otherwise.

@author Kethy
*/
bool pillHit(float cx, float cy, float w, float h, sf::Vector2f p);

/**
@brief Draws a pill-shaped UI element.

Renders a capsule shape with optional outline styling.

@param win The render window to draw to.
@param cx X-coordinate of the center.
@param cy Y-coordinate of the center.
@param w Width of the pill.
@param h Height of the pill.
@param fill Fill color of the pill.
@param outlineThick Thickness of the outline (default 0 = no outline).
@param outlineCol Color of the outline.
@return void

@author Kethy
*/
void drawPill(sf::RenderWindow& win,
              float cx, float cy, float w, float h,
              sf::Color fill,
              float outlineThick = 0.f,
              sf::Color outlineCol = sf::Color::White);

/**
@brief Draws centered text inside a pill shape.

Automatically sizes and positions text relative to the pill height,
ensuring consistent UI appearance.

@param win The render window to draw to.
@param font Font used for rendering text.
@param cx X-coordinate of the center.
@param cy Y-coordinate of the center.
@param h Height of the pill (used for text scaling).
@param txt The text string to display.
@param col Text color.
@param letterSpacing Spacing between characters.
@return void

@author Kethy
*/
void drawPillLabel(sf::RenderWindow& win,
                   const sf::Font& font,
                   float cx, float cy, float h,
                   const std::string& txt,
                   sf::Color col,
                   float letterSpacing = 1.f);

/**
@brief Draws left-aligned text at a given position.

The text is aligned to the left at the specified X coordinate,
and vertically centered on the Y coordinate.

@param win The render window to draw to.
@param font Font used for rendering text.
@param txt The text string to display.
@param size Character size.
@param x X-coordinate (left-aligned).
@param y Y-coordinate (vertical center).
@param col Text color.
@param style Text style (e.g., bold, italic).
@param letterSpacing Spacing between characters.
@return void

@author Kethy
*/
void drawTextLeft(sf::RenderWindow& win, const sf::Font& font,
                  const std::string& txt, unsigned size,
                  float x, float y, sf::Color col,
                  sf::Text::Style style = sf::Text::Regular,
                  float letterSpacing = 1.f);

/**
@brief Draws center-aligned text at a given position.

The text is horizontally centered at the specified X coordinate,
and vertically centered on the Y coordinate.

@param win The render window to draw to.
@param font Font used for rendering text.
@param txt The text string to display.
@param size Character size.
@param cx X-coordinate (center-aligned).
@param y Y-coordinate (vertical center).
@param col Text color.
@param style Text style (e.g., bold, italic).
@param letterSpacing Spacing between characters.
@return void

@author Kethy
*/
void drawTextCenter(sf::RenderWindow& win, const sf::Font& font,
                    const std::string& txt, unsigned size,
                    float cx, float y, sf::Color col,
                    sf::Text::Style style = sf::Text::Regular,
                    float letterSpacing = 1.f);