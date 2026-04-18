#include "YandexDiskClient.h"

#include <curl/curl.h>
#include <json-glib/json-glib.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

size_t write_to_string(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total_size = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t write_to_file(void* contents, size_t size, size_t nmemb, void* userp) {
    return std::fwrite(contents, size, nmemb, static_cast<FILE*>(userp));
}

bool ensure_parent_dirs_for_file(const std::string& file_path) {
    std::error_code ec;
    const fs::path parent = fs::path(file_path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
    }
    return !ec;
}

std::string file_type_to_mime(const std::string& type, const std::string& mime_type) {
    if (type == "dir") {
        return "inode/directory";
    }
    if (!mime_type.empty()) {
        return mime_type;
    }
    return "application/octet-stream";
}

std::string json_string_member(JsonObject* obj, const char* name) {
    if (!json_object_has_member(obj, name)) {
        return {};
    }
    return json_object_get_string_member(obj, name);
}

std::uint64_t json_uint64_member(JsonObject* obj, const char* name) {
    if (!json_object_has_member(obj, name)) {
        return 0;
    }
    return static_cast<std::uint64_t>(json_object_get_int_member(obj, name));
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

} // namespace

YandexDiskClient::YandexDiskClient(std::string oauth_token, std::string remote_root)
    : oauth_token_(std::move(oauth_token)),
      remote_root_(std::move(remote_root)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (remote_root_.empty()) {
        remote_root_ = "disk:/CloudClient";
    }
}

bool YandexDiskClient::configured() const {
    return !oauth_token_.empty();
}

std::string YandexDiskClient::build_remote_path(const std::string& remote_relative_path) const {
    std::string rel = remote_relative_path;

    if (rel.empty() || rel == "/") {
        return remote_root_;
    }

    while (!rel.empty() && rel.front() == '/') {
        rel.erase(rel.begin());
    }

    if (!remote_root_.empty() && remote_root_.back() == '/') {
        return remote_root_ + rel;
    }

    return remote_root_ + "/" + rel;
}

std::string YandexDiskClient::build_remote_file_path(
    const std::string& remote_relative_dir,
    const std::string& display_name
) const {
    std::string dir = remote_relative_dir;

    if (dir.empty()) {
        dir = "/";
    }

    if (dir != "/" && !dir.empty() && dir.back() == '/') {
        dir.pop_back();
    }

    if (dir == "/") {
        return build_remote_path("/" + display_name);
    }

    return build_remote_path(dir + "/" + display_name);
}

YandexListResult YandexDiskClient::list_directory(const std::string& remote_relative_path) const {
    if (!configured()) {
        return {false, false, "Не задан YADISK_TOKEN.", {}};
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Не удалось инициализировать CURL.", {}};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;
    const std::string remote_path = build_remote_path(remote_relative_path);

    char* encoded_path = curl_easy_escape(curl, remote_path.c_str(), 0);
    std::string api_url =
        "https://cloud-api.yandex.net/v1/disk/resources"
        "?limit=1000"
        "&fields=type,name,path,size,mime_type,modified,_embedded.items.name,_embedded.items.path,_embedded.items.type,_embedded.items.size,_embedded.items.mime_type,_embedded.items.modified"
        "&path=";
    api_url += (encoded_path ? encoded_path : "");
    if (encoded_path) {
        curl_free(encoded_path);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, curl_easy_strerror(code), {}};
    }

    if (http_code == 404) {
        return {false, true, "Папка не найдена на Yandex Disk.", {}};
    }

    if (http_code < 200 || http_code >= 300) {
        std::ostringstream ss;
        ss << "Не удалось получить список файлов.\nHTTP: " << http_code << "\n\n" << response;
        return {false, false, ss.str(), {}};
    }

    GError* error = nullptr;
    JsonParser* parser = nullptr;
    if (!parse_json(response, &parser, &error)) {
        std::string msg = error ? error->message : "JSON parse error";
        if (error) g_error_free(error);
        return {false, false, msg, {}};
    }

    JsonNode* root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        return {false, false, "Некорректный JSON ответа.", {}};
    }

    JsonObject* root_obj = json_node_get_object(root);
    if (!json_object_has_member(root_obj, "_embedded")) {
        g_object_unref(parser);
        return {true, false, "", {}};
    }

    JsonObject* embedded = json_object_get_object_member(root_obj, "_embedded");
    if (!json_object_has_member(embedded, "items")) {
        g_object_unref(parser);
        return {true, false, "", {}};
    }

    JsonArray* items = json_object_get_array_member(embedded, "items");
    std::vector<FileItem> result;

    const guint len = json_array_get_length(items);
    result.reserve(len);

    for (guint i = 0; i < len; ++i) {
        JsonObject* item_obj = json_array_get_object_element(items, i);

        const std::string type = json_string_member(item_obj, "type");
        const std::string name = json_string_member(item_obj, "name");
        const std::string path = json_string_member(item_obj, "path");
        const std::string modified = json_string_member(item_obj, "modified");
        const std::string mime_type = json_string_member(item_obj, "mime_type");
        const std::uint64_t size = json_uint64_member(item_obj, "size");

        FileItem item;
        item.id = path;
        item.name = name;

        // path приходит как "disk:/CloudClient/..." → превращаем в "/..."
        std::string relative = path;
        const std::string prefix = remote_root_;
        if (!prefix.empty() && relative.rfind(prefix, 0) == 0) {
            relative.erase(0, prefix.size());
            if (relative.empty()) {
                relative = "/";
            }
        }
        if (relative.empty() || relative.front() != '/') {
            relative = "/" + relative;
        }

        item.path = relative;
        item.is_directory = (type == "dir");
        item.size = size;
        item.mime_type = file_type_to_mime(type, mime_type);
        item.modified_at = modified.empty() ? "unknown" : modified;
        item.is_cached = false;

        result.push_back(item);
    }

    g_object_unref(parser);
    return {true, false, "", result};
}

