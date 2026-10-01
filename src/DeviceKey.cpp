#include "DeviceKey.h"

#include <algorithm>
#include <cctype>

std::string deviceKey(const std::string& name, const std::string& serial, const std::string& location)
{
    const char* const whitespace = " \t\r\n";
    std::string trimmed;
    const std::size_t first = serial.find_first_not_of(whitespace);
    if(first != std::string::npos)
    {
        trimmed = serial.substr(first, serial.find_last_not_of(whitespace) - first + 1);
    }

    std::string lowered = trimmed;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    if(trimmed.empty() || lowered == "none")
    {
        return name + "|" + location;
    }
    return name + "|" + trimmed;
}
