#include <iostream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <ctime>
#include <map>
#include <cmath>
#include <vector>

#include "VoiceCommandListener.h"
#include "TimeAction.h"
#include "BrowserAction.h"
#include "WeatherAction.h"
#include "SystemAction.h"
#include "UI.h"
#include "APIAction.h"
#include "Settings.h"
#include "ConversationLogger.h"

// ── Inlined response strings ──────────────────────────────────────────────────

/**
 * @namespace responses
 * @brief Stores helper functions for common response messages spoken by Neura.
 *
 * This namespace groups short helper functions that format user-facing
 * response strings for common commands such as time requests, tab switching,
 * weather failures, media control, and recording actions.
 */
namespace responses {

     /** @brief Formats the assistant response for the current time. */
    static std::string formatTimeResponse(const std::string& t)   { return "Current time is " + t; }

    /** @brief Response used when browser tabs are switched successfully. */
    static std::string tabsSwitched()                              { return "Switched to the next browser tab."; }

    /** @brief Response used when no additional browser tabs are available. */
    static std::string tabsUnavailable()                           { return "No additional browser tabs are available."; }

    /** @brief Response used when windows are switched successfully. */
    static std::string windowsSwitched()                           { return "Switched to another open window."; }
    
    /** @brief Response used when window switching is unavailable. */
    static std::string windowsUnavailable()                        { return "No additional windows are available or accessibility permission is restricted."; }
    
    /** @brief Formats the assistant response for reading selected screen text. */
    static std::string screenRead(const std::string& text)        { return "Reading selected text: " + text; }
    
    /** @brief Response used when no readable selected text is available. */
    static std::string screenReadUnavailable()                     { return "No readable text detected on the screen. Try selecting text first."; }
    
    /** @brief Response used when the spoken command is not recognized. */
    static std::string unknownCommand()                            { return "Sorry, command not recognized."; }
    
    /** @brief Response used when weather information cannot be retrieved. */
    static std::string weatherUnavailable() { return "Weather is unavailable right now."; }
    
    /** @brief Response used when media playback starts successfully. */
    static std::string mediaPlayed()        { return "Playing media."; }
    
     /** @brief Response used when media playback is paused successfully. */
    static std::string mediaPaused()        { return "Pausing media."; }
    
    /** @brief Response used when media control is unavailable. */
    static std::string mediaUnavailable()   { return "Media control is unavailable."; }
    
    /** @brief Response used when a song could not be played. */
    static std::string songUnavailable()    { return "I could not play that song."; }
    
}


/**
 * @brief Converts a string to lowercase.
 *
 * @param s The input string.
 * @return A lowercase copy of the input string.
 */
std::string g_normalizedText;
std::mutex g_normalizedMtx;
extern std::atomic<bool> g_assistantPaused;

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}


/**
 * @brief Checks whether one string contains another.
 *
 * @param haystack The string to search in.
 * @param needle The substring to search for.
 * @return true if @p needle is found inside @p haystack, false otherwise.
 */

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}


/**
 * @brief Removes leading and trailing whitespace from a string.
 *
 * @param s The input string.
 * @return A trimmed copy of the input string.
 */

static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}


/**
 * @brief Normalizes a Whisper transcript for easier command matching.
 *
 * Converts the transcript to lowercase, removes surrounding whitespace,
 * filters out punctuation, collapses repeated spaces, and ignores
 * placeholder silence tokens such as `[blank_audio]`.
 *
 * @param s The raw transcript string.
 * @return A cleaned and normalized transcript string.
 */

static std::string normalizeTranscript(std::string s) {
    s = toLower(trim(s));

    if (s == "[blank_audio]" || s == "[ blank_audio ]" ||
        s == "[silence]" || s == "[ silence ]") {
        return "";
    }

    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || std::isspace(c)) out += c;
        else out += ' ';
    }

    std::string collapsed;
    bool prevSpace = false;
    for (unsigned char c : out) {
        bool isSpace = std::isspace(c);
        if (isSpace) {
            if (!prevSpace) collapsed += ' ';
        } else {
            collapsed += c;
        }
        prevSpace = isSpace;
    }

    return trim(collapsed);
}


/**
 * @brief Converts a `std::tm` structure into ISO date format.
 *
 * Produces a date string in the form `YYYY-MM-DD`.
 *
 * @param tmVal The time structure to convert.
 * @return The formatted ISO date string.
 */

static std::string toIsoDate(const std::tm& tmVal) {
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmVal);
    return std::string(buf);
}


/**
 * @brief Parses a spoken time into 24-hour format.
 *
 * Accepts times such as `9`, `9:30`, `9 am`, or `9:30 pm`
 * and converts them into `HH:MM` format.
 *
 * @param rawTime The spoken time string.
 * @param ampm Optional AM/PM suffix.
 * @param outTime24h Output parameter storing the converted 24-hour time.
 * @return true if parsing succeeds, false otherwise.
 */

static bool parseHourMin(const std::string& rawTime,
                         const std::string& ampm,
                         std::string& outTime24h)
{
    int hour = 0, minute = 0;

    std::string timeOnly = rawTime;
    std::string mer = toLower(trim(ampm));

    // Strip fused am/pm suffix (e.g. "5pm", "535pm" after normalization)
    if (mer.empty() && timeOnly.size() >= 2) {
        std::string tail2 = toLower(timeOnly.substr(timeOnly.size() - 2));
        if (tail2 == "am" || tail2 == "pm") {
            mer      = tail2;
            timeOnly = trim(timeOnly.substr(0, timeOnly.size() - 2));
        }
    }

    // Try "HH:MM" or "HH MM" (colon became space after normalization)
    if (timeOnly.find(':') != std::string::npos) {
        if (std::sscanf(timeOnly.c_str(), "%d:%d", &hour, &minute) != 2) return false;
    } else if (timeOnly.find(' ') != std::string::npos) {
        if (std::sscanf(timeOnly.c_str(), "%d %d", &hour, &minute) != 2) return false;
    } else {
        if (std::sscanf(timeOnly.c_str(), "%d", &hour) != 1) return false;
        minute = 0;
    }

    if (mer == "pm" && hour < 12) hour += 12;
    if (mer == "am" && hour == 12) hour = 0;

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    outTime24h = buf;
    return true;
}


