#include "Settings.h"
#include "config.h"
#include "UIHelpers.h"
#include "UIWidgets.h"

#include <SFML/Graphics.hpp>
#include <portaudio.h>
#include <opencv2/videoio.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

/**
 * @file Settings.cpp
 * @brief Implementation of settings management and settings UI
 * @author Kethy
 */

// ── Presets ───────────────────────────────────────────────────────────────────

AppSettings g_settings;

const ColourPreset UI_COLOUR_PRESETS[] = {
    {"Pink",   255,  20, 147},
    {"Purple", 180,   0, 255},
    {"Blue",     0,  80, 255},
    {"Cyan",     0, 200, 220},
    {"Green",    0, 210,  80},
};
const int UI_COLOUR_COUNT = 5;

const char* ACCENT_NAMES[] = {
    "American", "British", "Australian", "Canadian"
};
const int ACCENT_COUNT = 4;

void AppSettings::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        try {
            if      (key == "volume")      volume      = std::stoi(val);
            else if (key == "accent")      accentIndex = std::stoi(val);
            else if (key == "color")       colorIndex  = std::stoi(val);
            else if (key == "camera")      cameraIndex = std::stoi(val);
            else if (key == "mic")         micIndex    = std::stoi(val);
        } catch (...) {}  // Silently ignore invalid values
    }
}

void AppSettings::save(const std::string& path) const {
    std::ofstream f(path);
    f << "volume="      << volume      << "\n"
      << "accent="      << accentIndex << "\n"
      << "color="       << colorIndex  << "\n"
      << "camera="      << cameraIndex << "\n"
      << "mic="         << micIndex    << "\n";
}

bool runSettingsPage() {
    g_settings.load();

    // ── Enumerate available microphones via PortAudio ─────────────────────
    std::vector<std::string> micNames = {"Default"};
    std::vector<int>         micIdxs  = {-1};
    Pa_Initialize();
    for (int i = 0; i < Pa_GetDeviceCount(); ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->maxInputChannels > 0) {
            micNames.push_back(info->name);
            micIdxs.push_back(i);
        }
    }
    Pa_Terminate();
    
    // Find which dropdown index matches the saved mic
    int micSel = 0;
    for (int i = 1; i < (int)micIdxs.size(); ++i)
        if (micIdxs[i] == g_settings.micIndex) { micSel = i; break; }

    // ── Create window centered at 62% desktop size ─────────────────────────
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    float W = std::min((float)desktop.size.x * 0.62f, 980.f);
    float H = std::min((float)desktop.size.y * 0.88f, 900.f);

    sf::RenderWindow window(
        sf::VideoMode({(unsigned)W,(unsigned)H}),
        "Neura - Settings",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);
    window.setPosition({(int)((desktop.size.x-W)/2),(int)((desktop.size.y-H)/2)});

    sf::Texture backTex;
    bool hasBackImg = backTex.loadFromFile("images/back_arrow.png");

    sf::Texture questionTex;
    bool hasQuestionImg = questionTex.loadFromFile("images/question_button.png");
    const float qSz = H * 0.026f;

    // tooltip text per token row
    const std::string tooltips[4] = {
        " Go to Section 'API Token Setup for Google Calendar' in the README and follow the instructions to obtain this token.",
        "Go to Section 'API Token Setup for Google Calendar' in the README and follow the instructions to obtain this token.",
        "Go to Section 'API Token Setup for Google Calendar' in the README and follow the instructions to obtain this token.",
        "Go to Section 'YouTube API Setup' in the README and follow the instructions to obtain this API key."
    };

    sf::Font font;
    #if defined(__APPLE__)
        if (!font.openFromFile("/System/Library/Fonts/Avenir.ttc")) return false;
    #else
        if (!font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) return false;
    #endif

    // ── Layout constants (Y-positions as percentages of window height) ────
    const float pillH     = 42.f;
    const float ctlW      = W * 0.42f;
    const float ctlCX     = W * 0.64f;
    const float lblX      = W * 0.07f;

    const float titleY    = H * 0.080f;
    const float subtitleY = H * 0.150f;
    const float sec1Y     = H * 0.195f;
    const float colourY   = H * 0.250f;
    const float voiceY    = H * 0.320f;
    const float sec2Y     = H * 0.380f;
    const float micY      = H * 0.435f;
    const float token1Y   = H * 0.510f;
    const float token2Y   = H * 0.585f;
    const float token3Y   = H * 0.660f;
    const float token4Y   = H * 0.735f;
    const float volumeY   = H * 0.815f;
    const float saveY     = H * 0.910f;

    const float tokenFieldW  = ctlW * 0.62f;
    const float submitW      = ctlW * 0.30f;
    const float tokenFieldCX = ctlCX - ctlW/2.f + tokenFieldW/2.f;
    const float submitCX     = ctlCX + ctlW/2.f - submitW/2.f;

    // ── Detect available TTS voices at runtime ────────────────────────────
    struct VoiceOption {
        std::string displayName;  // shown in dropdown
        std::string voiceId;      // passed to TTS command
        int         accentIndex;  // maps to g_settings.accentIndex
    };

    std::vector<VoiceOption> availableVoices;

