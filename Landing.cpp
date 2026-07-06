#include "Landing.h"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <vector>
#include "Settings.h"
#include "UIHelpers.h"
#include "UI.h"

/**
 * @file Landing.cpp
 * @brief Implementation of the Neura application landing page
 * @author Kethy
 */

/**
 * @brief Attempts to load a system font with platform-specific fallbacks
 * 
 * Tries multiple font paths in order of preference for each platform:
 * macOS uses Avenir, Arial Unicode, or Arial; Windows uses Segoe UI,
 * Arial, or Calibri; Linux uses DejaVu Sans or Liberation Sans.
 * 
 * @param font Font object to load the system font into
 * @return true if any font was successfully loaded, false if all attempts failed
 */
static bool loadLandingFont(sf::Font& font) {
#ifdef __APPLE__
    static const char* candidates[] = {
        "/System/Library/Fonts/Avenir.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
#else
    static const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"
    };
#endif

    // Try each font path until one succeeds
    for (const char* path : candidates) {
        if (font.openFromFile(path)) return true;
    }
    return false;
}

bool runLandingPage() {
    g_settings.load();

    // Create window at half desktop size, centered on screen
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    unsigned windowWidth  = desktop.size.x / 2;
    unsigned windowHeight = desktop.size.y / 2;

    sf::RenderWindow window(
        sf::VideoMode({windowWidth, windowHeight}),
        "Neura",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);
    window.setPosition({
        static_cast<int>((desktop.size.x - windowWidth) / 2),
        static_cast<int>((desktop.size.y - windowHeight) / 2)
    });

    sf::Font font;
    #if defined(__APPLE__)
        if (!font.openFromFile("/System/Library/Fonts/Avenir.ttc")) return false;
    #else
        if (!font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) return false;
    #endif
    if (!loadLandingFont(font)) return false;

    // Calculate layout positions as proportions of window size
    float mid  = static_cast<float>(windowWidth)  / 2.f;
    float midY = static_cast<float>(windowHeight) / 2.f;
    float W    = static_cast<float>(windowWidth);
    float H    = static_cast<float>(windowHeight);

    // ── "Welcome to" text ──────────────────────────────────────────────────
    sf::Text welcomeText(font, "Welcome to", static_cast<unsigned>(H * 0.055f));
    welcomeText.setFillColor(sf::Color(20, 20, 20));
    {
        auto b = welcomeText.getLocalBounds();
        welcomeText.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        welcomeText.setPosition({mid, H * 0.17f});
    }

    // ── "Neura" title ──────────────────────────────────────────────────────
    sf::Text title(font, "Neura", static_cast<unsigned>(H * 0.175f));
    title.setFillColor(sf::Color(10, 10, 10));
    title.setStyle(sf::Text::Bold);
    {
        auto b = title.getLocalBounds();
        title.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        title.setPosition({mid, H * 0.345f});
    }

    // ── "YOUR PERSONAL ASSISTANT" subtitle ─────────────────────────────────
    sf::Text subtitle(font, "YOUR PERSONAL ASSISTANT", static_cast<unsigned>(H * 0.022f));
    subtitle.setFillColor(sf::Color(160, 160, 160));
    subtitle.setLetterSpacing(3.0f);
    {
        auto b = subtitle.getLocalBounds();
        subtitle.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        subtitle.setPosition({mid, H * 0.50f});
    }

    // ── Button layout dimensions and positions ─────────────────────────────
    float btnW   = W * 0.62f;
    float btnH   = H * 0.092f;
    float btn1Y  = H * 0.624f;
    float btn2Y  = H * 0.740f;
    float btn3Y  = H * 0.856f;

    sf::Color btnNormal(224, 224, 218);
    sf::Color btnHover (200, 200, 194);

    // Helper to create pill-shaped button background
    auto pillAt = [&](float y, sf::Color col) {
        auto p = makePill(mid, y, btnW, btnH);
        p.setFillColor(col);
        return p;
    };

    // Helper to create centered button label
    auto labelAt = [&](const std::string& str, float y) {
        sf::Text t(font, str, static_cast<unsigned>(btnH * 0.32f));
        t.setFillColor(sf::Color(50, 50, 50));
        t.setLetterSpacing(2.5f);
        auto b = t.getLocalBounds();
        t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        t.setPosition({mid, y});
        return t;
    };

    sf::Text startText    = labelAt("START",    btn1Y);
    sf::Text tutorialText = labelAt("TUTORIAL", btn2Y);
    sf::Text settingsText = labelAt("SETTINGS", btn3Y);

    // Define clickable areas for each button
    sf::FloatRect startHit   {{mid - btnW/2.f, btn1Y - btnH/2.f}, {btnW, btnH}};
    sf::FloatRect tutorialHit{{mid - btnW/2.f, btn2Y - btnH/2.f}, {btnW, btnH}};
    sf::FloatRect settingsHit{{mid - btnW/2.f, btn3Y - btnH/2.f}, {btnW, btnH}};

    while (window.isOpen()) {
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        while (auto event = window.pollEvent()) {
            // Close window on X button
            if (event->is<sf::Event::Closed>()) return false;

            // Exit on Escape key
            if (auto key = event->getIf<sf::Event::KeyPressed>())
                if (key->code == sf::Keyboard::Key::Escape) return false;

            if (auto click = event->getIf<sf::Event::MouseButtonReleased>()) {
                sf::Vector2f mp = window.mapPixelToCoords(click->position);
                
                // START button - proceed to main application
                if (startHit.contains(mp)) {
                    window.close();
                    return true;
                }
                
                // SETTINGS button - open settings, then return to landing
                if (settingsHit.contains(mp)) {
                    window.close();
                    runSettingsPage();
                    return runLandingPage();
                }

                // TUTORIAL button - enable tutorial mode and proceed to app
                if (tutorialHit.contains(mp)) {
                    startTutorial();
                    window.close();
                    return true;
                }
            }
        }

        // Update button appearance based on hover state
        auto startPill    = pillAt(btn1Y, startHit.contains(mouse)    ? btnHover : btnNormal);
        auto tutorialPill = pillAt(btn2Y, tutorialHit.contains(mouse)  ? btnHover : btnNormal);
        auto settingsPill = pillAt(btn3Y, settingsHit.contains(mouse)  ? btnHover : btnNormal);

        window.clear(sf::Color(255, 255, 255));

        window.draw(welcomeText);
        window.draw(title);
        window.draw(subtitle);

        window.draw(startPill);    window.draw(startText);
        window.draw(tutorialPill); window.draw(tutorialText);
        window.draw(settingsPill); window.draw(settingsText);

        window.display();
    }

    return false;
}