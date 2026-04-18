#include "AppController.h"
#include "MainWindow.h"
#include "MockCloudProvider.h"

#include <sstream>

AppController::AppController(MainWindow* window)
    : window_(window),
      provider_(std::make_unique<MockCloudProvider>()) {
}

void AppController::initialize() {
    load_files();
}

const std::vector<FileItem>& AppController::files() const {
    return current_files_;
}

const std::string& AppController::current_path() const {
    return current_path_;
}

void AppController::load_files() {
    current_files_ = provider_->list_files(current_path_);
    selected_index_ = -1;

    window_->set_files(current_files_);
    window_->set_path(current_path_);

    std::ostringstream ss;
    ss << "Статус: открыт путь " << current_path_ << ", объектов — " << current_files_.size();
    window_->set_status(ss.str());
}

void AppController::navigate_to(const std::string& path) {
    current_path_ = path;
    load_files();
}

void AppController::navigate_up() {
    if (current_path_ == "/") {
        window_->set_status("Статус: уже открыт корневой каталог");
        return;
    }

    const std::size_t pos = current_path_.find_last_of('/');
    if (pos == std::string::npos || pos == 0) {
        current_path_ = "/";
    } else {
        current_path_ = current_path_.substr(0, pos);
    }

    load_files();
}

void AppController::select_index(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= current_files_.size()) {
        selected_index_ = -1;
        window_->set_status("Статус: ничего не выбрано");
        return;
    }

    selected_index_ = index;
    const FileItem& item = current_files_[static_cast<std::size_t>(index)];

    std::ostringstream ss;
    ss << "Выбрано: " << item.name << " | путь: " << item.path;
    window_->set_status(ss.str());
}

const FileItem* AppController::selected_item() const {
    if (selected_index_ < 0 || static_cast<std::size_t>(selected_index_) >= current_files_.size()) {
        return nullptr;
    }

    return &current_files_[static_cast<std::size_t>(selected_index_)];
}

void AppController::show_result(const OperationResult& result) {
    window_->set_status("Статус: " + result.title);
    window_->show_info_dialog(result.title, result.message);
}

void AppController::activate_selected() {
    const FileItem* item = selected_item();
    if (item == nullptr) {
        window_->set_status("Статус: сначала выбери объект");
        return;
    }

    if (item->is_directory) {
        navigate_to(item->path);
        return;
    }

    OperationRequest request;
    request.type = OperationType::Open;
    request.remote_path = item->path;
    request.display_name = item->name;
    request.is_directory = item->is_directory;

    show_result(task_manager_.execute(request));
}

void AppController::request_upload() {
    window_->open_file_dialog();
}

void AppController::handle_local_file_chosen(const std::string& local_path, const std::string& display_name) {
    OperationRequest request;
    request.type = OperationType::Upload;
    request.local_path = local_path;
    request.remote_path = current_path_;
    request.display_name = display_name;

    show_result(task_manager_.execute(request));
}

void AppController::request_download() {
    const FileItem* item = selected_item();
    if (item == nullptr) {
        window_->set_status("Статус: выбери файл для скачивания");
        return;
    }

    if (item->is_directory) {
        window_->set_status("Статус: папку нельзя скачать как файл");
        window_->show_info_dialog("Скачивание недоступно", "Для mock-версии можно скачивать только файлы.");
        return;
    }

    OperationRequest request;
    request.type = OperationType::Download;
    request.remote_path = item->path;
    request.display_name = item->name;
    request.is_directory = item->is_directory;

    show_result(task_manager_.execute(request));
}

void AppController::request_delete() {
    const FileItem* item = selected_item();
    if (item == nullptr) {
        window_->set_status("Статус: выбери объект для удаления");
        return;
    }

    std::ostringstream detail;
    detail << "Путь: " << item->path << "\n\n"
           << "Подтверди удаление объекта.";

    window_->show_delete_confirmation(item->name, detail.str());
}

void AppController::confirm_delete(bool confirmed) {
    if (!confirmed) {
        window_->set_status("Статус: удаление отменено");
        return;
    }

    const FileItem* item = selected_item();
    if (item == nullptr) {
        window_->set_status("Статус: объект для удаления не найден");
        return;
    }

    OperationRequest request;
    request.type = OperationType::Delete;
    request.remote_path = item->path;
    request.display_name = item->name;
    request.is_directory = item->is_directory;

    show_result(task_manager_.execute(request));
}

void AppController::request_share() {
    const FileItem* item = selected_item();
    if (item == nullptr) {
        window_->set_status("Статус: выбери объект для шаринга");
        return;
    }

    OperationRequest request;
    request.type = OperationType::Share;
    request.remote_path = item->path;
    request.display_name = item->name;
    request.is_directory = item->is_directory;

    show_result(task_manager_.execute(request));
}