YandexResult YandexDiskClient::create_directory_if_needed(const std::string& remote_relative_dir) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    std::string dir = remote_relative_dir;
    if (dir.empty() || dir == "/") {
        dir = "/";
    }

    const std::string remote_dir_path = build_remote_path(dir);

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка создания папки", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    char* encoded_path = curl_easy_escape(curl, remote_dir_path.c_str(), 0);
    std::string api_url = "https://cloud-api.yandex.net/v1/disk/resources?path=";
    api_url += (encoded_path ? encoded_path : "");
    if (encoded_path) {
        curl_free(encoded_path);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка создания папки", curl_easy_strerror(code)};
    }

    if (http_code == 201 || http_code == 409) {
        std::ostringstream ss;
        ss << (http_code == 201 ? "Папка создана: " : "Папка уже существует: ") << remote_dir_path;
        return {true, false, "Папка готова", ss.str()};
    }

    std::ostringstream ss;
    ss << "Не удалось создать папку.\nHTTP: " << http_code << "\n\n" << response;
    return {false, false, "Ошибка создания папки", ss.str()};
}

YandexResult YandexDiskClient::create_directory(const std::string& remote_relative_path) const {
    return create_directory_if_needed(remote_relative_path);
}

YandexResult YandexDiskClient::upload_file(
    const std::string& local_path,
    const std::string& remote_relative_dir,
    const std::string& display_name
) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    const YandexResult dir_result = create_directory_if_needed(remote_relative_dir);
    if (!dir_result.success) {
        return dir_result;
    }

    std::ifstream in(local_path, std::ios::binary);
    if (!in.is_open()) {
        return {false, false, "Ошибка загрузки", "Не удалось прочитать локальный файл."};
    }
    std::ostringstream file_stream;
    file_stream << in.rdbuf();
    const std::string file_content = file_stream.str();

    const std::string remote_file_path = build_remote_file_path(remote_relative_dir, display_name);

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка загрузки", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    char* encoded_path = curl_easy_escape(curl, remote_file_path.c_str(), 0);
    std::string api_url = "https://cloud-api.yandex.net/v1/disk/resources/upload?overwrite=true&path=";
    api_url += (encoded_path ? encoded_path : "");
    if (encoded_path) {
        curl_free(encoded_path);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка загрузки", curl_easy_strerror(code)};
    }

    if (http_code < 200 || http_code >= 300) {
        std::ostringstream ss;
        ss << "Не удалось получить upload URL.\nHTTP: " << http_code << "\n\n" << response;
        return {false, false, "Ошибка загрузки", ss.str()};
    }

    GError* error = nullptr;
    JsonParser* parser = nullptr;
    if (!parse_json(response, &parser, &error)) {
        std::string msg = error ? error->message : "JSON parse error";
        if (error) g_error_free(error);
        return {false, false, "Ошибка загрузки", msg};
    }

    JsonObject* root_obj = json_node_get_object(json_parser_get_root(parser));
    const std::string href = json_string_member(root_obj, "href");
    g_object_unref(parser);

    if (href.empty()) {
        return {false, false, "Ошибка загрузки", "Yandex не вернул href для загрузки."};
    }

    curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка загрузки", "Не удалось инициализировать CURL для PUT."};
    }

    response.clear();
    curl_easy_setopt(curl, CURLOPT_URL, href.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, file_content.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(file_content.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    code = curl_easy_perform(curl);
    http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка загрузки", curl_easy_strerror(code)};
    }

    if (http_code >= 200 && http_code < 300) {
        std::ostringstream ss;
        ss << "Файл загружен в Yandex Disk.\n\n"
           << "Локальный файл:\n" << local_path << "\n\n"
           << "Удалённый путь:\n" << remote_file_path;
        return {true, false, "Загрузка в Yandex Disk", ss.str()};
    }

    std::ostringstream ss;
    ss << "Yandex Disk не принял файл.\nHTTP: " << http_code << "\n\n" << response;
    return {false, false, "Ошибка загрузки", ss.str()};
}