#if defined(__APPLE__)
    // Query macOS `say -v ?` and match against known voices
    struct KnownVoice { const char* sayName; const char* display; int idx; };
    static const KnownVoice knownVoices[] = {
        {"Samantha", "American (Samantha)", 0},
        {"Daniel",   "British (Daniel)",    1},
        {"Karen",    "Australian (Karen)",  2},
        {"Fiona",    "Scottish (Fiona)",    2},
        {"Moira",    "Irish (Moira)",       1},
    };
    FILE* sayList = popen("/usr/bin/say -v ? 2>/dev/null", "r");
    std::vector<std::string> installedVoices;
    if (sayList) {
        char line[256];
        while (fgets(line, sizeof(line), sayList)) {
            std::string s(line);
            size_t end = s.find_first_of(" \t");
            if (end != std::string::npos)
                installedVoices.push_back(s.substr(0, end));
        }
        pclose(sayList);
    }
    // Match installed voices against known mappings, avoiding duplicates
    for (const auto& kv : knownVoices) {
        for (const auto& iv : installedVoices) {
            if (iv == kv.sayName) {
                bool already = false;
                for (const auto& av : availableVoices)
                    if (av.accentIndex == kv.idx) { already = true; break; }
                if (!already)
                    availableVoices.push_back({kv.display, kv.sayName, kv.idx});
                break;
            }
        }
    }

#else
    // Query Linux espeak for available English variants
    struct KnownVoice { const char* espeakId; const char* display; int idx; };
    static const KnownVoice knownVoices[] = {
        {"en",    "American (en)",    0},
        {"en-us", "American (en-us)", 0},
        {"en-gb", "British (en-gb)",  1},
        {"en-au", "Australian",       2},
        {"en-ca", "Canadian",         3},
    };
    FILE* espkList = popen("espeak --voices=en 2>/dev/null", "r");
    std::vector<std::string> installedVoices;
    if (espkList) {
        char line[256];
        while (fgets(line, sizeof(line), espkList)) {
            std::string s(line);
            std::istringstream ss(s);
            std::string tok1, tok2;
            ss >> tok1 >> tok2;
            if (!tok2.empty()) installedVoices.push_back(tok2);
        }
        pclose(espkList);
    }
    for (const auto& kv : knownVoices) {
        for (const auto& iv : installedVoices) {
            if (iv == kv.espeakId) {
                bool already = false;
                for (const auto& av : availableVoices)
                    if (av.accentIndex == kv.idx) { already = true; break; }
                if (!already)
                    availableVoices.push_back({kv.display, kv.espeakId, kv.idx});
                break;
            }
        }
    }
