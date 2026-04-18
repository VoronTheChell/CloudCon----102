#include "YandexOAuthClient.h"

#include <curl/curl.h>
#include <json-glib/json-glib.h>

#include <sstream>

namespace {

size_t write_to_string(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total_size = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string json_string_member(JsonObject* object, const char* name) {
    if (!json_object_has_member(object, name)) {
        return {};
    }
    return json_object_get_string_member(object, name);
}

bool parse_json(const std::string& text, JsonParser** out_parser, GError** out_error) {
    JsonParser* parser = json_parser_new();
    if (!json_parser_load_from_data(parser, text.c_str(), static_cast<gssize>(text.size()), out_error)) {
        g_object_unref(parser);
        return false;
    }
    *out_parser = parser;
    return true;
}

std::string escape(CURL* curl, const std::string& value) {
    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result = encoded ? encoded : "";
    if (encoded) {
        curl_free(encoded);
    }
    return result;
}

} // namespace

std::string YandexOAuthClient::build_verification_code_url(const std::string& client_id) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {};
    }

    const std::string encoded_client_id = escape(curl, client_id);
    const std::string encoded_redirect = escape(curl, "https://oauth.yandex.com/verification_code");
    curl_easy_cleanup(curl);

    std::ostringstream url;
    url << "https://oauth.yandex.com/authorize?response_type=code"
        << "&client_id=" << encoded_client_id
        << "&redirect_uri=" << encoded_redirect
        << "&force_confirm=yes";
    return url.str();
}

YandexOAuthResult YandexOAuthClient::exchange_code(
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& code
) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, "Не удалось инициализировать CURL."};
    }

    std::ostringstream body;
    body << "grant_type=authorization_code"
         << "&code=" << escape(curl, code)
         << "&client_id=" << escape(curl, client_id)
         << "&client_secret=" << escape(curl, client_secret);

    curl_easy_cleanup(curl);
    return request_token(body.str());
}

YandexOAuthResult YandexOAuthClient::refresh_access_token(
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& refresh_token
) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, "Не удалось инициализировать CURL."};
    }

    std::ostringstream body;
    body << "grant_type=refresh_token"
         << "&refresh_token=" << escape(curl, refresh_token)
         << "&client_id=" << escape(curl, client_id)
         << "&client_secret=" << escape(curl, client_secret);

    curl_easy_cleanup(curl);
    return request_token(body.str());
}

YandexOAuthResult YandexOAuthClient::request_token(const std::string& post_fields) const {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, "Не удалось инициализировать CURL."};
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    curl_easy_setopt(curl, CURLOPT_URL, "https://oauth.yandex.com/token");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, curl_easy_strerror(code)};
    }

    GError* error = nullptr;
    JsonParser* parser = nullptr;
    if (!parse_json(response, &parser, &error)) {
        const std::string message = error ? error->message : "Некорректный JSON ответа.";
        if (error) {
            g_error_free(error);
        }
        return {false, message};
    }

    JsonObject* root_obj = json_node_get_object(json_parser_get_root(parser));

    if (http_code < 200 || http_code >= 300) {
        const std::string error_code = json_string_member(root_obj, "error");
        const std::string description = json_string_member(root_obj, "error_description");
        std::ostringstream ss;
        ss << "OAuth ошибка";
        if (!error_code.empty()) {
            ss << " (" << error_code << ")";
        }
        if (!description.empty()) {
            ss << ": " << description;
        }
        g_object_unref(parser);
        return {false, ss.str()};
    }

    YandexOAuthResult result;
    result.success = true;
    result.message = "Токен успешно получен.";
    result.access_token = json_string_member(root_obj, "access_token");
    result.refresh_token = json_string_member(root_obj, "refresh_token");
    if (json_object_has_member(root_obj, "expires_in")) {
        result.expires_in = static_cast<long>(json_object_get_int_member(root_obj, "expires_in"));
    }

    g_object_unref(parser);

    if (result.access_token.empty()) {
        return {false, "Yandex OAuth не вернул access_token."};
    }

    return result;
}
