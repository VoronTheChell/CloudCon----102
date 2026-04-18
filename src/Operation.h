#pragma once

#include <string>

enum class OperationType {
    Upload,
    Download,
    Delete,
    Share,
    Open
};

struct OperationRequest {
    OperationType type;
    std::string local_path;
    std::string remote_path;
    std::string display_name;
    bool is_directory {false};
};

struct OperationResult {
    bool success {false};
    std::string title;
    std::string message;
};