/**
 * @brief Parses a spoken calendar command.
 *
 * Supports relative date formats such as `tomorrow at 9 pm`,
 * month-name dates such as `on april 2 at 9 pm`, and
 * ISO dates such as `on 2026-04-02 at 21:00`.
 *
 * @param text The normalized voice command text.
 * @param title Output parameter storing the event title.
 * @param date Output parameter storing the parsed date in `YYYY-MM-DD` format.
 * @param time24h Output parameter storing the parsed time in `HH:MM` format.
 * @return true if the calendar command was parsed successfully, false otherwise.
 */

static bool parseCalendarCommand(const std::string& text,
                                 std::string& title,
                                 std::string& date,
                                 std::string& time24h)
{
    std::smatch m;

    std::regex relPattern(
        R"(add event (.+?) (tomorrow|today) (?:at )?(\d{1,2}(?::?\s*\d{2})?)\s*(am|pm)?\b)"
    );
    if (std::regex_search(text, m, relPattern)) {
        title = trim(m[1].str());
        std::time_t now = std::time(nullptr);
        std::tm localTm = *std::localtime(&now);
        std::string rel = trim(m[2].str());
        if (rel == "tomorrow") {
            localTm.tm_mday += 1;
            std::mktime(&localTm);
        }
        date = toIsoDate(localTm);
        return parseHourMin(m[3].str(), m[4].str(), time24h);
    }

    std::regex monthPattern(
        R"(add event (.+?) (?:on )?([a-z]+)\s+(\d{1,2})(?:st|nd|rd|th)? (?:at )?(\d{1,2}(?::?\s*\d{2})?)\s*(am|pm)?\b)"
    );
    if (std::regex_search(text, m, monthPattern)) {
        title = trim(m[1].str());
        static const std::map<std::string, int> months = {
            {"january",0},{"february",1},{"march",2},{"april",3},
            {"may",4},{"june",5},{"july",6},{"august",7},
            {"september",8},{"october",9},{"november",10},{"december",11}
        };
        auto it = months.find(trim(m[2].str()));
        if (it == months.end()) return false;
        std::time_t now = std::time(nullptr);
        std::tm localTm = *std::localtime(&now);
        localTm.tm_mon  = it->second;
        localTm.tm_mday = std::stoi(m[3].str());
        localTm.tm_hour = 0;
        localTm.tm_min  = 0;
        localTm.tm_sec  = 0;
        std::mktime(&localTm);
        date = toIsoDate(localTm);
        return parseHourMin(m[4].str(), m[5].str(), time24h);
    }

    std::regex isoPattern(
        R"(add event (.+) on (\d{4}-\d{2}-\d{2}) at (\d{2}:\d{2}))"
    );
    if (std::regex_search(text, m, isoPattern)) {
        title   = trim(m[1].str());
        date    = m[2].str();
        time24h = m[3].str();
        return true;
    }

    return false;
}


/**
 * @brief Adds a duration in minutes to a local ISO date-time.
 *
 * Combines the given date and time, adds the requested number of minutes,
 * and returns a new local date-time string in ISO-like form.
 *
 * @param date The starting date in `YYYY-MM-DD` format.
 * @param time24h The starting time in `HH:MM` format.
 * @param minutesToAdd The number of minutes to add.
 * @return The resulting local date-time string in the form `YYYY-MM-DDTHH:MM:00`.
 */

static std::string addMinutesToIsoLocal(const std::string& date,
                                        const std::string& time24h,
                                        int minutesToAdd)
{
    int hour = 0, minute = 0;
    if (std::sscanf(time24h.c_str(), "%d:%d", &hour, &minute) != 2)
        return date + "T" + time24h + ":00";

    int total   = hour * 60 + minute + minutesToAdd;
    int endHour = (total / 60) % 24;
    int endMin  = total % 60;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%sT%02d:%02d:00", date.c_str(), endHour, endMin);
    return std::string(buf);
}

/**
 * @brief Parses a spoken calendar duration into minutes.
 *
 * Supports phrases such as `1 hour`, `2 hours 30 minutes`,
 * or `45 minutes`.
 *
 * @param text The spoken duration text.
 * @return The parsed duration in minutes, or -1 if parsing fails.
 */

static int parseDurationMinutes(const std::string& text) {
    std::smatch m;

    std::regex hourMinPattern(R"((\d+)\s*hour[s]?\s*(\d+)?\s*minute[s]?)");
    if (std::regex_search(text, m, hourMinPattern)) {
        int hours = std::stoi(m[1].str());
        int mins  = m[2].matched ? std::stoi(m[2].str()) : 0;
        return hours * 60 + mins;
    }

    std::regex hourPattern(R"((\d+)\s*hour[s]?)");
    if (std::regex_search(text, m, hourPattern)) {
        return std::stoi(m[1].str()) * 60;
    }

    std::regex minPattern(R"((\d+)\s*minute[s]?)");
    if (std::regex_search(text, m, minPattern)) {
        return std::stoi(m[1].str());
    }

    return -1;
}

