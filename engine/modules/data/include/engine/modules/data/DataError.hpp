#pragma once

#include <string>
#include <sstream>

namespace engine {

struct DataError {
    std::string filename;
    int line = -1;
    std::string key;
    std::string reason;

    std::string format() const {
        std::ostringstream oss;
        oss << "[" << (filename.empty() ? "data" : filename);
        if (line > 0) {
            oss << ":" << line;
        }
        oss << "] ";
        if (!key.empty()) {
            oss << "at '" << key << "': ";
        }
        oss << reason;
        return oss.str();
    }
};

} // namespace engine