YandexResult YandexDiskClient::download_file(
    const std::string& remote_relative_path,
    const std::string& local_target_path
) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    const std::string remote_file_path = build_remote_path(remote_relative_path);

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка скачивания", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    char* encoded_path = curl_easy_escape(curl, remote_file_path.c_str(), 0);
    std::string api_url = "https://cloud-api.yandex.net/v1/disk/resources/download?path=";
    api_url += (encoded_path ? encoded_path : "");
    if (encoded_path) {
        curl_free(encoded_path);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка скачивания", curl_easy_strerror(code)};
    }

    if (http_code == 404) {
        return {false, true, "Ошибка скачивания", "Файл не найден на Yandex Disk."};
    }

    if (http_code < 200 || http_code >= 300) {
        std::ostringstream ss;
        ss << "Не удалось получить download URL.\nHTTP: " << http_code << "\n\n" << response;
        return {false, false, "Ошибка скачивания", ss.str()};
    }

    // --- Парсим href ---
    GError* error = nullptr;
    JsonParser* parser = nullptr;
    if (!parse_json(response, &parser, &error)) {
        std::string msg = error ? error->message : "JSON parse error";
        if (error) g_error_free(error);
        return {false, false, "Ошибка скачивания", msg};
    }

    JsonObject* root_obj = json_node_get_object(json_parser_get_root(parser));
    const std::string href = json_string_member(root_obj, "href");
    g_object_unref(parser);

    if (href.empty()) {
        return {false, false, "Ошибка скачивания", "Yandex не вернул href для скачивания."};
    }

    // --- Подготовка файла ---
    if (!ensure_parent_dirs_for_file(local_target_path)) {
        return {false, false, "Ошибка скачивания", "Не удалось создать локальные каталоги."};
    }

    FILE* out = std::fopen(local_target_path.c_str(), "wb");
    if (!out) {
        return {false, false, "Ошибка скачивания", "Не удалось открыть файл для записи."};
    }

    // --- СКАЧИВАНИЕ ---
    curl = curl_easy_init();
    if (!curl) {
        std::fclose(out);
        return {false, false, "Ошибка скачивания", "Не удалось инициализировать CURL."};
    }

    curl_easy_setopt(curl, CURLOPT_URL, href.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

    // 🔥 КЛЮЧЕВОЙ ФИКС
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    // иногда помогает стабильности
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "cloud-client/1.0");

    code = curl_easy_perform(curl);

    http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);
    std::fclose(out);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка скачивания", curl_easy_strerror(code)};
    }

    if (http_code >= 200 && http_code < 300) {
        return {
            true,
            false,
            "Скачивание",
            "Файл успешно скачан в кеш:\n" + local_target_path
        };
    }

    std::ostringstream ss;
    ss << "Yandex Disk не отдал файл.\nHTTP: " << http_code;
    return {false, false, "Ошибка скачивания", ss.str()};
}

