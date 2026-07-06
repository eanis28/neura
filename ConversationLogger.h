#pragma once

/**
 * @file ConversationLogger.h
 * @brief Real-time JSON logger for command/response exchanges.
 *
 * @details Every exchange is appended to a JSON array immediately after it
 * occurs, so the log file is always current — even if the program exits
 * unexpectedly. The output file is a valid JSON array for the entire session.
 *
 * Output format (conversation.json):
 * @author Meridith Shang
 */

#include <string>
#include <fstream>

/**
 * @brief Appends structured JSON log entries for every command/response cycle.
 *
 * @details Opens (or creates) a JSON file at construction and writes each
 * entry immediately via log(). The destructor closes the JSON array
 * correctly so the file is always well-formed, even after an unclean exit
 * during normal operation (entries already flushed remain valid).
 * @author Meridith Shang
 */
class ConversationLogger {
public:
    /**
     * @author Meridith Shang
     * @brief Opens or creates the JSON log file.
     *
     * @details When append is true, new entries are added after any
     * existing content, preserving history across sessions. When false,
     * the file is truncated and a fresh JSON array is started.
     *
     * @param filePath Path to the JSON log file.
     *                 Defaults to "conversation.json" in the working directory.
     * @param append   true to preserve existing log entries across sessions;
     *                 false to start a new log each session.
     */
    explicit ConversationLogger(const std::string& filePath = "conversation.json",
                                bool append = true);

    /**
     * @brief Closes the JSON array and flushes the file to disk.
     *
     * @details Writes the closing ] bracket to ensure the file is a
     * valid JSON array. Entries written before an unexpected termination
     * (e.g. a crash after construction) remain readable up to the last
     * successfully flushed entry.
     */
    ~ConversationLogger();

    /**
     * @author Meridith Shang
     * @brief Appends one command/response exchange to the log.
     *
     * @details Serialises the exchange as a JSON object with id,
     * timestamp, command, response, and success fields,
     * then flushes immediately so the entry is durable. Should be called
     * after every action, regardless of outcome.
     *
     * @param command  The raw input string issued by the user or caller.
     * @param response The reply or status message produced by the action.
     * @param success  true if the action completed without error;
     *                 false otherwise.
     */
    void log(const std::string& command,
             const std::string& response,
             bool               success);

    /**
     * @author Meridith Shang
     * @brief Returns the path of the file being written to.
     *
     * @return A const reference to the log file path string set at construction.
     */
    const std::string& getFilePath() const { return filePath; }

private:
    std::string  filePath;          ///< Path to the JSON log file.
    std::fstream file;              ///< Open file stream; kept alive for the object's lifetime.
    int          entryCount = 0;    ///< Monotonically increasing counter used to assign entry IDs.
    bool         firstEntry = true; ///< true until the first entry is written; controls comma placement.

    /**
     * @author Meridith Shang
     * @brief Returns the current local time formatted as an ISO 8601 string.
     *
     * @return A string of the form "YYYY-MM-DDTHH:MM:SS" representing
     *         the local wall-clock time at the moment of the call.
     */
    static std::string currentTimestamp();

    /**
     * @author Meridith Shang
     * @brief Escapes special characters in s so it is safe to embed in a JSON string.
     *
     * @details Handles the characters required by the JSON specification:
     * \", \\, \n, \r, \t, and control characters
     * in the range U+0000–U+001F.
     *
     * @param s The raw input string to escape.
     * @return A copy of s with all necessary characters escaped.
     */
    static std::string jsonEscape(const std::string& s);
};