#pragma once

#include <string>
#include <vector>

#include "FileItem.h"

struct YandexResult {
    bool success {false};
    bool not_found {false};
    std::string title;
    std::string message;
};

struct YandexListResult {
    bool success {false};
    bool not_found {false};
    std::string message;
    std::vector<FileItem> items;
};

class YandexDiskClient {
public:
    YandexDiskClient(std::string oauth_token, std::string remote_root);

    bool configured() const;

    YandexListResult list_directory(const std::string& remote_relative_path) const;

    YandexResult create_directory(const std::string& remote_relative_path) const;
    YandexResult upload_file(
        const std::string& local_path,
        const std::string& remote_relative_dir,
        const std::string& display_name
    ) const;
    YandexResult download_file(
        const std::string& remote_relative_path,
        const std::string& local_target_path
    ) const;
    YandexResult delete_item(const std::string& remote_relative_path) const;

    YandexResult rename_item(
        const std::string& old_remote_relative_path,
        const std::string& new_remote_relative_path
    ) const;

    YandexResult move_item(
        const std::string& old_remote_relative_path,
        const std::string& new_remote_relative_path
    ) const;

    YandexResult copy_item(
        const std::string& old_remote_relative_path,
        const std::string& new_remote_relative_path
    ) const;

    YandexResult create_share_link(const std::string& remote_relative_path) const;

private:
    std::string oauth_token_;
    std::string remote_root_;

    std::string build_remote_path(const std::string& remote_relative_path) const;
    std::string build_remote_file_path(
        const std::string& remote_relative_dir,
        const std::string& display_name
    ) const;

    YandexResult create_directory_if_needed(const std::string& remote_relative_dir) const;
};