#endif

    // Fallback: always provide at least one voice option
    if (availableVoices.empty()) {
#if defined(__APPLE__)
        availableVoices.push_back({"American (Samantha)", "Samantha", 0});
#else
        availableVoices.push_back({"American (en)", "en", 0});
#endif
    }

    // Build dropdown options and find saved voice index
    std::vector<std::string> voiceOpts;
    int voiceDropSel = 0;
    for (int i = 0; i < (int)availableVoices.size(); ++i) {
        voiceOpts.push_back(availableVoices[i].displayName);
        if (availableVoices[i].accentIndex == g_settings.accentIndex)
            voiceDropSel = i;
    }

    // ── Initialize UI widgets ──────────────────────────────────────────────
    PillDropdown voiceDrop;
    voiceDrop.init(ctlCX, voiceY, ctlW, pillH, voiceOpts, voiceDropSel);

    PillDropdown micDrop;
    micDrop.init(ctlCX, micY, ctlW, pillH, micNames, micSel);

    Configuration cfg;
    cfg.load();

    // Token fields start empty with placeholders
    PillTextField clientIdField;
    clientIdField.init(tokenFieldCX, token1Y, tokenFieldW, pillH,
                       "", "Token Example");

    PillTextField clientSecretField;
    clientSecretField.init(tokenFieldCX, token2Y, tokenFieldW, pillH,
                           "", "Token Example");

    PillTextField refreshTokenField;
    refreshTokenField.init(tokenFieldCX, token3Y, tokenFieldW, pillH,
                           "", "Token Example");

    PillTextField youtubeApiKeyField;
    youtubeApiKeyField.init(tokenFieldCX, token4Y, tokenFieldW, pillH,
                            "", "Token Example");

    const float sliderStartX = ctlCX - ctlW/2.f + 55.f;
    const float sliderW      = ctlW - 55.f;
    Slider volSlider;
    volSlider.init(sliderStartX, volumeY, sliderW, g_settings.volume);

    CircleButton backBtn;
    backBtn.cx = W * 0.072f;
    backBtn.cy = H * 0.055f;
    backBtn.r  = H * 0.032f;

    int workingColorIndex = g_settings.colorIndex;

    // Store original values to detect unsaved changes
    const int savedVolume = g_settings.volume;
    const int savedAccent = g_settings.accentIndex;
    const int savedColor  = g_settings.colorIndex;
    const int savedMic    = g_settings.micIndex;

    bool showUnsavedModal = false;
    bool result           = false;

    std::string tokenStatus[4];
    sf::Clock   tokenStatusClock[4];

#ifdef __APPLE__
    const std::string pasteHint = "cmd+V to paste";
#else
    const std::string pasteHint = "Ctrl+V to paste";