YandexResult YandexDiskClient::delete_item(const std::string& remote_relative_path) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    const std::string remote_file_path = build_remote_path(remote_relative_path);

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка удаления", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    char* encoded_path = curl_easy_escape(curl, remote_file_path.c_str(), 0);
    std::string api_url = "https://cloud-api.yandex.net/v1/disk/resources?permanently=true&path=";
    api_url += (encoded_path ? encoded_path : "");
    if (encoded_path) {
        curl_free(encoded_path);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка удаления", curl_easy_strerror(code)};
    }

    if (http_code == 404) {
        return {false, true, "Удаление из Yandex Disk", "Объект уже отсутствует на Yandex Disk."};
    }

    if (http_code == 204 || http_code == 202) {
        std::ostringstream ss;
        ss << "Объект удалён из Yandex Disk.\n\n"
           << "Удалённый путь:\n" << remote_file_path;
        return {true, false, "Удаление из Yandex Disk", ss.str()};
    }

    std::ostringstream ss;
    ss << "Yandex Disk не удалил объект.\nHTTP: " << http_code << "\n\n" << response;
    return {false, false, "Ошибка удаления", ss.str()};
}

YandexResult YandexDiskClient::rename_item(
    const std::string& old_remote_relative_path,
    const std::string& new_remote_relative_path
) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка переименования", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    const std::string old_remote = build_remote_path(old_remote_relative_path);
    const std::string new_remote = build_remote_path(new_remote_relative_path);

    char* encoded_from = curl_easy_escape(curl, old_remote.c_str(), 0);
    char* encoded_to = curl_easy_escape(curl, new_remote.c_str(), 0);

    std::string api_url =
        "https://cloud-api.yandex.net/v1/disk/resources/move?overwrite=true&from=";
    api_url += (encoded_from ? encoded_from : "");
    api_url += "&path=";
    api_url += (encoded_to ? encoded_to : "");

    if (encoded_from) curl_free(encoded_from);
    if (encoded_to) curl_free(encoded_to);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка переименования", curl_easy_strerror(code)};
    }

    if (http_code == 201 || http_code == 202) {
        std::ostringstream ss;
        ss << "Объект переименован на Yandex Disk.\n\n"
           << "Старый путь:\n" << old_remote << "\n\n"
           << "Новый путь:\n" << new_remote;
        return {true, false, "Переименование в Yandex Disk", ss.str()};
    }

    if (http_code == 404) {
        return {false, true, "Ошибка переименования", "Объект не найден на Yandex Disk."};
    }

    std::ostringstream ss;
    ss << "Yandex Disk не переименовал объект.\nHTTP: " << http_code << "\n\n" << response;
    return {false, false, "Ошибка переименования", ss.str()};
}

YandexResult YandexDiskClient::move_item(
    const std::string& old_remote_relative_path,
    const std::string& new_remote_relative_path
) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка перемещения", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    const std::string old_remote = build_remote_path(old_remote_relative_path);
    const std::string new_remote = build_remote_path(new_remote_relative_path);

    char* encoded_from = curl_easy_escape(curl, old_remote.c_str(), 0);
    char* encoded_to = curl_easy_escape(curl, new_remote.c_str(), 0);

    std::string api_url =
        "https://cloud-api.yandex.net/v1/disk/resources/move?overwrite=true&from=";
    api_url += (encoded_from ? encoded_from : "");
    api_url += "&path=";
    api_url += (encoded_to ? encoded_to : "");

    if (encoded_from) {
        curl_free(encoded_from);
    }
    if (encoded_to) {
        curl_free(encoded_to);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка перемещения", curl_easy_strerror(code)};
    }

    if (http_code == 201 || http_code == 202) {
        std::ostringstream ss;
        ss << "Объект перемещён.\n\n"
           << "Старый путь:\n" << old_remote << "\n\n"
           << "Новый путь:\n" << new_remote;
        return {true, false, "Перемещение", ss.str()};
    }

    if (http_code == 404) {
        return {false, true, "Ошибка перемещения", "Объект не найден на Yandex Disk."};
    }

    std::ostringstream ss;
    ss << "Не удалось переместить объект.\nHTTP: " << http_code << "\n\n" << response;
    return {false, false, "Ошибка перемещения", ss.str()};
}

