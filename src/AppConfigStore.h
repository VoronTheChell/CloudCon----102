#pragma once

#include <string>

struct AppConfig {
    std::string client_id;
    std::string client_secret;
    std::string access_token;
    std::string refresh_token;
    std::string remote_root {"disk:/CloudClient"};
};

class AppConfigStore {
public:
    bool load(AppConfig& out_config) const;
    bool save(const AppConfig& config) const;

private:
    std::string config_path() const;
};
