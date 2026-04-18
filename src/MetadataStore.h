#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sqlite3.h>

#include "FileItem.h"

enum class SyncState {
    Synced = 0,
    PendingUpload = 1,
    PendingDelete = 2,
    PendingMove = 3,
    PendingRename = 4,
    PendingCreateDir = 5,
    Conflict = 6
};

struct MetadataEntry {
    std::string remote_path;
    std::string parent_path;
    std::string name;
    bool is_directory {false};
    std::uint64_t size {0};
    std::string mime_type;
    std::string modified_at;
    bool is_cached {false};
    SyncState sync_state {SyncState::Synced};
};

class MetadataStore {
public:
    MetadataStore();
    ~MetadataStore();

    bool initialize();

    bool upsert_entry(const MetadataEntry& entry);
    bool upsert_entries(const std::vector<MetadataEntry>& entries);

    std::vector<MetadataEntry> list_directory(const std::string& parent_path) const;
    bool get_entry(const std::string& remote_path, MetadataEntry& out_entry) const;

    bool remove_entry(const std::string& remote_path);
    bool remove_subtree(const std::string& remote_path);

    bool mark_cached(const std::string& remote_path, bool is_cached);
    bool update_sync_state(const std::string& remote_path, SyncState state);

    bool clear_directory_snapshot(const std::string& parent_path);

    std::string db_path() const;

private:
    std::string db_path_;
    sqlite3* db_ {nullptr};

    bool open_db();
    bool create_tables();

    static int sync_state_to_int(SyncState state);
    static SyncState int_to_sync_state(int value);
};  