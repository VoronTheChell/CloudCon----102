#pragma once

#include <string>
#include <vector>

#include "FileItem.h"
#include "Operation.h"

class CloudProvider {
public:
    virtual ~CloudProvider() = default;

    virtual std::vector<FileItem> list_files(const std::string& path) = 0;

    virtual OperationResult upload_file(
        const std::string& local_path,
        const std::string& remote_path,
        const std::string& display_name
    ) = 0;

    virtual OperationResult download_file(
        const std::string& remote_path,
        const std::string& display_name
    ) = 0;

    virtual OperationResult delete_item(
        const std::string& remote_path,
        const std::string& display_name,
        bool is_directory
    ) = 0;

    virtual OperationResult create_share_link(
        const std::string& remote_path,
        const std::string& display_name
    ) = 0;

    virtual OperationResult open_file(
        const std::string& remote_path,
        const std::string& display_name
    ) = 0;
};