/**
 * @brief Parses a spoken recurrence phrase for calendar events.
 *
 * Recognizes recurrence options such as one-time, daily,
 * weekdays, weekly, biweekly, monthly, and yearly.
 *
 * @param text The spoken recurrence text.
 * @param out Output parameter storing the parsed recurrence value.
 * @return true if a recurrence type was recognized, false otherwise.
 */

static bool parseRecurrenceAnswer(const std::string& text, Recurrence& out) {
    if (contains(text, "one time") || contains(text, "once") || contains(text, "non recurring")) {
        out = Recurrence::NONE;
        return true;
    }
    if (contains(text, "daily") || contains(text, "every day")) {
        out = Recurrence::DAILY;
        return true;
    }
    if (contains(text, "weekday") || contains(text, "weekdays")) {
        out = Recurrence::WEEKDAYS;
        return true;
    }
    if (contains(text, "weekly") || contains(text, "every week")) {
        out = Recurrence::WEEKLY;
        return true;
    }
    if (contains(text, "biweekly") || contains(text, "every two weeks")) {
        out = Recurrence::BIWEEKLY;
        return true;
    }
    if (contains(text, "monthly") || contains(text, "every month")) {
        out = Recurrence::MONTHLY;
        return true;
    }
    if (contains(text, "yearly") || contains(text, "annually")) {
        out = Recurrence::YEARLY;
        return true;
    }
    return false;
}

/**
 * @brief Converts a small number word into its integer value.
 *
 * Supports most numbers from zero to ninety-nine, including compound forms like "twenty one"
 *
 * @param word The number word to convert.
 * @return The integer value, or -1 if the word is not recognized.
 */
static int parseWordNumber(const std::string& input) {
    static const std::unordered_map<std::string, int> ones = {
        {"zero",0},{"one",1},{"two",2},{"three",3},{"four",4},{"five",5},
        {"six",6},{"seven",7},{"eight",8},{"nine",9},{"ten",10},
        {"eleven",11},{"twelve",12},{"thirteen",13},{"fourteen",14},
        {"fifteen",15},{"sixteen",16},{"seventeen",17},{"eighteen",18},
        {"nineteen",19}
    };
    static const std::unordered_map<std::string, int> tens = {
        {"twenty",20},{"thirty",30},{"forty",40},{"fifty",50},
        {"sixty",60},{"seventy",70},{"eighty",80},{"ninety",90}
    };

    std::string s = toLower(trim(input));

    // "a" / "an" → 1
    if (s == "a" || s == "an") return 1;

    // Try direct digit parse first
    try {
        size_t pos;
        int v = std::stoi(s, &pos);
        if (pos == s.size()) return v;
    } catch (...) {}

    // Tokenize on spaces and hyphens
    std::string tmp = s;
    for (char& c : tmp) if (c == '-') c = ' ';
    std::istringstream ss(tmp);
    std::vector<std::string> tokens;
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);

    // Remove filler words ("and", "a", "an") from compound forms
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
        [](const std::string& t){ return t == "and" || t == "a" || t == "an"; }),
        tokens.end());

    int result = 0;
    for (const auto& t : tokens) {
        auto oit = ones.find(t);
        if (oit != ones.end()) { result += oit->second; continue; }
        auto tit = tens.find(t);
        if (tit != tens.end()) { result += tit->second; continue; }
        if (t == "hundred")  { result = (result == 0 ? 1 : result) * 100; continue; }
        if (t == "thousand") { result = (result == 0 ? 1 : result) * 1000; continue; }
        // unknown token — give up
        return -1;
    }
    return result > 0 ? result : -1;
}


static int wordToNumber(const std::string& word) {
    return parseWordNumber(word);
}

/**
 * @brief Parses a timer duration from spoken text.
 *
 * Supports numeric forms such as `10 seconds` or `2 minutes`,
 * and basic word forms such as `five minutes`.
 *
 * @param text The spoken timer request.
 * @return The duration in seconds, or -1 if parsing fails.
 */
static int parseDuration(const std::string& text) {
    // Match digit + unit, e.g. "10 minutes", "2 hours", "90 seconds"
    std::regex digitPattern(R"((?:for\s+)?(\d+)\s*(hour|hours|hr|minute|minutes|min|second|seconds|sec))");
    std::smatch m;
    if (std::regex_search(text, m, digitPattern)) {
        int val = std::stoi(m[1].str());
        std::string unit = m[2].str();
        if (unit == "hour" || unit == "hours" || unit == "hr") val *= 3600;
        else if (unit == "minute" || unit == "minutes" || unit == "min") val *= 60;
        return val;
    }

    // Match word number + unit, e.g. "one hour", "thirty seconds"
    std::regex wordPattern(R"((?:for\s+)?(\w+)\s*(hour|hours|hr|minute|minutes|min|second|seconds|sec))");
    if (std::regex_search(text, m, wordPattern)) {
        int val = wordToNumber(m[1].str());
        if (val > 0) {
            std::string unit = m[2].str();
            if (unit == "hour" || unit == "hours" || unit == "hr") val *= 3600;
            else if (unit == "minute" || unit == "minutes" || unit == "min") val *= 60;
            return val;
        }
    }

    return -1;
}
/**
 * @brief Checks whether a transcript likely contains a command meant for Neura.
 *
 * Uses a list of known command phrases to decide whether the spoken text
 * appears to be directed at the assistant.
 *
 * @param text The normalized transcript text.
 * @return true if the text likely targets the assistant, false otherwise.
 */
