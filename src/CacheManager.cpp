#include "CacheManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

CacheManager::CacheManager() {
    const char* home = std::getenv("HOME");

    if (home) {
        cache_root_ = std::filesystem::path(home) / ".cache" / "cloud-client";
    } else {
        cache_root_ = std::filesystem::temp_directory_path() / "cloud-client";
    }
}

CacheManager::~CacheManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool CacheManager::initialize() {
    std::filesystem::create_directories(cache_root_);
    return open_db() && create_tables();
}

bool CacheManager::open_db() {
    const std::string db_path = (cache_root_ / "cache.db").string();

    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "SQLite error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return true;
}

bool CacheManager::create_tables() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS cache_files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "remote_path TEXT UNIQUE,"
        "local_path TEXT,"
        "cached_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    char* err = nullptr;

    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "SQL error: " << err << std::endl;
        sqlite3_free(err);
        return false;
    }

    return true;
}

std::string CacheManager::sanitize_remote_path(const std::string& remote_path) const {
    std::string result = remote_path;

    for (char& c : result) {
        if (c == '/' || c == '\\') c = '_';
    }

    if (!result.empty() && result.front() == '_') {
        result.erase(result.begin());
    }

    if (result.empty()) result = "root";
    return result;
}

std::string CacheManager::cached_file_path(const std::string& remote_path) {
    return (cache_root_ / sanitize_remote_path(remote_path)).string();
}

bool CacheManager::is_cached(const std::string& remote_path) {
    const char* sql = "SELECT COUNT(*) FROM cache_files WHERE remote_path = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, remote_path.c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (count <= 0) {
        return false;
    }

    return std::filesystem::exists(cached_file_path(remote_path));
}

bool CacheManager::put_file(const std::string& remote_path, const std::string& content) {
    const std::string path = cached_file_path(remote_path);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out << content;
    out.close();

    const char* sql =
        "INSERT OR REPLACE INTO cache_files (remote_path, local_path) VALUES (?, ?);";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, remote_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_STATIC);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return ok;
}

bool CacheManager::get_file(const std::string& remote_path, std::string& out_content) {
    const std::string path = cached_file_path(remote_path);

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    std::ostringstream ss;
    ss << in.rdbuf();

    out_content = ss.str();
    return true;
}

bool CacheManager::remove_file(const std::string& remote_path) {
    const std::string path = cached_file_path(remote_path);

    std::error_code ec;
    std::filesystem::remove(path, ec);

    const char* sql = "DELETE FROM cache_files WHERE remote_path = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, remote_path.c_str(), -1, SQLITE_STATIC);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return ok;
}

std::string CacheManager::cache_root() const {
    return cache_root_.string();
}