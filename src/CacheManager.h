#pragma once

#include <filesystem>
#include <string>
#include <sqlite3.h>

class CacheManager {
public:
    CacheManager();
    ~CacheManager();

    bool initialize();

    bool is_cached(const std::string& remote_path);
    std::string cached_file_path(const std::string& remote_path);

    bool put_file(const std::string& remote_path, const std::string& content);
    bool get_file(const std::string& remote_path, std::string& out_content);
    bool remove_file(const std::string& remote_path);

    std::string cache_root() const;

private:
    std::filesystem::path cache_root_;
    sqlite3* db_ {nullptr};

    bool open_db();
    bool create_tables();

    std::string sanitize_remote_path(const std::string& remote_path) const;
};