static bool likelyDirectedToAssistant(const std::string& text) {
    static const std::vector<std::string> commandPhrases = {
        "neura", "what time", "current time", "tell me the time",
        "switch tab", "next tab", "switch window", "next window",
        "read screen", "read the screen", "start recording", "stop recording",
        "record screen", "take screenshot", "capture screen", "set timer",
        "start timer", "increase volume", "decrease volume", "volume up",
        "volume down", "increase brightness", "decrease brightness",
        "brightness up", "brightness down", "close window", "select all",
        "select text", "copy", "paste", "search", "find", "weather",
        "play media", "pause media", "play song"
    };
    for (const auto& phrase : commandPhrases)
        if (contains(text, phrase)) return true;
    return false;
}

/**
 * @brief Checks whether a transcript is likely just noise.
 *
 * Filters out common noise transcripts such as keyboard clicking,
 * silence, or blank audio markers.
 *
 * @param text The normalized transcript text.
 * @return true if the text appears to be noise, false otherwise.
 */

static bool isNoiseTranscript(const std::string& text) {
    static const std::vector<std::string> noisePhrases = {
        "keyboard clicking", "typing", "mouse clicking", "clicking",
        "background noise", "silence", "blank audio"
    };
    for (const auto& p : noisePhrases)
        if (text == p) return true;
    return false;
}

/**
 * @brief Extracts the search query from a spoken search command.
 *
 * Supports phrases such as `search for ...`, `search ...`, and `find ...`.
 *
 * @param text The spoken search command.
 * @return The extracted search query.
 */

static std::string extractSearchQuery(const std::string& text) {
    auto pos = text.find("search for ");
    if (pos != std::string::npos) return text.substr(pos + 11);
    pos = text.find("search ");
    if (pos != std::string::npos) return text.substr(pos + 7);
    pos = text.find("find ");
    if (pos != std::string::npos) return text.substr(pos + 5);
    return text;
}

/**
 * @brief Extracts the song name from a spoken music command.
 *
 * Supports phrases such as `play song ...` and `play ...`.
 *
 * @param text The spoken music command.
 * @return The extracted song query, or an empty string if none was found.
 */

static std::string extractSongQuery(const std::string& text) {
    auto pos = text.find("play song ");
    if (pos != std::string::npos) return text.substr(pos + 10);
    pos = text.find("play ");
    if (pos != std::string::npos) return text.substr(pos + 5);
    return "";
}


/**
 * @brief Extracts a location from a spoken weather query.
 *
 * Supports weather-related prefixes such as `weather in`,
 * `what is the weather in`, and `do i need a jacket in`.
 *
 * @param text The spoken weather query.
 * @return The extracted location string, or an empty string if none is found.
 */
static std::string extractWeatherLocation(const std::string& text) {
    std::string s = trim(text);

    std::vector<std::string> prefixes = {
        "what is the weather like today in ",
        "what is the weather in ",
        "whats the weather in ",
        "weather like today in ",
        "weather in ",
        "do i need a jacket in ",
        "do i need a coat in ",
        "temperature in "
    };

    for (const auto& p : prefixes) {
        auto pos = s.find(p);
        if (pos != std::string::npos) {
            std::string loc = trim(s.substr(pos + p.size()));
            return loc;
        }
    }

    return "";
}


/** @brief Input audio sample rate before resampling. */
static constexpr int   SAMPLE_RATE_IN = 44100;

/** @brief RMS threshold used to detect the start of speech. */
static constexpr float SPEECH_START_THRESHOLD = 0.015f;

/** @brief RMS threshold used to detect the end of speech. */
static constexpr float SPEECH_END_THRESHOLD   = 0.010f;

/** @brief Maximum utterance length in milliseconds. */
static constexpr int   MAX_UTTERANCE_MS       = 8000;

/** @brief Required silence duration before ending an utterance. */
static constexpr int   END_SILENCE_MS         = 1500;

/** @brief Polling interval for checking new audio samples. */
static constexpr int   POLL_MS                = 100;


/**
 * @brief Computes the RMS value of a block of audio samples.
 *
 * The RMS value is used to estimate speech energy and detect
 * when the user starts and stops speaking.
 *
 * @param samples The audio samples to analyze.
 * @return The computed RMS amplitude.
 */
static float computeRms(const std::vector<float>& samples) {
    if (samples.empty()) return 0.0f;

    double sum = 0.0;
    for (float s : samples) sum += s * s;
    return static_cast<float>(std::sqrt(sum / samples.size()));
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

/**
 * @brief Constructs a `VoiceCommandListener` and loads the Whisper model.
 *
 * Initializes shared references used for microphone input and UI responses,
 * loads configuration settings, and attempts to create the Whisper context.
 *
 * @param modelPath Path to the Whisper model file.
 * @param sharedBuf Shared microphone audio buffer.
 * @param micMuted Shared flag indicating whether the microphone is muted.
 * @param sharedResponse Shared response object used by the UI.
 */

VoiceCommandListener::VoiceCommandListener(const std::string& modelPath,
                                           SharedAudioBuffer& sharedBuf,
                                           std::atomic<bool>& micMuted,
                                           SharedResponse& sharedResponse)
    : m_buf(sharedBuf),
      m_micMuted(micMuted),
      m_response(sharedResponse)
{
    m_config.load();

    whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!m_ctx)
        std::cerr << "[VoiceCommandListener] ERROR: Could not load model: " << modelPath << "\n";
    else
        std::cout << "[VoiceCommandListener] Whisper model loaded successfully.\n";
}

/**
 * @brief Destroys the `VoiceCommandListener`.
 *
 * Stops the background thread and frees the Whisper context if it exists.
 */

VoiceCommandListener::~VoiceCommandListener() {
    stop();
    if (m_ctx) whisper_free(m_ctx);
}


/**
 * @brief Starts the background voice listening thread.
 *
 * @return true if the listener starts successfully, false otherwise.
 */
