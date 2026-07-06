#pragma once

/**
 * @file BrowserAction.h
 * @brief Action that finds and plays the first YouTube result for a given song.
 *
 * @details Uses the YouTube Data API v3 (when an API key is present in
 * config.ini) to resolve the best matching video ID, then opens the
 * resulting youtube.com/watch URL in the OS default browser.
 * If the API key is absent or the search call fails, a YouTube search-results
 * page URL is opened as a fallback.
 * @author Meridith Shang
 */

#include "action.h"
#include <string>

/**
 * @brief Opens the first YouTube video matching a song title and artist.
 *
 * @details Inherits HTTP helpers and configuration access from Action.
 * On execute(), the action:
 * - Calls fetchFirstVideoId() to query the YouTube Data API v3.
 * - Falls back to buildFallbackSearchUrl() if the API call fails or
 *    no API key is configured.
 * - Passes the resolved URL to openInBrowser(), which delegates to
 *    the platform's default browser launcher
 *    (@c xdg-open on Linux, open on macOS, start on Windows).
 * 
 * @author Meridith Shang
 *
 */
class BrowserAction : public Action {
public:
    /**
     * @author Meridith Shang
     * @brief Constructs a BrowserAction for the given song.
     *
     * @param cfg    Application configuration (may contain a YouTube API key).
     * @param title  Title of the song to search for.
     * @param artist Name of the performing artist or band.
     */
    BrowserAction(const Configuration& cfg,
                  const std::string&   title,
                  const std::string&   artist);

    /**
     * @author Meridith Shang
     * @brief Searches YouTube and opens the best matching video in the browser.
     *
     * @return true if a URL was successfully opened, false otherwise.
     */
    bool execute() override;

    /**
     * @author Meridith Shang
     * @brief Returns a human-readable summary of the action.
     *
     * @return A string of the form "Play \"<title>\" by <artist> on YouTube".
     */
    std::string describe() const override;

    /**
     * @author Meridith Shang
     * @brief Returns the song title supplied at construction.
     * @return Reference to the stored song title string.
     */
    const std::string& getTitle()  const { return songTitle;  }

    /**
     * @author Meridith Shang
     * @brief Returns the artist name supplied at construction.
     * @return Reference to the stored artist string.
     */
    const std::string& getArtist() const { return songArtist; }

    /**
     * @author Meridith Shang
     * @brief Returns the URL that was opened by the last successful execute() call.
     *
     * @return The YouTube watch or search-results URL, or an empty string if
     *         execute() has not been called or failed.
     */
    const std::string& getOpenedUrl() const { return openedUrl; }

private:
    std::string songTitle;  ///< Song title supplied at construction.
    std::string songArtist; ///< Artist name supplied at construction.
    std::string openedUrl;  ///< Populated with the opened URL after a successful @c execute().

    /**
     * @author Meridith Shang
     * @brief Queries the YouTube Data API v3 for the top video matching the song.
     *
     * @details Constructs a search request using songTitle and songArtist,
     * percent-encodes the query via urlEncode(), and issues an httpGet()
     * call. Parses the JSON response and extracts the first videoId.
     *
     * @return The YouTube video ID string (e.g. "dQw4w9WgXcQ"),
     *         or an empty string if the API key is missing, the request fails,
     *         or no results are returned.
     */
    std::string fetchFirstVideoId() const;

    /**
     * @author Meridith Shang
     * @brief Builds a YouTube search-results URL as a fallback.
     *
     * @details Used when fetchFirstVideoId() returns an empty string.
     * Percent-encodes the combined title and artist query and appends it to
     * https://www.youtube.com/results?search_query=.
     *
     * @return A fully-qualified, percent-encoded YouTube search URL.
     */
    std::string buildFallbackSearchUrl() const;

    /**
     * @author Meridith Shang
     * @brief Opens a URL in the operating system's default browser.
     *
     * @details Dispatches to the appropriate launcher at runtime:
     * - Linux — xdg-open
     * - macOS — open
     * - Windows — start
     *
     * @param url The fully-qualified URL to open.
     * @return true if the launcher process was started without error;
     *         false otherwise.
     */
    bool openInBrowser(const std::string& url) const;
};