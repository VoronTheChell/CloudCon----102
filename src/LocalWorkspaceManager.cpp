#include "LocalWorkspaceManager.h"

#include <cstdlib>
#include <fstream>
#include <system_error>

LocalWorkspaceManager::LocalWorkspaceManager() {
    const char* home = std::getenv("HOME");

    if (home != nullptr) {
        root_ = std::filesystem::path(home) / "CloudClient" / "YandexDisk";
    } else {
        root_ = std::filesystem::temp_directory_path() / "CloudClient" / "YandexDisk";
    }
}

bool LocalWorkspaceManager::initialize() {
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    return !ec;
}

std::string LocalWorkspaceManager::root_path() const {
    return root_.string();
}

std::filesystem::path LocalWorkspaceManager::relative_path_from_remote(const std::string& remote_path) const {
    if (remote_path.empty() || remote_path == "/") {
        return {};
    }

    std::string normalized = remote_path;
    while (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
    }

    return std::filesystem::path(normalized);
}

std::string LocalWorkspaceManager::local_path_for_remote(const std::string& remote_path) const {
    return (root_ / relative_path_from_remote(remote_path)).string();
}

bool LocalWorkspaceManager::ensure_parent_dirs_for_remote(const std::string& remote_path) const {
    const auto full_path = root_ / relative_path_from_remote(remote_path);
    const auto parent = full_path.parent_path();

    std::error_code ec;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    return !ec;
}

bool LocalWorkspaceManager::write_file(const std::string& remote_path, const std::string& content) const {
    if (!ensure_parent_dirs_for_remote(remote_path)) {
        return false;
    }

    const auto full_path = root_ / relative_path_from_remote(remote_path);
    std::ofstream out(full_path, std::ios::binary);

    if (!out.is_open()) {
        return false;
    }

    out << content;
    return true;
}

bool LocalWorkspaceManager::exists(const std::string& remote_path) const {
    return std::filesystem::exists(root_ / relative_path_from_remote(remote_path));
}