#pragma once

#include <string>

class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();

    bool initialize(const std::string& app_name);
    void shutdown();

    void show_info(const std::string& title, const std::string& body);
    void show_success(const std::string& title, const std::string& body);
    void show_error(const std::string& title, const std::string& body);

    bool is_initialized() const;

private:
    bool initialized_ {false};

    void show_notification(const std::string& title, const std::string& body, const std::string& icon_name);
};