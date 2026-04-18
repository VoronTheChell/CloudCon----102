#pragma once

#include <string>

class SystemIntegration {
public:
    bool open_file(const std::string& local_path) const;
    bool open_directory(const std::string& local_path) const;
    bool open_url(const std::string& url) const;

private:
    bool open_target(const std::string& target) const;
};