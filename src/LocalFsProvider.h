#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "CloudProvider.h"

class LocalFsProvider final : public CloudProvider {
public:
    explicit LocalFsProvider(const std::string& root_path);

    std::vector<FileItem> list_files(const std::string& path) override;

    OperationResult upload_file(
        const std::string& local_path,
        const std::string& remote_path,
        const std::string& display_name
    ) override;

    OperationResult download_file(
        const std::string& remote_path,
        const std::string& display_name
    ) override;

    OperationResult delete_item(
        const std::string& remote_path,
        const std::string& display_name,
        bool is_directory
    ) override;

    OperationResult create_share_link(
        const std::string& remote_path,
        const std::string& display_name
    ) override;

    OperationResult open_file(
        const std::string& remote_path,
        const std::string& display_name
    ) override;

private:
    std::string root_path_;

    std::string absolute_path_from_remote(const std::string& remote_path) const;
    std::string remote_path_from_absolute(const std::string& absolute_path) const;
    std::string detect_mime_type(const std::filesystem::path& path, bool is_directory) const;
};