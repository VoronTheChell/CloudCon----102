#pragma once

#include <memory>
#include <string>
#include <vector>

#include "AppConfigStore.h"
#include "CacheManager.h"
#include "ClipboardManager.h"
#include "FileItem.h"
#include "MetadataStore.h"
#include "NotificationManager.h"
#include "PendingOperationsStore.h"
#include "SystemIntegration.h"
#include "YandexDiskClient.h"
#include "YandexOAuthClient.h"

class MainWindow;

class AppController {
public:
    explicit AppController(MainWindow* window);

    void initialize();

    void select_index(int index);
    void activate_selected();

    void request_open_selected();
    void request_upload();
    void request_download();
    void request_delete();
    void request_delete_from_cache();
    void request_share();
    void request_refresh();
    void request_open_cache_folder();

    void request_copy_selected();
    void request_paste_into_current();

    void navigate_up();

    void handle_local_file_chosen(const std::string& local_path, const std::string& display_name);

    void handle_create_folder_named(const std::string& folder_name);
    void handle_rename_selected(const std::string& new_name);
    void handle_copy_selected(const std::string& target_dir);
    void handle_move_selected(const std::string& target_dir);

    bool open_yandex_authorization_page(const std::string& client_id);
    bool connect_with_token(
        const std::string& access_token,
        const std::string& remote_root,
        const std::string& client_id = "",
        const std::string& client_secret = ""
    );
    bool connect_with_confirmation_code(
        const std::string& client_id,
        const std::string& client_secret,
        const std::string& code,
        const std::string& remote_root
    );

    bool handle_drop_move_to_directory(const std::string& source_path, const std::string& target_directory);
    std::string ensure_local_export_path(const std::string& remote_path);

    void confirm_delete(bool confirmed);

private:
    MainWindow* window_ {};

    std::unique_ptr<YandexDiskClient> yandex_client_;

    std::vector<FileItem> current_files_;
    std::string current_path_ {"/"};
    int selected_index_ {-1};
    bool online_ {false};
    std::string last_connection_error_;

    AppConfigStore config_store_;
    AppConfig config_;
    YandexOAuthClient oauth_client_;
    CacheManager cache_manager_;
    MetadataStore metadata_store_;
    PendingOperationsStore pending_store_;
    SystemIntegration system_integration_;
    NotificationManager notification_manager_;
    ClipboardManager clipboard_manager_;

    const FileItem* selected_item() const;

    void load_files(bool prefer_remote);
    bool check_online();

    void apply_cache_flags();
    void persist_current_directory_snapshot();
    MetadataEntry metadata_from_file_item(const FileItem& item) const;
    std::string parent_path_of(const std::string& remote_path) const;

    bool queue_upload(const std::string& local_path, const std::string& remote_dir, const std::string& display_name);
    bool ensure_cached_file(const FileItem& item, std::string& out_path);
    void rebuild_yandex_client();
    bool refresh_access_token_if_possible();
    bool apply_connection(const AppConfig& new_config, bool show_success_dialog, const std::string& success_detail);
};