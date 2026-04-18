#include "MetadataStore.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

MetadataStore::MetadataStore() {
    const char* home = std::getenv("HOME");

    fs::path base;
    if (home != nullptr) {
        base = fs::path(home) / ".local" / "share" / "cloud-client";
    } else {
        base = fs::temp_directory_path() / "cloud-client";
    }

    std::error_code ec;
    fs::create_directories(base, ec);
    db_path_ = (base / "metadata.db").string();
}

MetadataStore::~MetadataStore() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

bool MetadataStore::initialize() {
    return open_db() && create_tables();
}

bool MetadataStore::open_db() {
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "MetadataStore SQLite open error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return true;
}

bool MetadataStore::create_tables() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS metadata_entries ("
        "remote_path TEXT PRIMARY KEY,"
        "parent_path TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "is_directory INTEGER NOT NULL,"
        "size INTEGER NOT NULL,"
        "mime_type TEXT,"
        "modified_at TEXT,"
        "is_cached INTEGER NOT NULL DEFAULT 0,"
        "sync_state INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_metadata_parent_path ON metadata_entries(parent_path);";

    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "MetadataStore SQL error: " << err << std::endl;
        sqlite3_free(err);
        return false;
    }

    return true;
}

int MetadataStore::sync_state_to_int(SyncState state) {
    return static_cast<int>(state);
}

SyncState MetadataStore::int_to_sync_state(int value) {
    switch (value) {
        case 0: return SyncState::Synced;
        case 1: return SyncState::PendingUpload;
        case 2: return SyncState::PendingDelete;
        case 3: return SyncState::PendingMove;
        case 4: return SyncState::PendingRename;
        case 5: return SyncState::PendingCreateDir;
        case 6: return SyncState::Conflict;
        default: return SyncState::Synced;
    }
}

bool MetadataStore::upsert_entry(const MetadataEntry& entry) {
    const char* sql =
        "INSERT INTO metadata_entries ("
        "remote_path, parent_path, name, is_directory, size, mime_type, modified_at, is_cached, sync_state"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(remote_path) DO UPDATE SET "
        "parent_path=excluded.parent_path, "
        "name=excluded.name, "
        "is_directory=excluded.is_directory, "
        "size=excluded.size, "
        "mime_type=excluded.mime_type, "
        "modified_at=excluded.modified_at, "
        "is_cached=excluded.is_cached, "
        "sync_state=excluded.sync_state;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, entry.remote_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry.parent_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, entry.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, entry.is_directory ? 1 : 0);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(entry.size));
    sqlite3_bind_text(stmt, 6, entry.mime_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, entry.modified_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, entry.is_cached ? 1 : 0);
    sqlite3_bind_int(stmt, 9, sync_state_to_int(entry.sync_state));

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::upsert_entries(const std::vector<MetadataEntry>& entries) {
    char* err = nullptr;
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }

    bool ok = true;
    for (const auto& entry : entries) {
        if (!upsert_entry(entry)) {
            ok = false;
            break;
        }
    }

    if (ok) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }

    return ok;
}

std::vector<MetadataEntry> MetadataStore::list_directory(const std::string& parent_path) const {
    std::vector<MetadataEntry> result;

    const char* sql =
        "SELECT remote_path, parent_path, name, is_directory, size, mime_type, modified_at, is_cached, sync_state "
        "FROM metadata_entries "
        "WHERE parent_path = ? "
        "ORDER BY is_directory DESC, name COLLATE NOCASE ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, parent_path.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MetadataEntry entry;
        entry.remote_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        entry.parent_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        entry.is_directory = sqlite3_column_int(stmt, 3) != 0;
        entry.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));

        const unsigned char* mime = sqlite3_column_text(stmt, 5);
        const unsigned char* modified = sqlite3_column_text(stmt, 6);

        entry.mime_type = mime ? reinterpret_cast<const char*>(mime) : "";
        entry.modified_at = modified ? reinterpret_cast<const char*>(modified) : "";
        entry.is_cached = sqlite3_column_int(stmt, 7) != 0;
        entry.sync_state = int_to_sync_state(sqlite3_column_int(stmt, 8));

        result.push_back(entry);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool MetadataStore::get_entry(const std::string& remote_path, MetadataEntry& out_entry) const {
    const char* sql =
        "SELECT remote_path, parent_path, name, is_directory, size, mime_type, modified_at, is_cached, sync_state "
        "FROM metadata_entries WHERE remote_path = ? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, remote_path.c_str(), -1, SQLITE_TRANSIENT);

    const bool found = sqlite3_step(stmt) == SQLITE_ROW;
    if (found) {
        out_entry.remote_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out_entry.parent_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out_entry.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        out_entry.is_directory = sqlite3_column_int(stmt, 3) != 0;
        out_entry.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));

        const unsigned char* mime = sqlite3_column_text(stmt, 5);
        const unsigned char* modified = sqlite3_column_text(stmt, 6);

        out_entry.mime_type = mime ? reinterpret_cast<const char*>(mime) : "";
        out_entry.modified_at = modified ? reinterpret_cast<const char*>(modified) : "";
        out_entry.is_cached = sqlite3_column_int(stmt, 7) != 0;
        out_entry.sync_state = int_to_sync_state(sqlite3_column_int(stmt, 8));
    }

    sqlite3_finalize(stmt);
    return found;
}

bool MetadataStore::remove_entry(const std::string& remote_path) {
    const char* sql = "DELETE FROM metadata_entries WHERE remote_path = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, remote_path.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::remove_subtree(const std::string& remote_path) {
    const char* sql =
        "DELETE FROM metadata_entries "
        "WHERE remote_path = ? OR remote_path LIKE ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    const std::string like_path = remote_path + "/%";

    sqlite3_bind_text(stmt, 1, remote_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, like_path.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::mark_cached(const std::string& remote_path, bool is_cached) {
    const char* sql = "UPDATE metadata_entries SET is_cached = ? WHERE remote_path = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, is_cached ? 1 : 0);
    sqlite3_bind_text(stmt, 2, remote_path.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::update_sync_state(const std::string& remote_path, SyncState state) {
    const char* sql = "UPDATE metadata_entries SET sync_state = ? WHERE remote_path = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, sync_state_to_int(state));
    sqlite3_bind_text(stmt, 2, remote_path.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::clear_directory_snapshot(const std::string& parent_path) {
    const char* sql = "DELETE FROM metadata_entries WHERE parent_path = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, parent_path.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::string MetadataStore::db_path() const {
    return db_path_;
}