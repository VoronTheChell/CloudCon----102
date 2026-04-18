#pragma once

#include <string>

struct YandexOAuthResult {
    bool success {false};
    std::string message;
    std::string access_token;
    std::string refresh_token;
    long expires_in {0};
};

class YandexOAuthClient {
public:
    static std::string build_verification_code_url(const std::string& client_id);

    YandexOAuthResult exchange_code(
        const std::string& client_id,
        const std::string& client_secret,
        const std::string& code
    ) const;

    YandexOAuthResult refresh_access_token(
        const std::string& client_id,
        const std::string& client_secret,
        const std::string& refresh_token
    ) const;

private:
    YandexOAuthResult request_token(const std::string& post_fields) const;
};
