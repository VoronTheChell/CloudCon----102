#include "SystemIntegration.h"

#include <cstdlib>
#include <filesystem>
#include <string>

bool SystemIntegration::open_target(const std::string& target) const {
    std::string command = "xdg-open \"" + target + "\" >/dev/null 2>&1 &";
    const int result = std::system(command.c_str());
    return result == 0;
}

bool SystemIntegration::open_file(const std::string& local_path) const {
    if (!std::filesystem::exists(local_path)) {
        return false;
    }

    return open_target(local_path);
}

bool SystemIntegration::open_directory(const std::string& local_path) const {
    if (!std::filesystem::exists(local_path)) {
        return false;
    }

    return open_target(local_path);
}

bool SystemIntegration::open_url(const std::string& url) const {
    return open_target(url);
}