#pragma once

#include <filesystem>
#include <string>

class LocalWorkspaceManager {
public:
    LocalWorkspaceManager();

    bool initialize();
    std::string root_path() const;

    std::string local_path_for_remote(const std::string& remote_path) const;
    bool ensure_parent_dirs_for_remote(const std::string& remote_path) const;
    bool write_file(const std::string& remote_path, const std::string& content) const;
    bool exists(const std::string& remote_path) const;

private:
    std::filesystem::path root_;

    std::filesystem::path relative_path_from_remote(const std::string& remote_path) const;
};