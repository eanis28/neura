#include "TimeAction.h"
#include <ctime>
#include <sstream>
#include <iomanip>

std::string TimeAction::getCurrentTimeString() const {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};

    localTime = *std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%I:%M %p");
    return oss.str();
}