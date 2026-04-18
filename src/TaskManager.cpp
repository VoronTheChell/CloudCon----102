#include "TaskManager.h"

#include <sstream>

OperationResult TaskManager::execute(const OperationRequest& request) {
    OperationResult result;
    result.success = true;

    std::ostringstream ss;

    switch (request.type) {
        case OperationType::Upload:
            result.title = "Загрузка";
            ss << "Mock-загрузка файла \"" << request.display_name << "\"\n\n"
               << "Локальный путь:\n" << request.local_path << "\n\n"
               << "Удалённый путь:\n" << request.remote_path;
            result.message = ss.str();
            break;

        case OperationType::Download:
            result.title = "Скачивание";
            ss << "Mock-скачивание файла \"" << request.display_name << "\"\n\n"
               << "Удалённый путь:\n" << request.remote_path;
            result.message = ss.str();
            break;

        case OperationType::Delete:
            result.title = "Удаление";
            ss << "Mock-удаление объекта \"" << request.display_name << "\"\n\n"
               << "Путь:\n" << request.remote_path;
            result.message = ss.str();
            break;

        case OperationType::Share:
            result.title = "Ссылка создана";
            ss << "https://mock-share.local/share?path=" << request.remote_path;
            result.message = ss.str();
            break;

        case OperationType::Open:
            result.title = "Открытие";
            ss << "Mock-открытие объекта \"" << request.display_name << "\"\n\n"
               << "Путь:\n" << request.remote_path;
            result.message = ss.str();
            break;

        default:
            result.success = false;
            result.title = "Ошибка";
            result.message = "Неизвестный тип операции.";
            break;
    }

    return result;
}