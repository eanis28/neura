#include "UIHelpers.h"
#include <cmath>

// Compute centered origin using bounding box midpoint
sf::Vector2f centeredOrigin(const sf::Text& t) {
    auto b = t.getLocalBounds();
    return {b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f};
}

// Construct a pill shape using two semicircular arcs joined by straight edges
sf::ConvexShape makePill(float cx, float cy, float w, float h) {
    float r = h / 2.f;
    const int ARC = 18;
    std::vector<sf::Vector2f> pts;

    // Right semicircle
    float rightCX = cx + w/2.f - r;
    for (int i = 0; i <= ARC; ++i) {
        float a = (-90.f + 180.f * i / ARC) * 3.14159265f / 180.f;
        pts.push_back({rightCX + r * std::cos(a), cy + r * std::sin(a)});
    }

    // Left semicircle
    float leftCX = cx - w/2.f + r;
    for (int i = 0; i <= ARC; ++i) {
        float a = (90.f + 180.f * i / ARC) * 3.14159265f / 180.f;
        pts.push_back({leftCX + r * std::cos(a), cy + r * std::sin(a)});
    }

    sf::ConvexShape s(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) s.setPoint(i, pts[i]);
    return s;
}

// Simple bounding-box collision check (not curved edge accurate)
bool pillHit(float cx, float cy, float w, float h, sf::Vector2f p) {
    return p.x >= cx - w/2.f && p.x <= cx + w/2.f &&
           p.y >= cy - h/2.f && p.y <= cy + h/2.f;
}

// Draw pill with optional outline styling
void drawPill(sf::RenderWindow& win,
              float cx, float cy, float w, float h,
              sf::Color fill,
              float outlineThick,
              sf::Color outlineCol) {
    auto p = makePill(cx, cy, w, h);
    p.setFillColor(fill);
    if (outlineThick > 0.f) {
        p.setOutlineThickness(outlineThick);
        p.setOutlineColor(outlineCol);
    }
    win.draw(p);
}

// Draw centered text inside pill using dynamic sizing
void drawPillLabel(sf::RenderWindow& win,
                   const sf::Font& font,
                   float cx, float cy, float h,
                   const std::string& txt,
                   sf::Color col,
                   float letterSpacing) {
    sf::Text t(font, txt, (unsigned)(h * 0.40f));
    t.setFillColor(col);
    t.setLetterSpacing(letterSpacing);
    t.setOrigin(centeredOrigin(t));
    t.setPosition({cx, cy});
    win.draw(t);
}

// Draw left-aligned text anchored vertically at center
void drawTextLeft(sf::RenderWindow& win, const sf::Font& font,
                  const std::string& txt, unsigned size,
                  float x, float y, sf::Color col,
                  sf::Text::Style style, float letterSpacing) {
    sf::Text t(font, txt, size);
    t.setFillColor(col);
    t.setStyle(style);
    t.setLetterSpacing(letterSpacing);
    auto b = t.getLocalBounds();
    t.setOrigin({b.position.x, b.position.y + b.size.y / 2.f});
    t.setPosition({x, y});
    win.draw(t);
}

// Draw center-aligned text anchored both horizontally and vertically
void drawTextCenter(sf::RenderWindow& win, const sf::Font& font,
                    const std::string& txt, unsigned size,
                    float cx, float y, sf::Color col,
                    sf::Text::Style style, float letterSpacing) {
    sf::Text t(font, txt, size);
    t.setFillColor(col);
    t.setStyle(style);
    t.setLetterSpacing(letterSpacing);
    t.setOrigin(centeredOrigin(t));
    t.setPosition({cx, y});
    win.draw(t);
}