#include "BrowserAction.h"
#include "json.hpp"
#include <cstdlib>

using json = nlohmann::json;

BrowserAction::BrowserAction(const Configuration& cfg,
                             const std::string&   title,
                             const std::string&   artist)
    : Action(cfg), songTitle(title), songArtist(artist) {}

std::string BrowserAction::describe() const {
    return "Browser: Play \"" + songTitle + "\" by \"" + songArtist + "\"";
}

bool BrowserAction::execute() {
    if (!config.permissions.allowMediaControl) {
        log("MediaControl permission denied.");
        return false;
    }
    if (songTitle.empty()) {
        log("BrowserAction: title must not be empty.");
        return false;
    }

    std::string url;

    // Prefer a direct watch URL via the API to skip the search results page
    if (!config.apiSettings.youtubeApiKey.empty()) {
        std::string videoId = fetchFirstVideoId();
        if (!videoId.empty()) {
            url = "https://www.youtube.com/watch?v=" + videoId;
            log("BrowserAction: opening first result directly — " + url);
        } else {
            log("BrowserAction: API returned no results, falling back to search page.");
        }
    } else {
        log("BrowserAction: no youtubeApiKey set — falling back to search page.");
    }

    // Fall back to the YouTube search page if the API is unavailable or returns nothing
    if (url.empty())
        url = buildFallbackSearchUrl();

    openedUrl = url;
    return openInBrowser(url);
}

// Queries the YouTube Data API for the top video match; returns its ID or "" on failure
std::string BrowserAction::fetchFirstVideoId() const {
    std::string query = urlEncode(songTitle + " " + songArtist);
    std::string apiUrl =
        "https://www.googleapis.com/youtube/v3/search"
        "?part=id"
        "&type=video"
        "&maxResults=1"
        "&q=" + query +
        "&key=" + config.apiSettings.youtubeApiKey;

    log("YouTube API GET: " + apiUrl);
    std::string body = httpGet(apiUrl);
    if (body.empty()) {
        log("BrowserAction: empty response from YouTube API.");
        return "";
    }

    try {
        auto j = json::parse(body);

        if (j.contains("error")) {
            log("BrowserAction: YouTube API error " +
                std::to_string(j["error"].value("code", 0)) +
                " — " + j["error"].value("message", std::string("unknown")));
            return "";
        }

        auto& items = j["items"];
        if (!items.is_array() || items.empty()) {
            log("BrowserAction: YouTube API returned no items.");
            return "";
        }

        std::string videoId = items[0]["id"]["videoId"].get<std::string>();
        log("BrowserAction: found video ID " + videoId);
        return videoId;

    } catch (const std::exception& e) {
        log("BrowserAction: JSON parse error — " + std::string(e.what()));
        return "";
    }
}

// Builds a YouTube search URL for the song; used when the API is skipped or fails
std::string BrowserAction::buildFallbackSearchUrl() const {
    return "https://www.youtube.com/results?search_query="
         + urlEncode(songTitle + " " + songArtist);
}

// Opens a URL in the default browser; uses `open` on macOS, `xdg-open` on Linux
bool BrowserAction::openInBrowser(const std::string& url) const {
    std::string cmd;
#if defined(__APPLE__)
    cmd = "open \"" + url + "\"";
#else
    cmd = "xdg-open \"" + url + "\"";
#endif
    bool ok = std::system(cmd.c_str()) == 0;
    if (!ok) log("BrowserAction: failed to open browser.");
    return ok;
}