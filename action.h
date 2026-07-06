#pragma once

/**
 * @file action.h
 * @brief Base @c Action class and shared HTTP/utility helpers.
 * @details All action subclasses inherit from action and receive
 * httpGet(), httpPost(), urlEncode(), and log() at no extra
 * cost. Concrete subclasses only need to implement execute() and
 * describe().
 * @author Meridith Shang
 */

#include "config.h"
#include <string>

/**
 * @brief Abstract base class for all executable actions.
 * @details Holds a reference to the shared Configuration object and
 * exposes synchronous HTTP helpers (backed by libcurl) plus RFC-3986
 * percent-encoding. Subclasses must override execute() and
 * describe(); everything else is inherited.
 * @author Meridith Shang
 */
class Action {
public:
    /**
     * @brief Constructs an Action with the application-wide configuration.
     *
     * @param cfg Read-only configuration object whose lifetime must exceed
     *            that of this Action instance.
     * 
     * @author Meridith Shang
     */
    explicit Action(const Configuration& cfg) : config(cfg) {}

    /// @brief Virtual destructor — ensures correct cleanup of derived classes.
    virtual ~Action() = default;

    /**
     * @author Meridith Shang   
     * @brief Carries out the action.
     * @return true if the action completed successfully; false otherwise.
     */
    virtual bool execute() = 0;

    /**
     * @author Meridith Shang
     * @brief Returns a human-readable description of what this action will do.
     * @return A short summary string suitable for logging or display.
     */
    virtual std::string describe() const = 0;

protected:
    const Configuration& config; ///< Shared, read-only application configuration.

    /**
     * @author Meridith Shang
     * @brief Writes a timestamped log line to @c stdout.
     * @param message The text to log. A timestamp prefix is prepended automatically.
     */
    void log(const std::string& message) const;

    /**
     * @author Meridith Shang
     * @brief Performs a synchronous HTTP GET request via libcurl.
     * @param url Fully-qualified URL to request.
     * @return The response body as a string, or an empty string on any error.
     */
    std::string httpGet(const std::string& url) const;

    /**
     * @author Meridith Shang
     * @brief Performs a synchronous HTTP POST request via libcurl.
     *
     * @param url         Fully-qualified URL to POST to.
     * @param bearerToken OAuth 2.0 Bearer token used in the @c Authorization header.
     * @param body        Raw request body (typically JSON or form-encoded data).
     * @param contentType MIME type for the @c Content-Type header;
     *                    defaults to @c "application/json".
     * @return The response body as a string, or an empty string on any error.
     */
    std::string httpPost(const std::string& url,
                         const std::string& bearerToken,
                         const std::string& body,
                         const std::string& contentType = "application/json") const;

    /**
     * @author Meridith Shang
     * @brief Percent-encodes a raw string according to RFC 3986.
     *
     * @details Encodes all characters that are not unreserved
     * (@c ALPHA, @c DIGIT, @c '-', @c '.', @c '_', @c '~') into
     * @c %XX escape sequences. Safe to use for query-parameter values
     * and path segments.
     *
     * @param raw The unencoded input string.
     * @return The percent-encoded output string.
     */
    static std::string urlEncode(const std::string& raw);
};