bool VoiceCommandListener::start() {
    if (!m_ctx) {
        std::cerr << "[VoiceCommandListener] Cannot start - model not loaded.\n";
        return false;
    }
    m_running = true;
    m_thread  = std::thread(&VoiceCommandListener::runLoop, this);
    std::cout << "[VoiceCommandListener] Listening for voice commands...\n";
    return true;
}

/**
 * @brief Stops the background voice listening thread.
 *
 * Signals the listener loop to stop and joins the worker thread if needed.
 */
void VoiceCommandListener::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    return;
}


/**
 * @brief Runs the main microphone listening and speech recognition loop.
 *
 * This function waits for speech to begin, captures audio until silence is
 * detected, resamples the audio to Whisper's expected sample rate, runs
 * speech recognition, normalizes the resulting transcript, and dispatches
 * recognized commands for execution.
 *
 * @return true when the loop exits normally.
 */
bool VoiceCommandListener::runLoop() {
    const int maxSamples = (SAMPLE_RATE_IN * MAX_UTTERANCE_MS) / 1000;
    const int minSamplesToProcess = SAMPLE_RATE / 2;
    const int windowSamples = WINDOW_SECONDS * SAMPLE_RATE;

    while (m_running) {
        if (g_neuraSpeak.load()) {
            std::lock_guard<std::mutex> lock(m_buf.mtx);
            m_buf.samples.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        bool speechStarted = false;
        std::vector<float> captured;
        captured.reserve(maxSamples);

        // Step 1: wait until actual speech starts
        while (m_running && !speechStarted) {
            if (g_neuraSpeak.load()) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));

            std::vector<float> chunk;
            {
                std::lock_guard<std::mutex> lock(m_buf.mtx);
                if (!m_buf.samples.empty()) {
                    chunk.swap(m_buf.samples);
                }
            }

            if (chunk.empty()) continue;

            float rms = computeRms(chunk);
            if (rms >= SPEECH_START_THRESHOLD) {
                speechStarted = true;
                captured.insert(captured.end(), chunk.begin(), chunk.end());
            }
        }

        if (!m_running) break;
        if (!speechStarted) continue;
        if (g_neuraSpeak.load()) continue;

        // Step 2: keep listening until silence lasts long enough
        int silenceMs = 0;

        while (m_running && (int)captured.size() < maxSamples) {
            if (g_neuraSpeak.load()) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));

            std::vector<float> chunk;
            {
                std::lock_guard<std::mutex> lock(m_buf.mtx);
                if (!m_buf.samples.empty()) {
                    chunk.swap(m_buf.samples);
                }
            }

            if (!chunk.empty()) {
                float rms = computeRms(chunk);
                captured.insert(captured.end(), chunk.begin(), chunk.end());

                if (rms >= SPEECH_END_THRESHOLD) {
                    silenceMs = 0;
                } else {
                    silenceMs += POLL_MS;
                }
            } else {
                silenceMs += POLL_MS;
            }

            if (silenceMs >= END_SILENCE_MS) {
                break;
            }
        }

        if (g_neuraSpeak.load()) {
            std::lock_guard<std::mutex> lock(m_buf.mtx);
            m_buf.samples.clear();
            continue;
        }

        if ((int)captured.size() < minSamplesToProcess) {
            continue;
        }

        // Step 3: downsample 44100 -> 16000
        std::vector<float> resampled;
        double ratio = static_cast<double>(SAMPLE_RATE_IN) / SAMPLE_RATE;
        resampled.reserve(captured.size() / ratio + 1);

        for (size_t i = 0; ; ++i) {
            double srcIdx = i * ratio;
            if (srcIdx >= captured.size()) break;
            resampled.push_back(captured[static_cast<size_t>(srcIdx)]);
        }

        whisper_full_params wparams =
            whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        wparams.print_progress   = false;
        wparams.print_realtime   = false;
        wparams.print_timestamps = false;
        wparams.language         = "en";
        wparams.n_threads        = 4;
        wparams.single_segment   = false;
        wparams.no_context       = true;

        int rc = whisper_full(
            m_ctx,
            wparams,
            resampled.data(),
            static_cast<int>(resampled.size())
        );

        if (rc != 0) {
            std::cerr << "[VoiceCommandListener] whisper_full failed\n";
            continue;
        }

        std::string transcript;
        int nSeg = whisper_full_n_segments(m_ctx);
        for (int i = 0; i < nSeg; ++i) {
            transcript += whisper_full_get_segment_text(m_ctx, i);
        }

        if (transcript.empty()) continue;

        std::string lower = normalizeTranscript(transcript);
        {
            std::lock_guard<std::mutex> lock(g_normalizedMtx);
            g_normalizedText = lower;
        }
        std::cout << "[Whisper] Heard: " << transcript << "\n";
        std::cout << "[Whisper] Normalized: " << lower << "\n";

        if (lower.empty()) continue;
        if (isNoiseTranscript(lower)) continue;

        if (g_assistantPaused.load()) {
            if (contains(lower, "resume") ||
                contains(lower, "resume assistant") ||
                contains(lower, "unmute") ||
                contains(lower, "start listening"))
            {
                g_assistantPaused.store(false);
                std::lock_guard<std::mutex> lock(m_response.mtx);
                m_response.text    = "Resumed. Listening again.";
                m_response.updated = true;
                std::cout << "[Neura] Resumed. Listening again.\n";
            }
            continue;
        }

        dispatch(lower);
    }
    return true;
}

// ── respond — uses afplay so open palm can kill it ────────────────────────────

/**
 * @brief Returns the TTS voice name corresponding to the selected accent.
 *
 * Uses the current settings value to map an accent index to the correct
 * voice name for macOS, Windows, or Linux.
 *
 * @return The platform-specific voice name.
 */

