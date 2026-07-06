/**
 * action.cpp  —  base Action class implementation
 *
 * Contains: log(), httpGet(), httpPost()
 * All subclasses inherit these through Action.
 */

#include "action.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <ctime>

// libcurl write callback — appends each received chunk to a std::string
static size_t curlWriteCallback(char* ptr, size_t size,
                                 size_t nmemb, void* userdata) {
    size_t bytes = size * nmemb;
    static_cast<std::string*>(userdata)->append(ptr, bytes);
    return bytes;
}

// Prepends an ISO-8601 timestamp and writes the message to stdout
void Action::log(const std::string& message) const {
    std::time_t now = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    std::cout << "[" << ts << "] " << message << "\n";
}

// Performs a GET request and returns the response body, or "" on failure
std::string Action::httpGet(const std::string& url) const {
    CURL*       curl = curl_easy_init();
    std::string body;
    if (!curl) { log("httpGet: curl_easy_init() failed."); return ""; }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // follow redirects
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L); // 10-second timeout

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log("httpGet error: " + std::string(curl_easy_strerror(res)));
    } else {
        // Still check for HTTP-level errors even when the transfer succeeded
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        if (httpCode >= 400)
            log("httpGet: HTTP " + std::to_string(httpCode) + " error from server.");
    }

    curl_easy_cleanup(curl);
    return body;
}

// Performs a POST request with an optional Bearer token; returns the response body
std::string Action::httpPost(const std::string& url,
                              const std::string& bearerToken,
                              const std::string& body,
                              const std::string& contentType) const {
    CURL*       curl = curl_easy_init();
    std::string response;
    if (!curl) { log("httpPost: curl_easy_init() failed."); return ""; }

    // Build request headers; Authorization is omitted when bearerToken is empty
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
    if (!bearerToken.empty())
        headers = curl_slist_append(headers,
                      ("Authorization: Bearer " + bearerToken).c_str());

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size()); // explicit size handles embedded nulls
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       15L); // slightly longer than GET

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log("httpPost error: " + std::string(curl_easy_strerror(res)));
    } else {
        // Same HTTP-error guard as httpGet
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        if (httpCode >= 400)
            log("httpPost: HTTP " + std::to_string(httpCode) + " error from server.");
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

// Percent-encodes a string for safe inclusion in a URL query parameter
std::string Action::urlEncode(const std::string& raw) {
    std::ostringstream out;
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            // RFC 3986 unreserved characters pass through unchanged
            out << c;
        } else {
            // All other bytes are encoded as %XX
            out << '%' << std::uppercase << std::hex
                << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    return out.str();
}