YandexResult YandexDiskClient::copy_item(
    const std::string& old_remote_relative_path,
    const std::string& new_remote_relative_path
) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка копирования", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    const std::string old_remote = build_remote_path(old_remote_relative_path);
    const std::string new_remote = build_remote_path(new_remote_relative_path);

    char* encoded_from = curl_easy_escape(curl, old_remote.c_str(), 0);
    char* encoded_to = curl_easy_escape(curl, new_remote.c_str(), 0);

    std::string api_url =
        "https://cloud-api.yandex.net/v1/disk/resources/copy?overwrite=true&from=";
    api_url += (encoded_from ? encoded_from : "");
    api_url += "&path=";
    api_url += (encoded_to ? encoded_to : "");

    if (encoded_from) {
        curl_free(encoded_from);
    }
    if (encoded_to) {
        curl_free(encoded_to);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка копирования", curl_easy_strerror(code)};
    }

    if (http_code == 201 || http_code == 202) {
        std::ostringstream ss;
        ss << "Объект скопирован.\n\n"
           << "Исходный путь:\n" << old_remote << "\n\n"
           << "Новый путь:\n" << new_remote;
        return {true, false, "Копирование", ss.str()};
    }

    if (http_code == 404) {
        return {false, true, "Ошибка копирования", "Объект не найден на Yandex Disk."};
    }

    std::ostringstream ss;
    ss << "Не удалось скопировать объект.\nHTTP: " << http_code << "\n\n" << response;
    return {false, false, "Ошибка копирования", ss.str()};
}

YandexResult YandexDiskClient::create_share_link(const std::string& remote_relative_path) const {
    if (!configured()) {
        return {false, false, "Yandex Disk", "Не задан YADISK_TOKEN."};
    }

    const std::string remote_file_path = build_remote_path(remote_relative_path);

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, false, "Ошибка шаринга", "Не удалось инициализировать CURL."};
    }

    std::string response;
    const std::string auth_header = "Authorization: OAuth " + oauth_token_;

    char* encoded_path = curl_easy_escape(curl, remote_file_path.c_str(), 0);
    std::string publish_url = "https://cloud-api.yandex.net/v1/disk/resources/publish?path=";
    publish_url += (encoded_path ? encoded_path : "");
    if (encoded_path) {
        curl_free(encoded_path);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, publish_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode code = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (code != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {false, false, "Ошибка шаринга", curl_easy_strerror(code)};
    }

    if (!(http_code >= 200 && http_code < 300)) {
        std::ostringstream ss;
        ss << "Не удалось опубликовать ресурс.\nHTTP: " << http_code << "\n\n" << response;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {false, false, "Ошибка шаринга", ss.str()};
    }

    response.clear();
    encoded_path = curl_easy_escape(curl, remote_file_path.c_str(), 0);
    std::string meta_url = "https://cloud-api.yandex.net/v1/disk/resources?fields=public_url&path=";
    meta_url += (encoded_path ? encoded_path : "");
    if (encoded_path) {
        curl_free(encoded_path);
    }

    curl_easy_setopt(curl, CURLOPT_URL, meta_url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    code = curl_easy_perform(curl);
    http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return {false, false, "Ошибка шаринга", curl_easy_strerror(code)};
    }

    if (!(http_code >= 200 && http_code < 300)) {
        std::ostringstream ss;
        ss << "Не удалось получить public_url.\nHTTP: " << http_code << "\n\n" << response;
        return {false, false, "Ошибка шаринга", ss.str()};
    }

    GError* error = nullptr;
    JsonParser* parser = nullptr;
    if (!parse_json(response, &parser, &error)) {
        std::string msg = error ? error->message : "JSON parse error";
        if (error) g_error_free(error);
        return {false, false, "Ошибка шаринга", msg};
    }

    JsonObject* root_obj = json_node_get_object(json_parser_get_root(parser));
    const std::string public_url = json_string_member(root_obj, "public_url");
    g_object_unref(parser);

    if (public_url.empty()) {
        return {false, false, "Ошибка шаринга", "Yandex Disk не вернул public_url."};
    }

    return {true, false, "Ссылка готова", public_url};
}