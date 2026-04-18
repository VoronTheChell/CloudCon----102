#include "NotificationManager.h"

#include <libnotify/notify.h>

NotificationManager::NotificationManager() = default;

NotificationManager::~NotificationManager() {
    shutdown();
}

bool NotificationManager::initialize(const std::string& app_name) {
    if (initialized_) {
        return true;
    }

    initialized_ = notify_init(app_name.c_str());
    return initialized_;
}

void NotificationManager::shutdown() {
    if (initialized_) {
        notify_uninit();
        initialized_ = false;
    }
}

bool NotificationManager::is_initialized() const {
    return initialized_;
}

void NotificationManager::show_notification(
    const std::string& title,
    const std::string& body,
    const std::string& icon_name
) {
    if (!initialized_) {
        return;
    }

    NotifyNotification* notification = notify_notification_new(
        title.c_str(),
        body.c_str(),
        icon_name.empty() ? nullptr : icon_name.c_str()
    );

    if (notification == nullptr) {
        return;
    }

    notify_notification_set_timeout(notification, 3500);
    notify_notification_show(notification, nullptr);
    g_object_unref(G_OBJECT(notification));
}

void NotificationManager::show_info(const std::string& title, const std::string& body) {
    show_notification(title, body, "dialog-information");
}

void NotificationManager::show_success(const std::string& title, const std::string& body) {
    show_notification(title, body, "emblem-default");
}

void NotificationManager::show_error(const std::string& title, const std::string& body) {
    show_notification(title, body, "dialog-error");
}