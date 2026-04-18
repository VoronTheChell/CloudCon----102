#include "PendingOperationsStore.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

PendingOperationsStore::PendingOperationsStore() {
    const char* home = std::getenv("HOME");

    fs::path base;
    if (home != nullptr) {
        base = fs::path(home) / ".local" / "share" / "cloud-client";
    } else {
        base = fs::temp_directory_path() / "cloud-client";
    }

    std::error_code ec;
    fs::create_directories(base, ec);
    db_path_ = (base / "pending_ops.db").string();
}

PendingOperationsStore::~PendingOperationsStore() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

bool PendingOperationsStore::initialize() {
    return open_db() && create_tables();
}

bool PendingOperationsStore::open_db() {
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "PendingOperationsStore SQLite open error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return true;
}

bool PendingOperationsStore::create_tables() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS pending_operations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "operation_type INTEGER NOT NULL,"
        "source_path TEXT,"
        "target_path TEXT,"
        "local_path TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "status INTEGER NOT NULL DEFAULT 0,"
        "retry_count INTEGER NOT NULL DEFAULT 0,"
        "last_error TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_pending_status ON pending_operations(status);";

    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "PendingOperationsStore SQL error: " << err << std::endl;
        sqlite3_free(err);
        return false;
    }

    return true;
}

int PendingOperationsStore::type_to_int(PendingOperationType type) {
    return static_cast<int>(type);
}

PendingOperationType PendingOperationsStore::int_to_type(int value) {
    switch (value) {
        case 0: return PendingOperationType::UploadFile;
        case 1: return PendingOperationType::DeleteItem;
        case 2: return PendingOperationType::CreateDir;
        case 3: return PendingOperationType::RenameItem;
        case 4: return PendingOperationType::MoveItem;
        case 5: return PendingOperationType::CopyItem;
        default: return PendingOperationType::UploadFile;
    }
}

int PendingOperationsStore::status_to_int(PendingOperationStatus status) {
    return static_cast<int>(status);
}

PendingOperationStatus PendingOperationsStore::int_to_status(int value) {
    switch (value) {
        case 0: return PendingOperationStatus::Pending;
        case 1: return PendingOperationStatus::InProgress;
        case 2: return PendingOperationStatus::Failed;
        default: return PendingOperationStatus::Pending;
    }
}

bool PendingOperationsStore::enqueue(
    PendingOperationType type,
    const std::string& source_path,
    const std::string& target_path,
    const std::string& local_path
) {
    const char* sql =
        "INSERT INTO pending_operations "
        "(operation_type, source_path, target_path, local_path, status, retry_count, last_error) "
        "VALUES (?, ?, ?, ?, ?, 0, '');";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, type_to_int(type));
    sqlite3_bind_text(stmt, 2, source_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, target_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, local_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, status_to_int(PendingOperationStatus::Pending));

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<PendingOperation> PendingOperationsStore::list_pending() const {
    std::vector<PendingOperation> result;

    const char* sql =
        "SELECT id, operation_type, source_path, target_path, local_path, created_at, status, retry_count, last_error "
        "FROM pending_operations "
        "WHERE status IN (0, 2) "
        "ORDER BY id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PendingOperation op;
        op.id = sqlite3_column_int(stmt, 0);
        op.type = int_to_type(sqlite3_column_int(stmt, 1));

        const unsigned char* source = sqlite3_column_text(stmt, 2);
        const unsigned char* target = sqlite3_column_text(stmt, 3);
        const unsigned char* local = sqlite3_column_text(stmt, 4);
        const unsigned char* created = sqlite3_column_text(stmt, 5);
        const unsigned char* last_error = sqlite3_column_text(stmt, 8);

        op.source_path = source ? reinterpret_cast<const char*>(source) : "";
        op.target_path = target ? reinterpret_cast<const char*>(target) : "";
        op.local_path = local ? reinterpret_cast<const char*>(local) : "";
        op.created_at = created ? reinterpret_cast<const char*>(created) : "";
        op.status = int_to_status(sqlite3_column_int(stmt, 6));
        op.retry_count = sqlite3_column_int(stmt, 7);
        op.last_error = last_error ? reinterpret_cast<const char*>(last_error) : "";

        result.push_back(op);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool PendingOperationsStore::mark_in_progress(int id) {
    const char* sql =
        "UPDATE pending_operations "
        "SET status = ?, last_error = '' "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, status_to_int(PendingOperationStatus::InProgress));
    sqlite3_bind_int(stmt, 2, id);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool PendingOperationsStore::mark_failed(int id, const std::string& error_message) {
    const char* sql =
        "UPDATE pending_operations "
        "SET status = ?, retry_count = retry_count + 1, last_error = ? "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, status_to_int(PendingOperationStatus::Failed));
    sqlite3_bind_text(stmt, 2, error_message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, id);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool PendingOperationsStore::remove(int id) {
    const char* sql = "DELETE FROM pending_operations WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::string PendingOperationsStore::db_path() const {
    return db_path_;
}