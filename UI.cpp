#include "UI.h"
#include "AudioManager.h"
#include "CameraThread.h"
#include "Settings.h"
#include "SystemCommandAction.h"
#include "TimeAction.h"
#include "TimerAction.h"
#include "VoiceCommandListener.h"
#include "History.h"
#include "QuickReference.h"

#include <SFML/Graphics.hpp>
#include <mutex>
#include <cmath>
#include <vector>
#include <optional>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <sstream>

#ifdef __APPLE__
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

// ── Globals ───────────────────────────────────────────────────────────────────
SharedResponse sharedResponse;
std::atomic<bool> g_neuraSpeak(false);
std::atomic<int>  g_gestureState(0);
std::atomic<bool> g_cancelSpeech(false);
std::atomic<bool> g_assistantPaused(false);
extern std::string g_normalizedText;
extern std::mutex g_normalizedMtx;

// ── Cancel speech ─────────────────────────────────────────────────────────────

void cancelSpeech() {
    g_cancelSpeech = true;
    g_neuraSpeak   = false;
    g_micMuted     = false;

    // run kills in background so UI thread doesn't freeze
    std::thread([]() {
#ifdef __APPLE__
        std::system("pkill -x afplay 2>/dev/null");
        std::system("pkill -x say 2>/dev/null");
#elif defined(_WIN32)
        std::system("taskkill /F /IM powershell.exe /T 2>nul");
#else
        std::system("pkill -x espeak 2>/dev/null");
        std::system("pkill -x espeak-ng 2>/dev/null");
#endif
    }).detach();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string escapeForDoubleQuotedShell(const std::string& text) {
    std::string safe;
    safe.reserve(text.size());
    for (char c : text) {
        if (c == '"') safe += '\'';
        else safe += c;
    }
    return safe;
}

static std::string escapeForPowerShellSingleQuoted(const std::string& text) {
    std::string safe;
    safe.reserve(text.size());
    for (char c : text) {
        if (c == '\'') safe += "''";
        else if (c == '"') safe += ' ';
        else safe += c;
    }
    return safe;
}

static bool g_showTutorial = false;
static int g_tutorialStep = 0;

void startTutorial() {
    g_showTutorial = true;
    g_tutorialStep = 0;
}

static std::vector<std::string> tutorialSteps = {
    "Welcome to Neura\nYour gesture & voice assistant",

    "LET'S START WITH GESTURES\n\n"
    "To move your cursor, point with one finger and move your hand.\n\n"
    "Try it now.",

    "DRAGGING & ACTIONS\n\n"
    "To drag items, use two fingers and move your hand.\n"
    "To cancel an action, show an open palm.\n\n"
    "Give it a quick try.",

    "QUICK RESPONSES\n\n"
    "Thumbs up means yes.\n"
    "Thumbs down means no.\n\n"
    "You'll use these for quick confirmations.",

    "NOW TRY A VOICE COMMAND\n\n"
    "Say \"Take a screenshot\" out loud.\n\n"
    "Go ahead and try it. It should save to your default screen shot location",

    "VOICE CONTROLS\n\n"
    "You can say things like \"Increase volume\", \"Set timer for 10 seconds\", or \"Close window\".\n\n"
    "Just speak naturally.",

    "ASK NEURA\n\n"
    "Try saying \"What is the weather in London\" or \"Play song Blinding Lights\".\n",

    "TEXT & CONTROL COMMANDS\n\n"
    "You can say \"Read screen\", \"Copy\", \"Paste\", \"Pause\", or \"Resume\" while working.\n",

    "NEED MORE?\n\n"
    "Click the [?] button anytime to see the full list of commands and features.",

    "You're ready!\nClick anywhere to start using Neura."
};

// ── speakText ─────────────────────────────────────────────────────────────────
/**
 * @brief Converts text to speech asynchronously.
 * 
 * This function uses platform-specific TTS engines to vocalize text input.
 * It runs in a detached thread to avoid blocking the UI.
 * 
 * The function also manages:
 * - Microphone muting during playback
 * - Cancellation handling via g_cancelSpeech
 * - Volume control (on Windows)
 * 
 * @param text The text string to be spoken.
 * @return void
 * 
 * @author Erina
 */
static void speakText(const std::string& text) {
    g_neuraSpeak   = true;
    g_micMuted     = true;
    g_cancelSpeech = false;

#if defined(__APPLE__)
    std::string safe = text;
    for (char& c : safe) {
        if (c == '"') c = '\'';
    }
    std::thread([safe]() {
        // Get voice from settings
        static const char* voices[] = {"Samantha", "Daniel", "Karen", "Samantha"};
        int idx = std::clamp(g_settings.accentIndex, 0, 3);
        std::string voice = voices[idx];
        
        // generate aiff first, then play with afplay (which is killable)
        std::string tmp = "/tmp/neura_tts.aiff";
        std::string gen = "/usr/bin/say -v \"" + voice + "\" -o \"" + tmp + "\" \"" + safe + "\"";
        std::system(gen.c_str());

        if (!g_cancelSpeech.load()) {
            float vol = std::clamp(g_settings.volume, 0, 100) / 100.f;
            std::string playCmd = "/usr/bin/afplay -v " + std::to_string(vol) + " \"" + tmp + "\" &";
            std::system(playCmd.c_str());

            // small sleep so afplay has time to start before we poll
            std::this_thread::sleep_for(std::chrono::milliseconds(300));

            // poll until afplay finishes or we get cancelled
            while (!g_cancelSpeech.load()) {
                FILE* check = popen("pgrep -x afplay", "r");
                if (check) {
                    char buf[32] = {};
                    bool running = fgets(buf, sizeof(buf), check) != nullptr;
                    pclose(check);
                    if (!running) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (g_cancelSpeech.load()) {
                std::system("pkill -x afplay 2>/dev/null");
            }
        }

        g_neuraSpeak   = false;
        g_micMuted     = false;
        g_cancelSpeech = false;
    }).detach();

#elif defined(_WIN32)
    std::thread([text]() {
        int neuraVol = std::clamp(g_settings.volume, 0, 100);
        std::string safe = escapeForPowerShellSingleQuoted(text);
        if (g_cancelSpeech.load()) { g_neuraSpeak = false; g_micMuted = false; return; }
        
        // Generate unique marker file to track completion
        std::string marker = "C:\\Windows\\Temp\\neura_tts_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".txt";
        
        std::string cmd =
            "powershell -NoProfile -Command \""
            "Add-Type -AssemblyName System.Speech; "
            "$speak = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
            "$speak.Volume = " + std::to_string(neuraVol) + "; "
            "$speak.Speak('" + safe + "'); "
            "Out-File -FilePath '" + marker + "'\"";
        
        // Start TTS in background
        std::thread([cmd, marker]() {
            std::system(cmd.c_str());
        }).detach();
        
        // Poll for completion marker file
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        while (!g_cancelSpeech.load()) {
            std::ifstream check(marker);
            if (check.good()) {
                check.close();
                DeleteFileA(marker.c_str());
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (g_cancelSpeech.load()) {
            std::system("taskkill /F /IM powershell.exe /T 2>nul");
        }
        
        g_neuraSpeak   = false;
        g_micMuted     = false;
        g_cancelSpeech = false;
    }).detach();

#else
    std::string safe = text;
    for (char& c : safe) {
        if (c == '"') c = '\'';
    }
    std::string voice = "en-us";
    
    std::thread([safe, voice]() {
        if (g_cancelSpeech.load()) { g_neuraSpeak = false; g_micMuted = false; return; }
        int amp = std::clamp(g_settings.volume * 2, 0, 200);
        std::system(("/usr/bin/espeak -v " + voice + " -a " + std::to_string(amp) + " \"" + safe + "\" &").c_str());

        // Small delay for espeak to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Poll until espeak finishes or we get cancelled
        while (!g_cancelSpeech.load()) {
            FILE* check = popen("pgrep -x espeak", "r");
            if (!check) break;
            char buf[32] = {};
            bool running = fgets(buf, sizeof(buf), check) != nullptr;
            pclose(check);
            if (!running) {
                check = popen("pgrep -x espeak-ng", "r");
                if (check) {
                    running = fgets(buf, sizeof(buf), check) != nullptr;
                    pclose(check);
                }
                if (!running) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (g_cancelSpeech.load()) {
            std::system("pkill -x espeak 2>/dev/null");
            std::system("pkill -x espeak-ng 2>/dev/null");
        }

        g_neuraSpeak   = false;
        g_micMuted     = false;
        g_cancelSpeech = false;
    }).detach();
#endif
}

static bool loadUIFont(sf::Font& font) {
#ifdef __APPLE__
    static const char* candidates[] = {
        "/System/Library/Fonts/Avenir.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
#elif defined(_WIN32)
    static const char* candidates[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\calibri.ttf"
    };
#else
    static const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"
    };
#endif
    for (const char* path : candidates) {
        if (font.openFromFile(path)) return true;
    }
    return false;
}

static std::string wrapText(const std::string& text, const sf::Font& font, unsigned charSize, float maxWidth) {
    std::string wrapped;
    std::string currentLine;
    std::istringstream words(text);
    std::string word;

    while (words >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        sf::Text test(font, testLine, charSize);
        
        if (test.getLocalBounds().size.x > maxWidth && !currentLine.empty()) {
            wrapped += currentLine + "\n";
            currentLine = word;
        } else {
            currentLine = testLine;
        }
    }
    
    if (!currentLine.empty()) {
        wrapped += currentLine;
    }
    
    return wrapped;
}

static std::string truncateText(const std::string& text, const sf::Font& font, unsigned charSize, float maxWidth) {
    sf::Text test(font, text, charSize);
    
    // If text fits, return as-is
    if (test.getLocalBounds().size.x <= maxWidth) {
        return text;
    }
    
    // Binary search for the right truncation point
    std::string truncated = text;
    while (!truncated.empty()) {
        sf::Text ellipsisTest(font, truncated + "...", charSize);
        if (ellipsisTest.getLocalBounds().size.x <= maxWidth) {
            return truncated + "...";
        }
        // Remove one character at a time from the end
        truncated.pop_back();
    }
    
    return "...";
}

bool runUI() {
    g_settings.load();

    AudioManager audio;
    audio.init(g_settings.micIndex);

    VoiceCommandListener voiceListener("ggml-base.en.bin", g_audioBuf, g_micMuted, sharedResponse);
    voiceListener.start();

    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    unsigned windowW = (unsigned)(desktop.size.x * 0.75f);
    unsigned windowH = (unsigned)(desktop.size.y * 0.75f);

    sf::RenderWindow window(
        sf::VideoMode({windowW, windowH}),
        "Neura",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);
    window.setPosition({
        static_cast<int>((desktop.size.x - windowW) / 2),
        static_cast<int>((desktop.size.y - windowH) / 2)
    });

#ifdef __APPLE__
    id nsWindow = (id)window.getNativeHandle();
    ((void (*)(id, SEL, long))objc_msgSend)(
        nsWindow, sel_registerName("setLevel:"), 3L);
    ((void (*)(id, SEL, unsigned long))objc_msgSend)(
        nsWindow, sel_registerName("setCollectionBehavior:"), 1UL << 0);
#elif defined(_WIN32)
    HWND hwnd = static_cast<HWND>(window.getNativeHandle());
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif

    float headerH = windowH * 0.13f;
    float footerH = windowH * 0.045f;
    float camW    = windowW * 0.75f;
    float camH    = windowH - headerH - footerH;
    float camY    = headerH;
    float rightX  = camW;
    float rightW  = windowW - camW;
    float rightCX = rightX + rightW / 2.f;

    CameraThread camera;
    camera.start((unsigned)camW, (unsigned)camH);

    sf::Texture camTexture;
    [[maybe_unused]] auto resizeResult = camTexture.resize({(unsigned)camW, (unsigned)camH});
    sf::Sprite camSprite(camTexture);
    camSprite.setPosition({0.f, camY});

    sf::Texture backTex, historyTex, questionTex;
    bool hasBack     = backTex.loadFromFile("images/back_arrow.png");
    bool hasHistory  = historyTex.loadFromFile("images/history.png");
    bool hasQuestion = questionTex.loadFromFile("images/question_button.png");

    float btnSz  = headerH * 0.48f;
    float btnY   = (headerH - btnSz) / 2.f;
    float backX  = windowW * 0.030f;
    float questX = windowW - backX - btnSz;
    float histX  = questX - btnSz - backX;

    auto makeSprite = [&](sf::Texture& tex) {
        sf::Sprite s(tex);
        auto ts = tex.getSize();
        s.setScale({btnSz / ts.x, btnSz / ts.y});
        return s;
    };

    int ci = std::min(g_settings.colorIndex, UI_COLOUR_COUNT - 1);
    sf::Color accentColor(
        UI_COLOUR_PRESETS[ci].r,
        UI_COLOUR_PRESETS[ci].g,
        UI_COLOUR_PRESETS[ci].b);

    const sf::Color COLOR_IDLE   = accentColor;
    const sf::Color COLOR_OUTPUT = sf::Color(40, 40, 40);

    sf::Font font;
    #if defined(__APPLE__)
        bool fontLoaded = font.openFromFile("/System/Library/Fonts/Avenir.ttc");
    #elif defined(_WIN32)
        bool fontLoaded = font.openFromFile("C:\\Windows\\Fonts\\seguisym.ttf");
    #else
        bool fontLoaded = font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    #endif

    std::optional<sf::Text> titleText;
    if (fontLoaded) {
        titleText.emplace(font, "Neura Assistant", (unsigned)(headerH * 0.30f));
        titleText->setFillColor(sf::Color(20, 20, 20));
        auto b = titleText->getLocalBounds();
        titleText->setOrigin({b.position.x + b.size.x / 2.f,
                              b.position.y + b.size.y / 2.f});
        titleText->setPosition({rightCX, camY + camH * 0.18f});
    }

    float voiceCX   = rightCX;
    float voiceCY   = camY + camH * 0.54f;
    float maxRadius = rightW * 0.40f;
    float labelY    = camY + camH * 0.78f;

    sf::CircleShape circle(maxRadius * 0.5f);
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineThickness(3.f);
    circle.setOutlineColor(COLOR_IDLE);
    float initR = maxRadius * 0.5f;
    circle.setOrigin({initR, initR});
    circle.setPosition({voiceCX, voiceCY});

    std::optional<sf::Text> instrText;
    if (fontLoaded) {
        instrText.emplace(font,
            "T=time  B=tabs  W=windows\nR=read  S=rec start  X=stop", 15);
        instrText->setFillColor(sf::Color(100, 100, 100));
        instrText->setLineSpacing(1.3f);
        auto b = instrText->getLocalBounds();
        instrText->setOrigin({b.position.x + b.size.x / 2.f, b.position.y});
        instrText->setPosition({rightCX, camY + camH * 0.86f});
    }

    std::string currentResponse;
    std::optional<sf::Text> responseText;

    std::string currentNormalized;
    std::optional<sf::Text> normalizedText;
    if (fontLoaded) {
        normalizedText.emplace(font, currentNormalized, 12);
        normalizedText->setFillColor(accentColor);
        auto b = normalizedText->getLocalBounds();
        normalizedText->setOrigin({b.position.x, b.position.y});
        normalizedText->setPosition({10.f, camY + camH + 5.f});
    }

    if (fontLoaded) {
        responseText.emplace(font, currentResponse, 13);
        responseText->setFillColor(accentColor);        
        auto b = responseText->getLocalBounds();
        responseText->setOrigin({b.position.x + b.size.x / 2.f, b.position.y});
        responseText->setPosition({rightCX, camY + camH * 0.74f});
    }

    std::vector<sf::CircleShape> waves;
    float smoothed    = 0.05f;
    float noiseFloor  = 0.08f;
    bool  backClicked = false;

    SystemCommandAction sysAction;

    // ── main loop ─────────────────────────────────────────────────────────
    while (window.isOpen()) {
        sf::Vector2f mousePos = window.mapPixelToCoords(
            sf::Mouse::getPosition(window));

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (g_showTutorial) {
                if (auto click = event->getIf<sf::Event::MouseButtonReleased>()) {
                    if (click->button == sf::Mouse::Button::Left) {
                        g_tutorialStep++;
                        if (g_tutorialStep >= (int)tutorialSteps.size())
                            g_showTutorial = false;
                    }
                }
            }

            if (auto key = event->getIf<sf::Event::KeyPressed>()) {
                switch (key->code) {
                case sf::Keyboard::Key::Escape: window.close(); break;
                case sf::Keyboard::Key::T: {
                    TimeAction t;
                    currentResponse = "Current time is " + t.getCurrentTimeString();
                    speakText(currentResponse);
                    break;
                }
                case sf::Keyboard::Key::B:
                    currentResponse = sysAction.switchTabs()
                        ? "Tabs switched." : "Tab switch unavailable.";
                    speakText(currentResponse); break;
                case sf::Keyboard::Key::W:
                    currentResponse = sysAction.switchWindows()
                        ? "Windows switched." : "Window switch unavailable.";
                    speakText(currentResponse); break;
                case sf::Keyboard::Key::R: {
                    std::string sel = sysAction.readSelectedText();
                    currentResponse = sel.empty() ? "Nothing selected." : "Read: " + sel;
                    speakText(currentResponse); break;
                }
                default: break;
                }
                if (responseText) {
                    responseText->setString(wrapText(currentResponse, font, 13, rightW * 0.85f));
                    auto b = responseText->getLocalBounds();
                    responseText->setOrigin({b.position.x + b.size.x / 2.f, b.position.y});
                    responseText->setPosition({rightCX, camY + camH * 0.74f});
                }
            }

            if (auto press = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (press->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mp = window.mapPixelToCoords(press->position);
                }
            }

            if (auto click = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (click->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mp = window.mapPixelToCoords(click->position);

                    sf::FloatRect backRect{{backX, btnY}, {btnSz, btnSz}};
                    if (backRect.contains(mp)) {
                        g_showTutorial = false;
                        g_tutorialStep = 0;
                        backClicked = true;
                        window.close();
                    }

                    sf::FloatRect histRect{{histX, btnY}, {btnSz, btnSz}};
                    if (histRect.contains(mp)) {
                        window.setVisible(false);
                        runHistoryPage();
                        window.setVisible(true);
                    }

                    sf::FloatRect questRect{{questX, btnY}, {btnSz, btnSz}};
                    if (questRect.contains(mp)) {
                        window.setVisible(false);
                        runQuickReference();
                        window.setVisible(true);
                    }
        
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(sharedResponse.mtx);
            if (sharedResponse.updated) {
                currentResponse = sharedResponse.text;
                sharedResponse.updated = false;
                // respond() in VoiceCommandListener already handles speaking
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(g_normalizedMtx);
            
            // Check if gesture is active (any gesture state > 0 means hand detected)
            int curGestureState = g_gestureState.load();
            
            if (curGestureState > 0 && normalizedText) {
                // Hand detected - show gesture detection message
                std::string displayText = "Gesture detected";
                normalizedText->setString(displayText);
                auto b = normalizedText->getLocalBounds();
                normalizedText->setOrigin({b.position.x, b.position.y});
                normalizedText->setPosition({10.f, camY + camH + 5.f});
            } else if (!g_normalizedText.empty() && normalizedText) {
                // No hand detected - show voice transcription
                std::string displayText = "Neura heard: " + g_normalizedText;
                std::string truncated = truncateText(displayText, font, 12, camW - 20.f);
                normalizedText->setString(truncated);
                auto b = normalizedText->getLocalBounds();
                normalizedText->setOrigin({b.position.x, b.position.y});
                normalizedText->setPosition({10.f, camY + camH + 5.f});
            } else if (normalizedText && curGestureState == 0) {
                // No gesture and no text - show waiting message
                normalizedText->setString("");
            }
        }

        // open palm cancels speech and clears response (fires once on transition)
        static int lastGestureState = 0;
        int curGestureState = g_gestureState.load();
        if (curGestureState == 3 && lastGestureState != 3) {
            cancelSpeech();
            currentResponse = "";
        }
        lastGestureState = curGestureState;

        // track if Neura was speaking, and clear response when it stops
        static bool wasSpeaking = false;
        bool isSpeaking = g_neuraSpeak.load();
        if (wasSpeaking && !isSpeaking) {
            // Just stopped speaking - clear the response on all platforms
            currentResponse = "";
        }
        wasSpeaking = isSpeaking;

        if (responseText && !currentResponse.empty()) {
            responseText->setString(wrapText(currentResponse, font, 13, rightW * 0.85f));
            auto b = responseText->getLocalBounds();
            responseText->setOrigin({b.position.x + b.size.x / 2.f, b.position.y});
            responseText->setPosition({rightCX, camY + camH * 0.74f});
        } else if (responseText && currentResponse.empty() && !g_neuraSpeak.load() && g_gestureState.load() == 0) {
            // Check if assistant is paused FIRST before setting any default text
            if (g_assistantPaused.load()) {
                responseText->setString("Paused. Say resume or unmute to continue");
            } else {
                responseText->setString("Listening...");
            }
            auto b = responseText->getLocalBounds();
            responseText->setOrigin({b.position.x + b.size.x / 2.f, b.position.y});
            responseText->setPosition({rightCX, camY + camH * 0.74f});
        }

        // ── audio / wave update ───────────────────────────────────────────
        float volume = g_volume.load();
        noiseFloor = noiseFloor * 0.992f + volume * 0.008f;
        volume -= noiseFloor;
        if (volume < 0.09f) volume = 0.f;
        volume   = std::pow(volume, 0.6f);
        smoothed = smoothed * 0.97f + volume * 0.03f;
        smoothed = std::min(smoothed, 1.0f);

        sf::Color currentColor = g_neuraSpeak.load() ? COLOR_OUTPUT : COLOR_IDLE;
        circle.setOutlineColor(currentColor);

        float scaledRadius = std::min(maxRadius * 0.5f + smoothed * maxRadius, maxRadius);
        circle.setRadius(scaledRadius);
        circle.setOrigin({scaledRadius, scaledRadius});
        circle.setPosition({voiceCX, voiceCY});

        if (smoothed > 0.03f) {
            sf::CircleShape wave(scaledRadius);
            wave.setFillColor(sf::Color::Transparent);
            wave.setOutlineThickness(2.f);
            wave.setOutlineColor(sf::Color(
                currentColor.r, currentColor.g, currentColor.b, 150));
            wave.setOrigin({scaledRadius, scaledRadius});
            wave.setPosition({voiceCX, voiceCY});
            waves.push_back(wave);
        }

        for (auto& wave : waves) {
            float r = wave.getRadius() + 1.5f;
            wave.setRadius(r);
            wave.setOrigin({r, r});
            sf::Color c = wave.getOutlineColor();
            if (c.a > 3) c.a -= 3;
            wave.setOutlineColor({c.r, c.g, c.b, c.a});
        }
        waves.erase(
            std::remove_if(waves.begin(), waves.end(),
                [](sf::CircleShape& w){ return w.getOutlineColor().a <= 3; }),
            waves.end());

        // ── draw ──────────────────────────────────────────────────────────
        window.clear(sf::Color(255, 255, 255));

        {
            cv::Mat frame;
            if (camera.copyLatest(frame))
                camTexture.update(frame.data);
        }
        window.draw(camSprite);

        sf::RectangleShape divider({1.f, (float)windowH});
        divider.setPosition({camW, 0.f});
        divider.setFillColor(sf::Color(255, 255, 255));
        window.draw(divider);

        window.draw(circle);
        for (auto& wave : waves)
            window.draw(wave);

        if (titleText) window.draw(*titleText);

        if (normalizedText) window.draw(*normalizedText);

        if (fontLoaded && g_timerActive.load()) {
            int total = g_timerRemaining.load();
            int mins = total / 60;
            int secs = total % 60;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);

            sf::Text timerLabel(font, "Timer", 14);
            timerLabel.setFillColor(sf::Color(120, 120, 120));
            auto lb = timerLabel.getLocalBounds();
            timerLabel.setOrigin({lb.position.x + lb.size.x / 2.f,
                                  lb.position.y + lb.size.y / 2.f});
            timerLabel.setPosition({rightCX, camY + camH * 0.27f});
            window.draw(timerLabel);

            sf::Text timerText(font, buf, 26);
            timerText.setFillColor(sf::Color(20, 20, 20));
            auto tb = timerText.getLocalBounds();
            timerText.setOrigin({tb.position.x + tb.size.x / 2.f,
                                 tb.position.y + tb.size.y / 2.f});
            timerText.setPosition({rightCX, camY + camH * 0.32f});
            window.draw(timerText);
        }

        if (responseText) window.draw(*responseText);
        if (instrText)    window.draw(*instrText);

        if (fontLoaded) {
            static const char* gestureLabels[] = {
                "", "Moving", "Selecting", "Cancel", "Yes", "No"
            };
            int gs = g_gestureState.load();
            if (gs > 0 && gs < 6) {
                sf::Text gl(font, gestureLabels[gs], 15);
                gl.setFillColor(sf::Color(40, 40, 40));
                auto lb = gl.getLocalBounds();
                gl.setOrigin({lb.position.x + lb.size.x / 2.f,
                              lb.position.y + lb.size.y / 2.f});
                gl.setPosition({voiceCX, labelY});
                window.draw(gl);
            }
        }

        if (hasBack) {
            auto s = makeSprite(backTex);
            s.setPosition({backX, btnY});
            window.draw(s);
        }
        if (hasHistory) {
            auto s = makeSprite(historyTex);
            s.setPosition({histX, btnY});
            window.draw(s);
        }
        if (hasQuestion) {
            auto s = makeSprite(questionTex);
            s.setPosition({questX, btnY});
            window.draw(s);
        }

        if (g_showTutorial && fontLoaded) {
            sf::RectangleShape overlay({(float)windowW, (float)windowH});
            overlay.setFillColor(sf::Color(0, 0, 0, 150));
            window.draw(overlay);

            sf::Text text(font, tutorialSteps[g_tutorialStep], 24);
            text.setFillColor(sf::Color::White);
            auto b = text.getLocalBounds();
            text.setOrigin({b.position.x + b.size.x / 2.f,
                            b.position.y + b.size.y / 2.f});
            text.setPosition({windowW / 2.f, windowH / 2.f});
            window.draw(text);

            sf::Text hint(font, "Click anywhere to continue", 16);
            hint.setFillColor(sf::Color(200, 200, 200));
            auto hb = hint.getLocalBounds();
            hint.setOrigin({hb.position.x + hb.size.x / 2.f,
                            hb.position.y + hb.size.y / 2.f});
            hint.setPosition({windowW / 2.f, windowH * 0.75f});
            window.draw(hint);
        }

        window.display();
    }

    camera.stop();
    voiceListener.stop();
    audio.stop();

    return backClicked;
}