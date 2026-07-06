#include "QuickReference.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

/**
 * @brief Represents a single entry in the quick reference list.
 *
 * Each item can either be a section header or a command entry.
 */
struct Item {
    std::string title;   /**< Display title or command name */
    std::string desc;    /**< Description or voice command */
    bool isHeader;       /**< True if this item is a section header */
};

/**
 * @brief Runs the Quick Reference UI window.
 *
 * Creates a scrollable SFML window displaying categorized voice commands
 * and gesture controls for Neura. The content is rendered as a vertical list
 * with section headers and command descriptions.
 *
 * Features:
 * - Platform-specific font loading
 * - Scrollable content using mouse wheel
 * - Automatic layout generation from item list
 *
 * @note The window runs its own event loop and blocks until closed.
 */
void runQuickReference() {

    /// Main application window
    sf::RenderWindow window(
        sf::VideoMode({1200, 700}),
        "Neura Quick Reference",
        sf::Style::Titlebar | sf::Style::Close
    );

    window.setFramerateLimit(60);

    /// Font used for all UI text
    sf::Font font;

    #if defined(__APPLE__)
        font.openFromFile("/System/Library/Fonts/Avenir.ttc");
    #else
        font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    #endif

    // Load back button image
    sf::Texture backTex;
    bool hasBack = backTex.loadFromFile("images/back_arrow.png");

    float btnSz = 40.f;
    float btnY = 15.f;
    float backX = 20.f;
    float headerH = 80.f;  // Height of title bar

    /// List of all UI items (headers + commands)
    std::vector<Item> items = {
        {"", "", true},
        {"SYSTEM CONTROLS", "", true},
        {"Screenshot", "Say \"Take a screenshot\"", false},
        {"Timer", "Say \"Set timer for X seconds\"", false},
        {"Volume", "Say \"Increase volume\" or \"Decrease volume\"", false},
        {"Brightness", "Say \"Increase brightness\" or \"Decrease brightness\"", false},
        {"Close Window", "Say \"Close window\"", false},
        {"Search", "Say \"Search for ...\" or \"Find ...\" to search your system", false},
        {"Switch tabs", "Say \"Switch tabs\"", false},
        {"Switch windows", "Say \"Switch windows\"", false},
        {"Play media", "Say \"Play media\"", false},
        {"Pause media", "Say \"Pause media\"", false},

        {"", "", true},
        {"ASK NEURA", "", true},
        {"Play music on YouTube", "Say \"Play song [Song Name]\" (optional: \"by [Artist]\")", false},
        {"Get the weather", "Say \"What is the weather in [City]?\"", false},
        {"Add Google Calendar event", "Say \"Add event [Event Name] [Today/Tomorrow/Date] at [Time am/pm]\"", false},
        {"Get the time", "Say \"What time is it?\"", false},

        {"", "", true},
        {"TEXT COMMANDS", "", true},
        {"Read screen", "Say \"Read screen\"", false},
        {"Select all text", "Say \"Select all\"", false},
        {"Copy selected text", "Say \"Copy\" after selecting text", false},
        {"Paste clipboard contents", "Say \"Paste\"", false},

        {"", "", true},
        {"CONTROL NEURA", "", true},
        {"Pause Assistant", "Say \"Pause\" or \"Mute\"", false},
        {"Resume Assistant", "Say \"Resume\" or \"Unmute\"", false},

        {"", "", true},
        {"GESTURES", "", true},
        {"Point with one finger and move", "Moves cursor", false},
        {"Point with two fingers and move", "Drags", false},
        {"Open palm", "Cancels assistant/Assistant will stop", false},
        {"Thumbs Up", "Yes", false},
        {"Thumbs Down", "No", false},
        {"", "", true},
    };

    /// Pre-built drawable text objects for rendering
    std::vector<sf::Text> texts;

    /// Current vertical layout position
    float y = headerH + 20.f;  // Start below title

    sf::Text titleText(font, "Quick Reference", 28);
    titleText.setFillColor(sf::Color(20, 20, 20));
    titleText.setPosition({60.f, 20.f});

    /**
     * @brief Build text elements from item list.
     *
     * Converts each Item into one or two sf::Text objects depending on type.
     * Handles spacing and layout positioning.
     */
    for (const auto& item : items) {

        if (item.isHeader) {
            if (item.title.empty()) {
                y += 20.f;
                continue;
            }

            sf::Text header(font, item.title, 22);
            header.setFillColor(sf::Color(20, 20, 20));
            header.setPosition({40.f, y});
            texts.push_back(header);

            y += 26.f;
        }
        else {
            sf::Text command(font, item.title + ":", 18);
            command.setStyle(sf::Text::Bold);
            command.setFillColor(sf::Color(80, 80, 80));
            command.setPosition({40.f, y});

            sf::Text desc(font, item.desc.empty() ? "" : " " + item.desc, 18);
            desc.setFillColor(sf::Color(80, 80, 80));

            /// Align description next to command text
            float offsetX = command.getLocalBounds().size.x + 5.f;
            desc.setPosition({40.f + offsetX, y});

            texts.push_back(command);
            texts.push_back(desc);

            y += 26.f;
        }
    }

    /// Total height of all content (used for scrolling bounds)
    float contentHeight = y;

    /// Current vertical scroll offset
    float scrollOffset = 0.f;

    /**
     * @brief Main render and event loop.
     *
     * Handles window events, scrolling, and drawing UI elements.
     */
    while (window.isOpen()) {

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (auto click = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (click->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mp = window.mapPixelToCoords(click->position);
                    sf::FloatRect backRect{{backX, btnY}, {btnSz, btnSz}};
                    if (backRect.contains(mp)) {
                        window.close();
                    }
                }
            }
            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                scrollOffset += wheel->delta * 30.f;
            }
        }

        /// Clamp scroll range
        float minScroll = std::min(0.f, window.getSize().y - contentHeight);

        scrollOffset = std::min(scrollOffset, 0.f);
        scrollOffset = std::max(scrollOffset, minScroll);

        window.clear(sf::Color(245, 245, 245));

        // Draw scrollable content
        for (auto& t : texts) {
            sf::Vector2f pos = t.getPosition();
            t.setPosition({pos.x, pos.y + scrollOffset});
            window.draw(t);
            t.setPosition(pos);
        }

        // Draw title bar on top (solid white background)
        sf::RectangleShape titleBar({(float)window.getSize().x, headerH});
        titleBar.setFillColor(sf::Color(255, 255, 255));
        window.draw(titleBar);

        // Draw divider
        sf::RectangleShape divider({(float)window.getSize().x, 2.f});
        divider.setFillColor(sf::Color(200, 200, 200));
        divider.setPosition({0.f, headerH});
        window.draw(divider);

        // Draw back button
        if (hasBack) {
            sf::Sprite backBtn(backTex);
            backBtn.setScale({btnSz / (float)backTex.getSize().x, 
                            btnSz / (float)backTex.getSize().y});
            backBtn.setPosition({backX, btnY});
            window.draw(backBtn);
        }

        // Draw title
        sf::Text titleText(font, "Quick Reference", 28);
        titleText.setFillColor(sf::Color(20, 20, 20));
        titleText.setPosition({80.f, 20.f});  // Move to the right (adjust the 100.f)
        window.draw(titleText);

        window.display();
    }
}