#endif

    // ── Unsaved changes modal geometry ─────────────────────────────────────
    const float mW        = std::min(W * 0.62f, 480.f);
    const float mH        = std::min(H * 0.50f, 400.f);
    const float mCX       = W / 2.f;
    const float mCY       = H / 2.f;
    const float mX        = mCX - mW/2.f;
    const float mY        = mCY - mH/2.f;
    const float mPillH    = 44.f;
    const float mCancelW  = mW * 0.40f;
    const float mCancelCX = mX + mCancelW/2.f + mW*0.055f;
    const float mCancelCY = mY + mPillH/2.f + mH*0.07f;
    const float mMsgY     = mY + mH * 0.35f;
    const float mBtnY     = mY + mH * 0.80f;
    const float mSEW      = mW * 0.44f;
    const float mExW      = mW * 0.30f;
    const float mSEX      = mX + mW*0.07f + mSEW/2.f;
    const float mExX      = mX + mW - mW*0.07f - mExW/2.f;

    // Color circle geometry
    const float cR      = 26.f;
    const float cGap    = cR * 2.8f;
    const float totalCW = (UI_COLOUR_COUNT-1) * cGap;
    const float cStartX = ctlCX - totalCW/2.f;

    // Check if any settings have been modified
    auto hasUnsavedChanges = [&]() {
        // token fields start empty — only count as a change if the user typed something
        bool tokenChanged = (!clientIdField.text.empty()       && clientIdField.text       != cfg.apiSettings.googleClientId)     ||
                            (!clientSecretField.text.empty()   && clientSecretField.text   != cfg.apiSettings.googleClientSecret) ||
                            (!refreshTokenField.text.empty()   && refreshTokenField.text   != cfg.apiSettings.googleRefreshToken) ||
                            (!youtubeApiKeyField.text.empty()  && youtubeApiKeyField.text  != cfg.apiSettings.youtubeApiKey);
        return volSlider.value         != savedVolume ||
               voiceDrop.index        != savedAccent ||
               workingColorIndex       != savedColor  ||
               micIdxs[micDrop.index] != savedMic    ||
               tokenChanged;
    };

    // Save all changes to disk
    auto doSave = [&]() {
        g_settings.volume      = volSlider.value;
        g_settings.accentIndex = availableVoices[voiceDrop.index].accentIndex;
        g_settings.colorIndex  = workingColorIndex;
        g_settings.micIndex    = micIdxs[micDrop.index];
        g_settings.save();
        if (!clientIdField.text.empty())
            cfg.apiSettings.googleClientId = clientIdField.text;
        if (!clientSecretField.text.empty())
            cfg.apiSettings.googleClientSecret = clientSecretField.text;
        if (!refreshTokenField.text.empty())
            cfg.apiSettings.googleRefreshToken = refreshTokenField.text;
        if (!youtubeApiKeyField.text.empty())
            cfg.apiSettings.youtubeApiKey = youtubeApiKeyField.text;
        cfg.save();
        result = true;
    };

    // Draw a token field row with its submit button and status
    auto drawTokenRow = [&](PillTextField& field, int rowIdx,
                            float rowY, sf::Color accentCol,
                            sf::Vector2f mouse) {
        field.draw(window, font, accentCol);

        // Paste hint below field
        drawTextLeft(window, font, pasteHint, (unsigned)(H*0.016f),
                     tokenFieldCX - tokenFieldW/2.f, rowY + pillH*0.72f,
                     sf::Color(160,160,160));

        // SUBMIT button
        bool hov = pillHit(submitCX, rowY, submitW, pillH, mouse);
        drawPill(window, submitCX, rowY, submitW, pillH,
                 hov ? sf::Color(70,70,70) : sf::Color(90,90,90));
        drawPillLabel(window, font, submitCX, rowY, pillH,
                      "SUBMIT", sf::Color(255,255,255), 1.5f);

        // Show "Saved!" message for 2 seconds after submit
        if (!tokenStatus[rowIdx].empty() &&
            tokenStatusClock[rowIdx].getElapsedTime().asSeconds() < 2.f)
            drawTextCenter(window, font, tokenStatus[rowIdx], (unsigned)(H*0.018f),
                           submitCX, rowY + pillH*0.9f, sf::Color(80,80,80));
    };

    // ── Main event loop ────────────────────────────────────────────────────
    while (window.isOpen()) {
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        while (auto ev = window.pollEvent()) {

            // Prompt to save on window close if changes exist
            if (ev->is<sf::Event::Closed>()) {
                if (hasUnsavedChanges() && !showUnsavedModal) { showUnsavedModal=true; continue; }
                window.close();
            }

            if (auto* k = ev->getIf<sf::Event::KeyPressed>()) {
                // Escape also prompts to save
                if (k->code == sf::Keyboard::Key::Escape) {
                    if (hasUnsavedChanges() && !showUnsavedModal) { showUnsavedModal=true; continue; }
                    window.close();
                }
                // Forward keyboard events to text fields
#ifdef __APPLE__
                bool modifier = k->system;  // Cmd on macOS
#else
                bool modifier = k->control;  // Ctrl on Windows/Linux
#endif
                clientIdField.onKeyPressed(k->code, modifier);
                clientSecretField.onKeyPressed(k->code, modifier);
                refreshTokenField.onKeyPressed(k->code, modifier);
                youtubeApiKeyField.onKeyPressed(k->code, modifier);
            }

            if (auto* te = ev->getIf<sf::Event::TextEntered>()) {
                clientIdField.onTextEntered(te->unicode);
                clientSecretField.onTextEntered(te->unicode);
                refreshTokenField.onTextEntered(te->unicode);
                youtubeApiKeyField.onTextEntered(te->unicode);
            }

            if (auto* p = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (p->button == sf::Mouse::Button::Left && !showUnsavedModal) {
                    sf::Vector2f mp = window.mapPixelToCoords(p->position);
                    // Start dragging volume slider
                    if (volSlider.hitArea(mp)) { 
                        volSlider.dragging = true; 
                        volSlider.applyX(mp.x); 
                    }
                }
            }

            if (auto* m = ev->getIf<sf::Event::MouseMoved>()) {
                // Update slider value while dragging
                if (volSlider.dragging)
                    volSlider.applyX(window.mapPixelToCoords(m->position).x);
            }

            if (auto* s = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                if (!showUnsavedModal) {
                    sf::Vector2f mp = window.mapPixelToCoords({s->position.x,s->position.y});
                    voiceDrop.onScroll(mp, s->delta);
                    micDrop.onScroll(mp, s->delta);
                }
            }

            if (auto* r = ev->getIf<sf::Event::MouseButtonReleased>()) {
                if (r->button == sf::Mouse::Button::Left) {
                    volSlider.dragging = false;
                    sf::Vector2f mp = window.mapPixelToCoords(r->position);

                    // Handle unsaved modal interactions
                    if (showUnsavedModal) {
                        if (pillHit(mCancelCX, mCancelCY, mCancelW, mPillH, mp))
                            showUnsavedModal = false;
                        else if (pillHit(mSEX, mBtnY, mSEW, mPillH, mp))
                            { doSave(); window.close(); }
                        else if (pillHit(mExX, mBtnY, mExW, mPillH, mp))
                            window.close();
                        continue;
                    }

                    // Color circle selection
                    for (int i = 0; i < UI_COLOUR_COUNT; ++i) {
                        float cx = cStartX + i*cGap;
                        float dx = mp.x-cx, dy = mp.y-colourY;
                        if (dx*dx+dy*dy <= cR*cR) workingColorIndex=i;
                    }

                    // Dropdown interactions (close other dropdown if one opens)
                    bool vcx = voiceDrop.onRelease(mp);
                    if (vcx && voiceDrop.open) micDrop.open=false;
                    bool mcx = micDrop.onRelease(mp);
                    if (mcx && micDrop.open) voiceDrop.open=false;

                    // Text field focus and double-click select-all
                    auto handleFocus = [&](PillTextField& field) {
                        bool wasHit = field.hit(mp);
                        if (wasHit && !field.focused) field.setFocus(true);
                        else if (!wasHit)             field.setFocus(false);
                        if (wasHit)                   field.onClick();
                    };
                    handleFocus(clientIdField);
                    handleFocus(clientSecretField);
                    handleFocus(refreshTokenField);
                    handleFocus(youtubeApiKeyField);

                    // Submit button for each token field
                    if (pillHit(submitCX, token1Y, submitW, pillH, mp)) {
                        cfg.apiSettings.googleClientId = clientIdField.text;
                        cfg.save(); 
                        tokenStatus[0] = "Saved!"; 
                        tokenStatusClock[0].restart();
                    }
                    if (pillHit(submitCX, token2Y, submitW, pillH, mp)) {
                        cfg.apiSettings.googleClientSecret = clientSecretField.text;
                        cfg.save(); 
                        tokenStatus[1] = "Saved!"; 
                        tokenStatusClock[1].restart();
                    }
                    if (pillHit(submitCX, token3Y, submitW, pillH, mp)) {
                        cfg.apiSettings.googleRefreshToken = refreshTokenField.text;
                        cfg.save(); 
                        tokenStatus[2] = "Saved!"; 
                        tokenStatusClock[2].restart();
                    }
                    if (pillHit(submitCX, token4Y, submitW, pillH, mp)) {
                        cfg.apiSettings.youtubeApiKey = youtubeApiKeyField.text;
                        cfg.save(); tokenStatus[3] = "Saved!"; tokenStatusClock[3].restart();
                    }

                    // Back button
                    if (backBtn.hit(mp)) {
                        if (hasUnsavedChanges()) { showUnsavedModal=true; continue; }
                        window.close();
                    }

                    // Save button
                    if (pillHit(W/2.f, saveY, W*0.28f, pillH*1.05f, mp))
                        { doSave(); window.close(); }
                }
            }
        }

        sf::Color accentCol(
            UI_COLOUR_PRESETS[workingColorIndex].r,
            UI_COLOUR_PRESETS[workingColorIndex].g,
            UI_COLOUR_PRESETS[workingColorIndex].b);

        // ── Rendering ──────────────────────────────────────────────────────
        window.clear(sf::Color(255,255,255));

        drawTextCenter(window, font, "Settings", (unsigned)(H*0.085f),
                       W/2.f, titleY, sf::Color(10,10,10), sf::Text::Bold);
        drawTextLeft(window, font, "CUSTOMIZE YOUR EXPERIENCE", (unsigned)(H*0.022f),
                     lblX, subtitleY, sf::Color(30,30,30), sf::Text::Regular, 1.6f);

        drawTextLeft(window, font, "ASSISTANT PERSONALIZATION", (unsigned)(H*0.018f),
                     lblX, sec1Y, sf::Color(150,150,150), sf::Text::Italic, 2.2f);
        drawTextLeft(window, font, "CONFIGURATION", (unsigned)(H*0.018f),
                     lblX, sec2Y, sf::Color(150,150,150), sf::Text::Italic, 2.2f);

        drawTextLeft(window, font, "Assistant Colour:",   (unsigned)(H*0.026f), lblX, colourY, sf::Color(10,10,10), sf::Text::Bold);
        drawTextLeft(window, font, "Assistant Voice:",    (unsigned)(H*0.026f), lblX, voiceY,  sf::Color(10,10,10), sf::Text::Bold);
        drawTextLeft(window, font, "Microphone:",         (unsigned)(H*0.026f), lblX, micY,    sf::Color(10,10,10), sf::Text::Bold);
        drawTextLeft(window, font, "googleClientId:",     (unsigned)(H*0.026f), lblX, token1Y, sf::Color(10,10,10), sf::Text::Bold);
        drawTextLeft(window, font, "googleClientSecret:", (unsigned)(H*0.026f), lblX, token2Y, sf::Color(10,10,10), sf::Text::Bold);
        drawTextLeft(window, font, "googleRefreshToken:", (unsigned)(H*0.026f), lblX, token3Y, sf::Color(10,10,10), sf::Text::Bold);
        drawTextLeft(window, font, "youtubeApiKey:",      (unsigned)(H*0.026f), lblX, token4Y, sf::Color(10,10,10), sf::Text::Bold);

        // Question mark help icons next to token labels
        int hoveredToken = -1;
        if (hasQuestionImg) {
            float tokenYs[4] = {token1Y, token2Y, token3Y, token4Y};
            const char* labels[] = {"googleClientId:", "googleClientSecret:", "googleRefreshToken:", "youtubeApiKey:"};

            for (int i = 0; i < 4; ++i) {
                sf::Text probe(font, labels[i], (unsigned)(H*0.026f));
                auto lb = probe.getLocalBounds();
                float qX = lblX + lb.position.x + lb.size.x + 20.f;
                float qY = tokenYs[i] - qSz / 2.f;

                sf::Sprite qs(questionTex);
                auto ts = questionTex.getSize();
                qs.setScale({qSz / ts.x, qSz / ts.y});
                qs.setPosition({qX, qY});
                window.draw(qs);

                sf::FloatRect qRect{{qX, qY}, {qSz, qSz}};
                if (qRect.contains(mouse)) hoveredToken = i;
            }
        }
        drawTextLeft(window, font, "Volume:",             (unsigned)(H*0.026f), lblX, volumeY, sf::Color(10,10,10), sf::Text::Bold);

        // Draw color circles with outline on selected color
        for (int i = 0; i < UI_COLOUR_COUNT; ++i) {
            float cx = cStartX + i*cGap;
            sf::CircleShape c(cR);
            c.setOrigin({cR,cR}); 
            c.setPosition({cx,colourY});
            c.setFillColor(sf::Color(
                UI_COLOUR_PRESETS[i].r,
                UI_COLOUR_PRESETS[i].g,
                UI_COLOUR_PRESETS[i].b));
            if (i==workingColorIndex) {
                c.setOutlineThickness(3.5f);
                c.setOutlineColor(sf::Color(30,30,30));
            }
            window.draw(c);
        }

        voiceDrop.drawHeader(window, font);
        micDrop.drawHeader(window, font);

        drawTokenRow(clientIdField,       0, token1Y, accentCol, mouse);
        drawTokenRow(clientSecretField,   1, token2Y, accentCol, mouse);
        drawTokenRow(refreshTokenField,   2, token3Y, accentCol, mouse);
        drawTokenRow(youtubeApiKeyField,  3, token4Y, accentCol, mouse);

        drawTextCenter(window, font, std::to_string(volSlider.value) + "%",
                       (unsigned)(H*0.022f),
                       sliderStartX - 30.f, volumeY, sf::Color(70,70,70));
        volSlider.draw(window);

        // Save button with hover effect
        {
            bool hov = pillHit(W/2.f, saveY, W*0.28f, pillH*1.05f, mouse);
            auto pill = makePill(W/2.f, saveY, W*0.28f, pillH*1.05f);
            pill.setFillColor(hov ? sf::Color(210,210,208) : sf::Color(225,225,223));
            pill.setOutlineThickness(1.5f);
            pill.setOutlineColor(sf::Color(160,160,160));
            window.draw(pill);
            drawPillLabel(window, font, W/2.f, saveY, pillH*1.05f,
                          "SAVE", sf::Color(40,40,40), 1.6f);
        }

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

        voiceDrop.drawList(window, font);
        micDrop.drawList(window, font);

        // ── Unsaved changes modal overlay ──────────────────────────────────
        if (showUnsavedModal) {
            // Semi-transparent black overlay
            sf::RectangleShape overlay({W,H});
            overlay.setPosition({0.f,0.f});
            overlay.setFillColor(sf::Color(0,0,0,90));
            window.draw(overlay);

            // White modal card
            sf::RectangleShape card({mW,mH});
            card.setPosition({mX,mY});
            card.setFillColor(sf::Color(255,255,255));
            window.draw(card);

            // Cancel button
            {
                bool hov = pillHit(mCancelCX, mCancelCY, mCancelW, mPillH, mouse);
                drawPill(window, mCancelCX, mCancelCY, mCancelW, mPillH,
                         hov ? sf::Color(210,210,208) : sf::Color(225,225,223),
                         1.f, sf::Color(170,170,170));
                drawPillLabel(window, font, mCancelCX, mCancelCY, mPillH,
                              "X   CANCEL", sf::Color(50,50,50), 1.4f);
            }

            // Multi-line warning message
            {
                sf::Text msg(font, "YOU HAVE\nUNSAVED CHANGES", (unsigned)(mH*0.106f));
                msg.setFillColor(sf::Color(15,15,15));
                msg.setStyle(sf::Text::Bold);
                msg.setLineSpacing(1.1f);
                auto b = msg.getLocalBounds();
                msg.setOrigin({b.position.x, b.position.y});
                msg.setPosition({mX + mW*0.08f, mMsgY});
                window.draw(msg);
            }

            // Save & Exit button
            {
                bool hov = pillHit(mSEX, mBtnY, mSEW, mPillH, mouse);
                drawPill(window, mSEX, mBtnY, mSEW, mPillH,
                         hov ? sf::Color(210,210,208) : sf::Color(225,225,223),
                         1.f, sf::Color(170,170,170));
                drawPillLabel(window, font, mSEX, mBtnY, mPillH,
                              "SAVE & EXIT", sf::Color(50,50,50), 1.3f);
            }

            // Exit without saving button
            {
                bool hov = pillHit(mExX, mBtnY, mExW, mPillH, mouse);
                drawPill(window, mExX, mBtnY, mExW, mPillH,
                         hov ? sf::Color(80,80,80) : sf::Color(100,100,100));
                drawPillLabel(window, font, mExX, mBtnY, mPillH,
                              "EXIT", sf::Color(255,255,255), 1.5f);
            }
        }

        // Tooltip for question mark icons (drawn last to appear on top)
        if (hoveredToken >= 0 && hasQuestionImg) {
            std::string tip = tooltips[hoveredToken];
            sf::Text tipText(font, tip, (unsigned)(H * 0.020f));
            tipText.setFillColor(sf::Color(255, 255, 255));

            auto tb = tipText.getLocalBounds();
            float tipW = tb.size.x + 16.f;
            float tipH = tb.size.y + 12.f;
            float tipX = mouse.x + 10.f;
            float tipY = mouse.y - tipH - 4.f;

            // Keep tooltip on screen
            if (tipX + tipW > W) tipX = W - tipW - 4.f;
            if (tipY < 0.f) tipY = mouse.y + 14.f;

            sf::RectangleShape tipBox({tipW, tipH});
            tipBox.setPosition({tipX, tipY});
            tipBox.setFillColor(sf::Color(40, 40, 40, 220));
            window.draw(tipBox);

            tipText.setOrigin({tb.position.x, tb.position.y});
            tipText.setPosition({tipX + 8.f, tipY + 6.f});
            window.draw(tipText);
        }

        window.display();
    }

    return result;
}