#include "ConversationLogger.h"

#include <ctime>
#include <sstream>
#include <iostream>

// Constructor 
//
// The JSON file is kept as a valid array at all times:
//   - On first write: we write the opening '[' then each entry
//   - On append: we read the file to count existing entries and
//     find the position of the closing ']' so we can insert before it
//
ConversationLogger::ConversationLogger(const std::string& path, bool append)
    : filePath(path) {

    if (append) {
        // Try to open the existing file and count entries
        std::ifstream reader(filePath);
        if (reader.is_open()) {
            std::string content((std::istreambuf_iterator<char>(reader)),
                                 std::istreambuf_iterator<char>());
            reader.close();

            // Count existing entries by counting "\"id\":" occurrences
            size_t pos = 0;
            while ((pos = content.find("\"id\":", pos)) != std::string::npos) {
                ++entryCount;
                ++pos;
            }

            firstEntry = (entryCount == 0);

            // Open for read/write so we can overwrite the closing ']'
            file.open(filePath, std::ios::in | std::ios::out);
            if (file.is_open() && entryCount > 0) {
                // Seek to just before the closing ']' to append new entries
                file.seekp(-2, std::ios::end); // step back over "\n]"
                return;
            }
            file.close();
        }
    }

    // No existing file (or append=false) — start fresh
    file.open(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[ConversationLogger] Could not open: " << filePath << "\n";
        return;
    }

    file << "[\n";
    file.flush();
    entryCount = 0;
    firstEntry = true;
}

//  Destructor 
ConversationLogger::~ConversationLogger() {
    if (file.is_open()) {
        file << "\n]\n";
        file.flush();
        file.close();
    }
}

// log() 
void ConversationLogger::log(const std::string& command,
                              const std::string& response,
                              bool               success) {
    if (!file.is_open()) return;

    ++entryCount;

    // Add comma separator between entries
    if (!firstEntry)
        file << ",\n";
    firstEntry = false;

    file << "  {\n"
         << "    \"id\":        " << entryCount                      << ",\n"
         << "    \"timestamp\": \"" << currentTimestamp()            << "\",\n"
         << "    \"command\":   \"" << jsonEscape(command)           << "\",\n"
         << "    \"response\":  \"" << jsonEscape(response)          << "\",\n"
         << "    \"success\":   "   << (success ? "true" : "false")  << "\n"
         << "  }";

    // Flush immediately so the file is always valid even if we crash
    file.flush();
}

// Helpers 

std::string ConversationLogger::currentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    return buf;
}

std::string ConversationLogger::jsonEscape(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:
                if (c < 0x20) {
                    // Control characters — encode as \uXXXX
                    char hex[8];
                    snprintf(hex, sizeof(hex), "\\u%04x", c);
                    out << hex;
                } else {
                    out << c;
                }
        }
    }
    return out.str();
}