// Maps accentIndex to the correct voice name per platform
static std::string getVoiceName() {
#if defined(__APPLE__)
    // 0=American, 1=British, 2=Australian, 3=Canadian
    static const char* voices[] = {"Samantha", "Daniel", "Karen", "Samantha"};
    int idx = std::clamp(g_settings.accentIndex, 0, 3);
    return voices[idx];
#else
    // espeak language codes
    static const char* voices[] = {"en", "en-gb", "en-au", "en-ca"};
    int idx = std::clamp(g_settings.accentIndex, 0, 3);
    return voices[idx];
#endif
}

/**
 * @brief Sends a spoken and on-screen response to the user.
 *
 * Updates the shared UI response text and launches asynchronous
 * text-to-speech playback using the current voice and volume settings.
 *
 * @param msg The message to display and speak.
 */

void VoiceCommandListener::respond(const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(m_response.mtx);
        m_response.text    = msg;
        m_response.updated = true;
    }

    std::cout << "[Neura] " << msg << "\n";

    std::thread([msg]() {
        g_neuraSpeak.store(true);
        g_cancelSpeech.store(false);

        std::string voice = getVoiceName();

#if defined(__APPLE__)
        std::string safe = msg;
        for (char& c : safe) {
            if (c == '"') c = '\'';
        }
        std::string tmp = "/tmp/neura_tts.aiff";
        std::system(("/usr/bin/say -v \"" + voice + "\" -o \"" + tmp + "\" \"" + safe + "\"").c_str());

        if (!g_cancelSpeech.load()) {
            float vol = std::clamp(g_settings.volume, 0, 100) / 100.f;
            std::string playCmd = "/usr/bin/afplay -v " + std::to_string(vol) + " \"" + tmp + "\" &";
            std::system(playCmd.c_str());

            std::this_thread::sleep_for(std::chrono::milliseconds(300));

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

#else
        if (!g_cancelSpeech.load()) {
            std::string safe = msg;
            for (char& c : safe) {
                if (c == '"') c = '\'';
            }
            int amp = std::clamp(g_settings.volume * 2, 0, 200);
            std::system(("/usr/bin/espeak -v " + voice + " -a " + std::to_string(amp) + " \"" + safe + "\" &").c_str());
            while (!g_cancelSpeech.load()) {
                FILE* check = popen("pgrep -x espeak", "r");
                if (!check) break;
                char buf[32] = {};
                bool running = fgets(buf, sizeof(buf), check) != nullptr;
                pclose(check);
                if (!running) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (g_cancelSpeech.load()) {
                std::system("pkill -x espeak 2>/dev/null");
                std::system("pkill -x espeak-ng 2>/dev/null");
            }
        }
#endif

        g_neuraSpeak.store(false);
        g_cancelSpeech.store(false);
    }).detach();

    return; 
}


/**
 * @brief Finalizes and submits a pending calendar event request.
 *
 * Creates a `CalendarEvent` from the fields collected in the
 * multi-step calendar conversation, sends it through the API action,
 * logs the result, and clears the pending request state.
 *
 * @return true if the calendar event was created successfully, false otherwise.
 */
bool VoiceCommandListener::finishPendingCalendarRequest() {

    ConversationLogger logger("conversation.json", /*append=*/true);
    CalendarEvent event;
    event.title            = m_pendingCalendar.title;
    event.startDateTimeISO = m_pendingCalendar.date + "T" + m_pendingCalendar.time24h + ":00";
    event.endDateTimeISO   = addMinutesToIsoLocal(
        m_pendingCalendar.date,
        m_pendingCalendar.time24h,
        m_pendingCalendar.durationMinutes
    );
    event.timeZone         = "America/Toronto";
    event.recurrence       = m_pendingCalendar.recurrence;

    APIAction api(m_config, event);
    bool ok = api.execute();
    std::string cal_msg;

    std::string cal_txt =
        "Create calendar event | "
        "Title: " + m_pendingCalendar.title +
        " | Date: " + m_pendingCalendar.date +
        " | Time: " + m_pendingCalendar.time24h +
        " | Duration: " + std::to_string(m_pendingCalendar.durationMinutes) + " minutes";


    if (ok){
        cal_msg = "Calendar event created: " + m_pendingCalendar.title;
        respond(cal_msg); 
        logger.log(cal_txt, cal_msg, true);
    }else{
        cal_msg = "I could not create the calendar event." + m_pendingCalendar.title;
        respond(cal_msg);
        logger.log(cal_txt, cal_msg, false);
    }

    m_pendingCalendar = PendingCalendarRequest{};
    return ok;
}

// ── dispatch ──────────────────────────────────────────────────────────────────

/**
 * @brief Interprets and executes a recognized voice command.
 *
 * This method routes normalized user speech to the appropriate feature,
 * including calendar creation, pause/resume commands, timers, system actions,
 * clipboard operations, weather queries, media controls, and song playback.
 *
 * @param text The normalized recognized speech command.
 */

void VoiceCommandListener::dispatch(const std::string& text) {
    ConversationLogger logger("conversation.json", /*append=*/true);

    // ── Cancel pending calendar flow ─────────────────────────────────────
    if (m_pendingCalendar.active &&
        (text == "cancel" || text == "never mind" || text == "stop"))
    {
        m_pendingCalendar = PendingCalendarRequest{};
        respond("Okay, cancelled the event setup.");
        return;
    }

    // ── Multi-step calendar flow ──────────────────────────────────────────
    if (m_pendingCalendar.active) {
        if (m_pendingCalendar.step == CalendarPromptStep::ASK_TITLE) {
            m_pendingCalendar.title = trim(text);
            if (m_pendingCalendar.title.empty()) {
                respond("I didn't catch the title. What is the event title?");
                return;
            }

            m_pendingCalendar.step = CalendarPromptStep::ASK_WHEN;
            respond("When is it?");
            return;
        }

        if (m_pendingCalendar.step == CalendarPromptStep::ASK_WHEN) {
            std::string titleDummy, date, time24h;

            if (!parseCalendarCommand("add event x " + text, titleDummy, date, time24h)) {
                respond("Please say something like today/tomorrow at HH:MM am/pm, or on <monthname> DD at HH:MM am/pm.");
                return;
            }

            m_pendingCalendar.date    = date;
            m_pendingCalendar.time24h = time24h;
            m_pendingCalendar.step    = CalendarPromptStep::ASK_DURATION;
            respond("For how long?");
            return;
        }

        if (m_pendingCalendar.step == CalendarPromptStep::ASK_DURATION) {
            int mins = parseDurationMinutes(text);
            if (mins <= 0) {
                respond("Please say a duration like 30 minutes or 1 hour.");
                return ;
            }

            m_pendingCalendar.durationMinutes = mins;
            m_pendingCalendar.step = CalendarPromptStep::ASK_RECURRENCE;
            respond("Is it one time, daily, weekly, biweekly, monthly, yearly, or weekdays?");
            return;
        }

        if (m_pendingCalendar.step == CalendarPromptStep::ASK_RECURRENCE) {
            Recurrence r;
            if (!parseRecurrenceAnswer(text, r)) {
                respond("Please say one time, daily, weekly, biweekly, monthly, yearly, or weekdays.");
                return;
            }

            m_pendingCalendar.recurrence = r;
            finishPendingCalendarRequest();
            return;
        }

        return;
    }

    // ── Calendar events ───────────────────────────────────────────────────
    if (contains(text, "add event") ||
        contains(text, "create event") ||
        contains(text, "set up an alarm") ||
        contains(text, "set an alarm") ||
        contains(text, "create a calendar event"))
    {
        m_pendingCalendar = PendingCalendarRequest{};
        m_pendingCalendar.active = true;
        m_pendingCalendar.step   = CalendarPromptStep::ASK_TITLE;

        respond("What is the title?");
        return;
    }

        // ── Mute / pause ──────────────────────────────────────────────────────
    if (text == "pause" ||
        text == "pause assistant" ||
        text == "cause" ||
        text == "mute" ||
        text == "mute assistant" ||
        text == "stop listening" ||
        text == "stop the assistant")
    {
        g_assistantPaused.store(true);
        const std::string msg = "Paused. Say resume or unmute to continue.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Resume ────────────────────────────────────────────────────────────
    if (text == "resume" || text == "resume assistant" ||
        text == "unmute" || text == "resume listening" ||
        text == "start listening")
    {
        const std::string msg = "Resumed. Listening again.";
        g_assistantPaused.store(false);
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Hello ────────────────────────────────────────────────────────
    if (text == "hello"     ||
        text == "hello neura" ||
        text == "hi neura" ||
        text == "hey neura")
    {
        const std::string msg = "Hello";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Thank You/You're Welcome  ────────────────────────────────────────────────────────
    if (contains(text, "thank you")     ||
        contains(text, "thanks") ||
        contains(text, "thank you neura") ||
        contains(text, "thanks neura")) 
    {
        const std::string msg = "You're Welcome";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }


    // ── Microphone off ────────────────────────────────────────────────────
    if (text == "turn off microphone" ||
        text == "turn off the microphone" ||
        text == "microphone off")
    {
        const std::string msg = "Microphone turned off. Turn it back on from the interface.";
        m_micMuted.store(true);
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Time ──────────────────────────────────────────────────────────────
    if (text == "time" || text == "current time" ||
        text == "what time is it" || text == "whats the time" ||
        text == "tell me the time")
    {
        TimeAction t;
        const std::string msg = responses::formatTimeResponse(t.getCurrentTimeString());
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Switch tabs ───────────────────────────────────────────────────────
    if (contains(text, "switch tab") || contains(text, "next tab") || contains(text, "twitch tab")) {
        bool ok = m_sys.switchTabs();
        const std::string msg = ok ? responses::tabsSwitched() : responses::tabsUnavailable();
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Switch windows ────────────────────────────────────────────────────
    if (contains(text, "switch window") || contains(text, "next window")) {
        bool ok = m_sys.switchWindows();
        const std::string msg = ok ? responses::windowsSwitched() : responses::windowsUnavailable();
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Read screen ───────────────────────────────────────────────────────
    if (contains(text, "read screen")     ||
        contains(text, "read the screen") ||
        contains(text, "what's on screen"))
    {
        std::string sel = m_sys.readSelectedText();
        bool ok = !sel.empty();
        const std::string msg = ok ? responses::screenRead("...") : responses::screenReadUnavailable();
        respond(msg);
        logger.log(text, "read text on screen", ok);
        return;
    }


    // ── Screenshot ────────────────────────────────────────────────────────
    if (contains(text, "screenshot")     ||
        contains(text, "capture screen") ||
        contains(text, "take a screen"))
    {
        bool ok = m_sys.takeScreenshot();
        const std::string msg = ok
            ? "Screenshot taken."
            : "I could not save the screenshot.";
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Timer ─────────────────────────────────────────────────────────────
    if (contains(text, "timer")         ||
        contains(text, "set a timer")   ||
        contains(text, "start a timer") ||
        contains(text, "start timer"))
    {
        int seconds = parseDuration(text);
        if (seconds <= 0) {
            const std::string msg = "Please say something like set a timer for 10 seconds, 5 minutes, or 1 hour.";
            respond(msg);
            logger.log(text, msg, false);
            return;
        }
        if (!m_timer.setTimer(seconds)) {
            const std::string msg = "A timer is already running.";
            respond(msg);
            logger.log(text, msg, false);
            return;
        }
        const std::string msg = "Timer set for " + std::to_string(seconds) + " second(s).";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Volume up ─────────────────────────────────────────────────────────
    if (contains(text, "volume up")       ||
        contains(text, "increase volume") ||
        contains(text, "louder"))
    {
        m_sys.changeVolume(10);
        const std::string msg = "Volume increased.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Volume down ───────────────────────────────────────────────────────
    if (contains(text, "volume down")     ||
        contains(text, "decrease volume") ||
        contains(text, "quieter")         ||
        contains(text, "lower volume"))
    {
        m_sys.changeVolume(-10);
        const std::string msg = "Volume decreased.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Brightness up ─────────────────────────────────────────────────────
    if (contains(text, "brightness up")       ||
        contains(text, "increase brightness") ||
        contains(text, "brighter"))
    {
        m_sys.changeBrightness(10);
        const std::string msg = "Brightness increased.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Brightness down ───────────────────────────────────────────────────
    if (contains(text, "brightness down")     ||
        contains(text, "decrease brightness") ||
        contains(text, "dimmer")              ||
        contains(text, "lower brightness"))
    {
        m_sys.changeBrightness(-10);
        const std::string msg = "Brightness decreased.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Close window ──────────────────────────────────────────────────────
    if (contains(text, "close window")    ||
        contains(text, "close this")      ||
        contains(text, "close the window"))
    {
        bool ok = m_sys.closeWindow();
        const std::string msg = ok ? "Closing the current window."
                                   : "I could not close the current window. Check app focus and accessibility permissions.";
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Select all ────────────────────────────────────────────────────────
    if (contains(text, "select all")  ||
        contains(text, "select text") ||
        contains(text, "highlight all"))
    {
        m_sys.selectAll();
        const std::string msg = "Selected all text.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Copy ──────────────────────────────────────────────────────────────
    if (contains(text, "copy")          ||
        contains(text, "copy that")     ||
        contains(text, "copy text")     ||
        contains(text, "copy selected"))
    {
        m_sys.copyText();
        const std::string msg = "Text copied.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Paste ─────────────────────────────────────────────────────────────
    if (contains(text, "paste")      ||
        contains(text, "paste that") ||
        contains(text, "paste text"))
    {
        m_sys.pasteText();
        const std::string msg = "Text pasted.";
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Search / Spotlight ────────────────────────────────────────────────
    if (contains(text, "search") || contains(text, "find")) {
        std::string query = extractSearchQuery(text);
        m_sys.searchComputer(query);

        const std::string msg = "Searching for: " + query;
        respond(msg);
        logger.log(text, msg, true);
        return;
    }

    // ── Weather ───────────────────────────────────────────────────────────
    if (contains(text, "weather") ||
        contains(text, "what is the weather") ||
        contains(text, "weather like today") ||
        contains(text, "do i need a jacket") ||
        contains(text, "do i need a coat") ||
        contains(text, "temperature in "))
    {
        std::string loc = extractWeatherLocation(text);

        WeatherAction weather(m_config, loc);
        if (!weather.execute()) {
            respond(responses::weatherUnavailable());
            return;
        }

        std::string summary = weather.getSummary();

        if (contains(text, "do i need a jacket") || contains(text, "do i need a coat")) {
            auto result = weather.getResult();
            if (!result) {
                respond(summary);
                return;
            }

            double temp = result->temperatureCelsius;
            if (temp <= 8.0) {
                respond("Yes, you should wear a jacket. " + summary);
            } else if (temp <= 15.0) {
                respond("You may want a light jacket. " + summary);
            } else {
                respond("You probably do not need a jacket. " + summary);
            }
            return;
        }

        respond(summary);
        logger.log(text, summary, true);
        return;
    }

    // ── Weather ───────────────────────────────────────────────────────────
    if (contains(text, "weather")              ||
        contains(text, "what is the weather")  ||
        contains(text, "weather like today"))
    {
        WeatherAction weather(m_config, "");
        bool ok = weather.execute();
        const std::string msg = ok ? weather.getSummary() : responses::weatherUnavailable();
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Play media ────────────────────────────────────────────────────────
    if (contains(text, "play media")   ||
        contains(text, "resume media") ||
        contains(text, "play music"))
    {
        SystemAction mediaAction(m_config, MediaCommand::PLAY);
        bool ok = mediaAction.execute();
        const std::string msg = ok ? responses::mediaPlayed() : responses::mediaUnavailable();
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Pause media ───────────────────────────────────────────────────────
    if (contains(text, "pause media") ||
        contains(text, "pause music") ||
        text == "cause music" ||
        contains(text, "stop media"))
    {
        SystemAction mediaAction(m_config, MediaCommand::PAUSE);
        bool ok = mediaAction.execute();
        const std::string msg = ok ? responses::mediaPaused() : responses::mediaUnavailable();
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Play song ─────────────────────────────────────────────────────────
    if (contains(text, "play song") || contains(text, "play ")) {
        std::string songQuery = extractSongQuery(text);
        if (songQuery.empty()) {
            const std::string msg = responses::songUnavailable();
            respond(msg);
            logger.log(text, msg, false);
            return;
        }
        BrowserAction browser(m_config, songQuery, "");
        bool ok = browser.execute();
        const std::string msg = ok ? "Playing " + songQuery + "." : responses::songUnavailable();
        respond(msg);
        logger.log(text, msg, ok);
        return;
    }

    // ── Unrecognised ──────────────────────────────────────────────────────
    if (likelyDirectedToAssistant(text)) {
        respond(responses::unknownCommand());
        return;
    }
    return; 
}