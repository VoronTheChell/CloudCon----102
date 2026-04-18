#include "TaskManager.h"

TaskManager::TaskManager(CloudProvider* provider)
    : provider_(provider) {
}

OperationResult TaskManager::execute(const OperationRequest& request) {
    if (provider_ == nullptr) {
        return {
            false,
            "Ошибка",
            "CloudProvider не инициализирован."
        };
    }

    switch (request.type) {
        case OperationType::Upload:
            return provider_->upload_file(
                request.local_path,
                request.remote_path,
                request.display_name
            );

        case OperationType::Download:
            return provider_->download_file(
                request.remote_path,
                request.display_name
            );

        case OperationType::Delete:
            return provider_->delete_item(
                request.remote_path,
                request.display_name,
                request.is_directory
            );

        case OperationType::Share:
            return provider_->create_share_link(
                request.remote_path,
                request.display_name
            );

        case OperationType::Open:
            return provider_->open_file(
                request.remote_path,
                request.display_name
            );

        default:
            return {
                false,
                "Ошибка",
                "Неизвестный тип операции."
            };
    }
}