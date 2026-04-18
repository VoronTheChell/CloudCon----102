#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>

enum class PendingOperationType {
    UploadFile = 0,
    DeleteItem = 1,
    CreateDir = 2,
    RenameItem = 3,
    MoveItem = 4,
    CopyItem = 5
};

enum class PendingOperationStatus {
    Pending = 0,
    InProgress = 1,
    Failed = 2
};

struct PendingOperation {
    int id {0};
    PendingOperationType type {PendingOperationType::UploadFile};
    std::string source_path;
    std::string target_path;
    std::string local_path;
    std::string created_at;
    PendingOperationStatus status {PendingOperationStatus::Pending};
    int retry_count {0};
    std::string last_error;
};

class PendingOperationsStore {
public:
    PendingOperationsStore();
    ~PendingOperationsStore();

    bool initialize();

    bool enqueue(
        PendingOperationType type,
        const std::string& source_path,
        const std::string& target_path,
        const std::string& local_path = ""
    );

    std::vector<PendingOperation> list_pending() const;
    bool mark_in_progress(int id);
    bool mark_failed(int id, const std::string& error_message);
    bool remove(int id);

    std::string db_path() const;

private:
    std::string db_path_;
    sqlite3* db_ {nullptr};

    bool open_db();
    bool create_tables();

    static int type_to_int(PendingOperationType type);
    static PendingOperationType int_to_type(int value);
    static int status_to_int(PendingOperationStatus status);
    static PendingOperationStatus int_to_status(int value);
};