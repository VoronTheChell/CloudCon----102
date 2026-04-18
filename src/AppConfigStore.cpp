#include "AppConfigStore.h"

#include <json-glib/json-glib.h>

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::string json_member_or_default(JsonObject* object, const char* name, const std::string& fallback = {}) {
    if (!json_object_has_member(object, name)) {
        return fallback;
    }
    return json_object_get_string_member(object, name);
}

std::string resolve_config_base_dir() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return xdg;
    }

    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::string(home) + "/.config";
    }

    return ".";
}

} // namespace

std::string AppConfigStore::config_path() const {
    return resolve_config_base_dir() + "/cloud-client/auth.json";
}

bool AppConfigStore::load(AppConfig& out_config) const {
    const std::string path = config_path();
    if (!fs::exists(path)) {
        out_config = AppConfig{};
        return true;
    }

    GError* error = nullptr;
    JsonParser* parser = json_parser_new();
    const gboolean ok = json_parser_load_from_file(parser, path.c_str(), &error);
    if (!ok) {
        if (error) {
            g_error_free(error);
        }
        g_object_unref(parser);
        return false;
    }

    JsonNode* root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        return false;
    }

    JsonObject* object = json_node_get_object(root);

    AppConfig loaded;
    loaded.client_id = json_member_or_default(object, "client_id");
    loaded.client_secret = json_member_or_default(object, "client_secret");
    loaded.access_token = json_member_or_default(object, "access_token");
    loaded.refresh_token = json_member_or_default(object, "refresh_token");
    loaded.remote_root = json_member_or_default(object, "remote_root", "disk:/CloudClient");
    if (loaded.remote_root.empty()) {
        loaded.remote_root = "disk:/CloudClient";
    }

    out_config = loaded;
    g_object_unref(parser);
    return true;
}

bool AppConfigStore::save(const AppConfig& config) const {
    const fs::path path = config_path();

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }

    JsonBuilder* builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "client_id");
    json_builder_add_string_value(builder, config.client_id.c_str());

    json_builder_set_member_name(builder, "client_secret");
    json_builder_add_string_value(builder, config.client_secret.c_str());

    json_builder_set_member_name(builder, "access_token");
    json_builder_add_string_value(builder, config.access_token.c_str());

    json_builder_set_member_name(builder, "refresh_token");
    json_builder_add_string_value(builder, config.refresh_token.c_str());

    json_builder_set_member_name(builder, "remote_root");
    json_builder_add_string_value(builder, config.remote_root.c_str());

    json_builder_end_object(builder);

    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    json_generator_set_pretty(generator, TRUE);

    GError* error = nullptr;
    const gboolean ok = json_generator_to_file(generator, path.c_str(), &error);
    if (error) {
        g_error_free(error);
    }

    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return ok == TRUE;
}
