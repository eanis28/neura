#include "History.h"
#include "UIHelpers.h"
#include "UIWidgets.h"
#include "Settings.h"

#include <SFML/Graphics.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <iomanip>
/**
 * @brief Represents a single logged command/response exchange.
 *
 * @details Mirrors the JSON object schema written by ConversationLogger.
 * Populated by loadHistory() when reading conversation.json.
 * @author Meridith Shang
 */
struct ConversationEntry {
    int         id        = 0;    ///< Sequential entry ID assigned at log time.
    std::string timestamp;        ///< ISO 8601 UTC timestamp of the exchange (e.g. "2026-03-24T17:44:45").
    std::string command;          ///< Raw command string issued by the user.
    std::string response;         ///< Reply or status message produced by the action.
    bool        success   = true; ///< true if the action completed without error; false otherwise.
};

// ── Minimal JSON parser ───────────────────────────────────────────────────────

/**
 * @author Meridith Shang
 * @brief Extracts the value associated with key from a flat JSON object string.
 *
 * @details Performs a lightweight, non-recursive parse — sufficient for the
 * flat objects written by ConversationLogger. Handles both quoted string
 * values (with \n, \t, \", and \\ escape sequences) and bare
 * unquoted values (numbers, booleans). Does not support nested objects or arrays.
 *
 * @param obj A JSON object substring, e.g. "{\"id\":1,\"command\":\"play\"}".
 * @param key The JSON key whose value should be extracted.
 * @return The extracted value as a string, with escape sequences decoded for
 *         quoted values; an empty string if the key is not found.
 */
static std::string extractString(const std::string& obj, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = obj.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t')) ++pos;
    if (pos >= obj.size()) return "";

    if (obj[pos] == '"') {
        ++pos;
        std::string result;
        while (pos < obj.size() && obj[pos] != '"') {
            if (obj[pos] == '\\' && pos + 1 < obj.size()) {
                ++pos;
                switch (obj[pos]) {
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    default:   result += obj[pos]; break;
                }
            } else {
                result += obj[pos];
            }
            ++pos;
        }
        return result;
    } else {
        size_t end = obj.find_first_of(",}\n", pos);
        std::string val = obj.substr(pos, end - pos);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);
        return val;
    }
}

/**
 * @author Meridith Shang
 * @brief Parses an ISO 8601 local-time string into a std::time_t value.
 *
 * @details Expects strings of the form "YYYY-MM-DDTHH:MM:SS" as written
 * by ConversationLogger::currentTimestamp(). Parsing is done with
 * std::get_time; the result is converted to a time_t via std::mktime(),
 * which interprets the broken-down time in the local timezone.
 *
 * @param ts The timestamp string to parse.
 * @return The corresponding std::time_t, or an implementation-defined
 *         value if parsing fails (typically -1 or 0).
 */
static std::time_t stringToTime(const std::string& ts) {
    std::tm tm = {};
    std::istringstream ss(ts);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

/**
 * @author Meridith Shang
 * @brief Loads, filters, and returns conversation history from a JSON log file.
 *
 * @details Reads the entire file at path into memory, then walks the content
 * brace-by-brace, extracting each ConversationEntry via extractString().
 * Entries are included only if:
 * - Their timestamp falls within the last 30 days.
 * - Their command field is non-empty.
 *
 * The returned vector is in reverse-chronological order (most recent first).
 * Returns an empty vector if the file cannot be opened.
 *
 * @param path Filesystem path to the conversation.json log file.
 * @return A reverse-chronologically ordered vector of ConversationEntry
 *         objects from the past 30 days, or an empty vector on failure.
 */
static std::vector<ConversationEntry> loadHistory(const std::string& path) {
    std::vector<ConversationEntry> entries;
    std::ifstream file(path);
    if (!file.is_open()) return entries;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    std::time_t now = std::time(nullptr);
    const long secondsIn30Days = 30 * 24 * 60 * 60;

    size_t pos = 0;
    while (pos < content.size()) {
        size_t start = content.find('{', pos);
        if (start == std::string::npos) break;
        size_t end = content.find('}', start);
        if (end == std::string::npos) break;
        std::string obj = content.substr(start, end - start + 1);

        ConversationEntry e;
        try { e.id = std::stoi(extractString(obj, "id")); } catch (...) {}
        e.timestamp = extractString(obj, "timestamp");
        e.command   = extractString(obj, "command");
        e.response  = extractString(obj, "response");
        std::string suc = extractString(obj, "success");
        e.success = (suc != "false");

        if (!e.timestamp.empty()) {
            std::time_t entryTime = stringToTime(e.timestamp);
            if (difftime(now, entryTime) <= secondsIn30Days) {
                if (!e.command.empty()) entries.push_back(e);
            }
        }

        pos = end + 1;
    }

    std::reverse(entries.begin(), entries.end());
    return entries;
}

// ── Text wrapping ─────────────────────────────────────────────────────────────

/**
 * @author Meridith Shang
 * @brief Wraps text to fit within maxWidth pixels at the given font and size.
 *
 * @details Splits the input on whitespace and greedily appends words to the
 * current line. Before each append, an sf::Text probe measures the candidate
 * line width; if it would exceed maxWidth, the current line is committed and
 * a new one is started with the overflowing word. Lines are joined with '\n'.
 *
 * @param text     The plain text string to wrap.
 * @param font     The SFML font used to measure glyph advances.
 * @param charSize Character size in pixels, matching the intended render size.
 * @param maxWidth Maximum line width in pixels before a line break is inserted.
 * @return A copy of @p text with @c '\n' characters inserted at wrap points.
 */
/// Returns the true rendered pixel width of @p str, matching the given render style.
static float textWidth(const sf::Font& font, const std::string& str,
                       unsigned charSize, sf::Text::Style style = sf::Text::Regular) {
    if (str.empty()) return 0.f;
    sf::Text probe(font, str, charSize);
    probe.setStyle(style);
    auto b = probe.getLocalBounds();
    return b.position.x + b.size.x;
}

/**
 * @author Meridith Shang
 * @brief Hard-breaks a single word into lines that fit within a pixel width.
 *
 * Iterates over word character by character, flushing the current line
 * with a `\n` whenever appending the next character would exceed maxWidth.
 * Called by `wrapText()` for words that are too wide to fit on a line on
 * their own.
 *
 * @param font      SFML font used to measure glyph widths.
 * @param word      The single whitespace-free token to break.
 * @param charSize  Character size in pixels, passed to `textWidth()`.
 * @param maxWidth  Maximum line width in pixels.
 * @param style     SFML text style (bold, italic, …) used during measurement.
 *                  Defaults to `sf::Text::Regular`.
 *
 * @return The broken string with `\n` inserted at each forced line break.
 *         The last segment is never followed by a trailing `\n`.
 *
 */
static std::string breakWord(const sf::Font& font,
                              const std::string& word,
                              unsigned charSize,
                              float maxWidth,
                              sf::Text::Style style = sf::Text::Regular) {
    std::string result, current;
    for (char c : word) {
        std::string test = current + c;
        if (textWidth(font, test, charSize, style) > maxWidth && !current.empty()) {
            result += current + '\n';
            current = std::string(1, c);
        } else {
            current += c;
        }
    }
    if (!current.empty()) result += current;
    return result;
}

/**
 * @brief Wraps a string to fit within a maximum pixel width.
 *
 * Splits text on whitespace and greedily fills each line until the next
 * word would exceed maxWidth. Words that are individually wider than
 * maxWidth are hard-broken character by character via `breakWord()`.
 *
 * @param text      The input string to wrap.
 * @param font      SFML font used to measure glyph widths.
 * @param charSize  Character size in pixels, passed to `textWidth()`.
 * @param maxWidth  Maximum line width in pixels. Lines will not exceed this.
 * @param style     SFML text style (bold, italic, …) used during measurement.
 *                  Defaults to `sf::Text::Regular`.
 *
 * @return The wrapped string with `\n` inserted at each line break.
 * @author Meridith Shang
 */
static std::string wrapText(const std::string& text,
                             const sf::Font&    font,
                             unsigned           charSize,
                             float              maxWidth,
                             sf::Text::Style    style = sf::Text::Regular) {
    std::istringstream words(text);
    std::string word, line, result;

    auto commitLine = [&]() {
        if (!result.empty()) result += '\n';
        result += line;
        line.clear();
    };

    while (words >> word) {
        // Single word too wide — hard-break it character by character
        if (textWidth(font, word, charSize, style) > maxWidth) {
            if (!line.empty()) commitLine();
            std::string broken = breakWord(font, word, charSize, maxWidth, style);
            size_t lastNL = broken.rfind('\n');
            if (lastNL == std::string::npos) {
                line = broken;
            } else {
                if (!result.empty()) result += '\n';
                result += broken.substr(0, lastNL);
                line = broken.substr(lastNL + 1);
            }
            continue;
        }

        std::string test = line.empty() ? word : line + " " + word;
        if (textWidth(font, test, charSize, style) <= maxWidth) {
            line = test;
        } else {
            commitLine();
            line = word;
        }
    }
    if (!line.empty()) {
        if (!result.empty()) result += '\n';
        result += line;
    }
    return result;
}

/**
 * @author Meridith Shang
 * @brief Returns the rendered height of a multi-line sf::Text string.
 *
 * @details Counts newline characters in str to determine the line count,
 * then multiplies by the font's line spacing at charSize. This gives a
 * consistent height measurement that accounts for actual line-break geometry
 * rather than relying on getLocalBounds(), which can vary with descenders.
 *
 * @param font     The SFML font used for rendering.
 * @param str      The (potentially multi-line) string to measure.
 * @param charSize Character size in pixels.
 * @return Total rendered height in pixels for all lines in str.
 */
static float measureTextHeight(const sf::Font& font,
                                const std::string& str,
                                unsigned charSize,
                                sf::Text::Style style = sf::Text::Regular) {
    if (str.empty()) return 0.f;
    int lines = 1 + (int)std::count(str.begin(), str.end(), '\n');
    // Use a probe to get the actual per-line height including style adjustments
    sf::Text probe(font, "Ag", charSize);
    probe.setStyle(style);
    auto b = probe.getLocalBounds();
    float lineH = b.position.y + b.size.y;
    return lineH * lines + font.getLineSpacing(charSize) * (lines - 1);
}
// ── Page ──────────────────────────────────────────────────────────────────────
bool runHistoryPage() {
    g_settings.load();

    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    float W = std::min((float)desktop.size.x * 0.55f, 860.f);
    float H = std::min((float)desktop.size.y * 0.88f, 900.f);

    sf::RenderWindow window(
        sf::VideoMode({(unsigned)W, (unsigned)H}),
        "Neura - History",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);
    window.setPosition({
        (int)((desktop.size.x - W) / 2),
        (int)((desktop.size.y - H) / 2)
    });

    sf::Font font;
    #if defined(__APPLE__)
        if (!font.openFromFile("/System/Library/Fonts/Avenir.ttc")) return false;
    #else
        if (!font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) return false;
    #endif

    sf::Texture backTex;
    bool hasBackImg = backTex.loadFromFile("images/back_arrow.png");

    auto entries = loadHistory("conversation.json");

    // ── Layout constants ──────────────────────────────────────────────────
    const float titleY      = H * 0.105f;
    const float subtitleY   = H * 0.178f;
    const float listTop     = H * 0.230f;
    const float listBottom  = H * 0.930f;
    const float listH       = listBottom - listTop;
    const float cardX       = W * 0.07f;
    const float cardW       = W * 0.86f;

    const float cardPadX    = 18.f;
    const float cardPadY    = 12.f;
    const float barWidth    = 4.f;
    const float barMargin   = 8.f;
    const float textOffsetX = cardPadX + barWidth + barMargin; // left text edge clears the accent bar
    const float textMaxW    = cardW - textOffsetX - cardPadX;  // available width for wrapped text
    const float cardGap     = 10.f;

    // Vertical spacing between the three text rows inside each card
    const float tsToCmd     = 5.f;
    const float cmdToResp   = 6.f;

    unsigned cmdSize  = (unsigned)(H * 0.026f);
    unsigned respSize = (unsigned)(H * 0.022f);
    unsigned tsSize   = (unsigned)(H * 0.016f);

    // ── Pre-measure card heights ───────────────────────────────────────────
    // Each card's height is computed once up front so the scroll range is exact
    struct CardLayout {
        float height = 0.f;
        std::string cmdWrapped;
        std::string respWrapped;
        float tsH   = 0.f;
        float cmdH  = 0.f;
        float respH = 0.f;
    };

    std::vector<CardLayout> layouts;
    layouts.reserve(entries.size());

    for (auto& e : entries) {
        CardLayout cl;
        cl.cmdWrapped  = wrapText(e.command,  font, cmdSize,  textMaxW, sf::Text::Bold);
        cl.respWrapped = wrapText(e.response, font, respSize, textMaxW);

        cl.tsH   = measureTextHeight(font, e.timestamp,    tsSize);
        cl.cmdH  = measureTextHeight(font, cl.cmdWrapped,  cmdSize, sf::Text::Bold);
        cl.respH = measureTextHeight(font, cl.respWrapped, respSize);

        // top pad + ts + gap + cmd + gap + resp + bottom pad
        cl.height = cardPadY + cl.tsH + tsToCmd + cl.cmdH + cmdToResp + cl.respH + cardPadY;
        layouts.push_back(cl);
    }

    // ── Scroll state ──────────────────────────────────────────────────────
    float totalContentH = 0.f;
    for (auto& cl : layouts) totalContentH += cl.height + cardGap;

    float scrollOffset = 0.f;
    float maxScroll    = std::max(0.f, totalContentH - listH);

    // ── Back button ───────────────────────────────────────────────────────
    CircleButton backBtn;
    backBtn.cx = W * 0.072f;
    backBtn.cy = H * 0.055f;
    backBtn.r  = H * 0.032f;

    // ── Clear button ──────────────────────────────────────────────────────
    const float clearW  = W * 0.22f;
    const float clearCX = W - W * 0.07f - clearW / 2.f;
    const float clearCY = H * 0.055f;
    const float clearH  = H * 0.052f;

    bool backClicked = false;

    // Resolve accent colour from user settings
    int ci = std::min(g_settings.colorIndex, UI_COLOUR_COUNT - 1);
    sf::Color accentCol(
        UI_COLOUR_PRESETS[ci].r,
        UI_COLOUR_PRESETS[ci].g,
        UI_COLOUR_PRESETS[ci].b);

    // Off-screen canvas used to clip the card list to the list region
    sf::RenderTexture listCanvas;
    listCanvas.resize({(unsigned)cardW + 20u, (unsigned)listH});

    // ── Main loop ─────────────────────────────────────────────────────────
    while (window.isOpen()) {
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>())
                window.close();

            if (auto* k = ev->getIf<sf::Event::KeyPressed>())
                if (k->code == sf::Keyboard::Key::Escape)
                    window.close();

            if (auto* s = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                scrollOffset -= s->delta * 30.f;
                scrollOffset = std::max(0.f, std::min(scrollOffset, maxScroll));
            }

            if (auto* r = ev->getIf<sf::Event::MouseButtonReleased>()) {
                if (r->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mp = window.mapPixelToCoords(r->position);

                    if (backBtn.hit(mp)) {
                        backClicked = true;
                        window.close();
                    }

                    if (pillHit(clearCX, clearCY, clearW, clearH, mp)) {
                        // Overwrite the file with an empty JSON array and reset all list state
                        std::ofstream clear("conversation.json", std::ios::trunc);
                        clear << "[\n]\n";
                        entries.clear();
                        layouts.clear();
                        totalContentH = 0.f;
                        maxScroll     = 0.f;
                        scrollOffset  = 0.f;
                    }
                }
            }
        }

        // ── Draw cards into off-screen canvas (provides list clipping) ────
        listCanvas.clear(sf::Color(255, 255, 255));

        float y = -scrollOffset;
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& e  = entries[i];
            const auto& cl = layouts[i];

            // Skip cards entirely above or below the visible list area
            if (y + cl.height < 0.f) { y += cl.height + cardGap; continue; }
            if (y > listH)           { break; }

            // Card background
            sf::RectangleShape card({cardW, cl.height});
            card.setPosition({0.f, y});
            card.setFillColor(sf::Color(247, 247, 245));
            listCanvas.draw(card);

            // Left accent bar: accent colour for success, red for failure
            sf::Color barCol = e.success ? accentCol : sf::Color(200, 60, 60);
            sf::RectangleShape bar({barWidth, cl.height - cardPadY});
            bar.setPosition({cardPadX * 0.5f, y + cardPadY * 0.5f});
            bar.setFillColor(barCol);
            listCanvas.draw(bar);

            float textX = textOffsetX;
            float curY  = y + cardPadY;

            // Timestamp (grey, smallest size)
            {
                sf::Text t(font, e.timestamp, tsSize);
                t.setFillColor(sf::Color(170, 170, 170));
                t.setPosition({textX, curY});
                listCanvas.draw(t);
            }
            curY += cl.tsH + tsToCmd;

            // Command (bold, near-black)
            {
                sf::Text t(font, cl.cmdWrapped, cmdSize);
                t.setFillColor(sf::Color(15, 15, 15));
                t.setStyle(sf::Text::Bold);
                t.setPosition({textX, curY});
                listCanvas.draw(t);
            }
            curY += cl.cmdH + cmdToResp;

            // Response (regular weight, mid-grey)
            {
                sf::Text t(font, cl.respWrapped, respSize);
                t.setFillColor(sf::Color(80, 80, 80));
                t.setPosition({textX, curY});
                listCanvas.draw(t);
            }

            y += cl.height + cardGap;
        }

        listCanvas.display();

        // ── Compose main window ───────────────────────────────────────────
        window.clear(sf::Color(255, 255, 255));

        // Title
        {
            sf::Text t(font, "History", (unsigned)(H * 0.105f));
            t.setFillColor(sf::Color(10, 10, 10));
            t.setStyle(sf::Text::Bold);
            auto b = t.getLocalBounds();
            t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
            t.setPosition({W / 2.f, titleY});
            window.draw(t);
        }

        // Subtitle
        {
            sf::Text t(font, "Your conversation history from the last 30 days",
                       (unsigned)(H * 0.022f));
            t.setFillColor(sf::Color(160, 160, 160));
            t.setLetterSpacing(1.4f);
            auto b = t.getLocalBounds();
            t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
            t.setPosition({W / 2.f, subtitleY});
            window.draw(t);
        }

        // Blit the clipped card list onto the main window
        sf::Sprite listSprite(listCanvas.getTexture());
        listSprite.setPosition({cardX, listTop});
        window.draw(listSprite);

        // Empty-state placeholder
        if (entries.empty()) {
            sf::Text t(font, "No conversations logged yet.\nStart talking to Neura!",
                       (unsigned)(H * 0.028f));
            t.setFillColor(sf::Color(190, 190, 190));
            t.setLineSpacing(1.4f);
            auto b = t.getLocalBounds();
            t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
            t.setPosition({W / 2.f, listTop + listH * 0.4f});
            window.draw(t);
        }

        // Scrollbar — only shown when content overflows the list area
        if (maxScroll > 0.f) {
            float trackH  = listH;
            float thumbH  = std::max(30.f, trackH * (listH / totalContentH));
            float thumbY  = listTop + (scrollOffset / maxScroll) * (trackH - thumbH);

            sf::RectangleShape track({3.f, trackH});
            track.setPosition({W - 8.f, listTop});
            track.setFillColor(sf::Color(220, 220, 218));
            window.draw(track);

            sf::RectangleShape thumb({3.f, thumbH});
            thumb.setPosition({W - 8.f, thumbY});
            thumb.setFillColor(sf::Color(160, 160, 158));
            window.draw(thumb);
        }

        {
            bool hov = pillHit(clearCX, clearCY, clearW, clearH, mouse);
            drawPill(window, clearCX, clearCY, clearW, clearH,
                     hov ? sf::Color(210, 210, 208) : sf::Color(225, 225, 223),
                     1.f, sf::Color(180, 180, 180));
            drawPillLabel(window, font, clearCX, clearCY, clearH,
                          "CLEAR HISTORY", sf::Color(80, 80, 80), 1.4f);
        }

        // Back button — image sprite if the texture loaded, plain circle otherwise
        if (hasBackImg) {
            sf::Sprite s(backTex);
            auto ts = backTex.getSize();
            float sz = backBtn.r * 2.f;
            s.setScale({sz / ts.x, sz / ts.y});
            s.setPosition({backBtn.cx - backBtn.r, backBtn.cy - backBtn.r});
            window.draw(s);
        } else {
            backBtn.draw(window, font, mouse);
        }

        window.display();
    }

    return backClicked;
}