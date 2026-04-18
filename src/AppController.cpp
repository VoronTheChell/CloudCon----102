#include "AppController.h"
#include "MainWindow.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {
bool str_ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}
}

AppController::AppController(MainWindow* window)
    : window_(window) {
}

void AppController::initialize() {
    if (!cache_manager_.initialize()) {
        window_->set_status("Ошибка инициализации кеша");
        return;
    }

    if (!metadata_store_.initialize()) {
        window_->set_status("Ошибка инициализации локальной базы метаданных");
        return;
    }

    if (!pending_store_.initialize()) {
        window_->set_status("Ошибка инициализации очереди операций");
        return;
    }

    notification_manager_.initialize("cloud-client");

    const char* token = std::getenv("YADISK_TOKEN");
    const char* remote_root = std::getenv("YADISK_REMOTE_ROOT");

    yandex_client_ = std::make_unique<YandexDiskClient>(
        token ? token : "",
        remote_root ? remote_root : "disk:/CloudClient"
    );

    load_files(true);
}

const FileItem* AppController::selected_item() const {
    if (selected_index_ < 0 || static_cast<std::size_t>(selected_index_) >= current_files_.size()) {
        return nullptr;
    }

    return &current_files_[static_cast<std::size_t>(selected_index_)];
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
    ss << "Выбрано: " << item.name;
    if (!item.is_directory) {
        ss << (item.is_cached ? " • кеширован" : " • не кеширован");
    }
    window_->set_status(ss.str());
}

bool AppController::check_online() {
    if (!yandex_client_ || !yandex_client_->configured()) {
        online_ = false;
        return false;
    }

    const YandexListResult result = yandex_client_->list_directory("/");
    online_ = result.success;
    return online_;
}

std::string AppController::parent_path_of(const std::string& remote_path) const {
    fs::path p(remote_path);
    std::string parent = p.parent_path().generic_string();
    return parent.empty() ? "/" : parent;
}

MetadataEntry AppController::metadata_from_file_item(const FileItem& item) const {
    MetadataEntry entry;
    entry.remote_path = item.path;
    entry.parent_path = parent_path_of(item.path);
    entry.name = item.name;
    entry.is_directory = item.is_directory;
    entry.size = item.size;
    entry.mime_type = item.mime_type;
    entry.modified_at = item.modified_at;
    entry.is_cached = item.is_cached;
    entry.sync_state = SyncState::Synced;
    return entry;
}

void AppController::persist_current_directory_snapshot() {
    metadata_store_.clear_directory_snapshot(current_path_);

    std::vector<MetadataEntry> entries;
    entries.reserve(current_files_.size());

    for (const auto& item : current_files_) {
        entries.push_back(metadata_from_file_item(item));
    }

    metadata_store_.upsert_entries(entries);
}

void AppController::apply_cache_flags() {
    for (auto& item : current_files_) {
        if (!item.is_directory) {
            item.is_cached = cache_manager_.is_cached(item.path);
            metadata_store_.mark_cached(item.path, item.is_cached);
        }
    }
}

bool AppController::ensure_cached_file(const FileItem& item, std::string& out_path) {
    if (item.is_directory) {
        return false;
    }

    out_path = cache_manager_.cached_file_path(item.path);

    if (cache_manager_.is_cached(item.path)) {
        return true;
    }

    if (!online_ || !yandex_client_) {
        return false;
    }

    const YandexResult result = yandex_client_->download_file(item.path, out_path);
    if (!result.success) {
        return false;
    }

    std::ifstream in(out_path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();

    cache_manager_.put_file(item.path, ss.str());
    metadata_store_.mark_cached(item.path, true);

    return true;
}

void AppController::load_files(bool prefer_remote) {
    check_online();

    if (prefer_remote && online_) {
        const YandexListResult result = yandex_client_->list_directory(current_path_);

        if (result.success) {
            current_files_ = result.items;
            apply_cache_flags();

            for (auto& item : current_files_) {
                if (item.is_directory) {
                    continue;
                }

                const bool looks_like_image =
                    item.mime_type.rfind("image/", 0) == 0 ||
                    str_ends_with(item.path, ".png") ||
                    str_ends_with(item.path, ".jpg") ||
                    str_ends_with(item.path, ".jpeg") ||
                    str_ends_with(item.path, ".webp") ||
                    str_ends_with(item.path, ".bmp") ||
                    str_ends_with(item.path, ".gif");

                if (!looks_like_image) {
                    continue;
                }

                if (cache_manager_.is_cached(item.path)) {
                    item.is_cached = true;
                    continue;
                }

                if (item.size > 15 * 1024 * 1024) {
                    continue;
                }

                std::string local_path;
                if (ensure_cached_file(item, local_path)) {
                    item.is_cached = true;
                }
            }

            apply_cache_flags();
            persist_current_directory_snapshot();
        } else if (result.not_found) {
            current_path_ = "/";
            window_->show_info_dialog("Папка не найдена", "Текущая папка не существует на диске. Возврат в корень.");

            const YandexListResult root_result = yandex_client_->list_directory(current_path_);
            if (root_result.success) {
                current_files_ = root_result.items;
                apply_cache_flags();

                for (auto& item : current_files_) {
                    if (item.is_directory) {
                        continue;
                    }

                    const bool looks_like_image =
                        item.mime_type.rfind("image/", 0) == 0 ||
                        str_ends_with(item.path, ".png") ||
                        str_ends_with(item.path, ".jpg") ||
                        str_ends_with(item.path, ".jpeg") ||
                        str_ends_with(item.path, ".webp") ||
                        str_ends_with(item.path, ".bmp") ||
                        str_ends_with(item.path, ".gif");

                    if (!looks_like_image) {
                        continue;
                    }

                    if (cache_manager_.is_cached(item.path)) {
                        item.is_cached = true;
                        continue;
                    }

                    if (item.size > 15 * 1024 * 1024) {
                        continue;
                    }

                    std::string local_path;
                    if (ensure_cached_file(item, local_path)) {
                        item.is_cached = true;
                    }
                }

                apply_cache_flags();
                persist_current_directory_snapshot();
            } else {
                current_files_.clear();
            }
        } else {
            window_->show_info_dialog("Ошибка", result.message);
            current_files_.clear();
        }
    } else {
        current_files_.clear();

        const std::vector<MetadataEntry> entries = metadata_store_.list_directory(current_path_);
        current_files_.reserve(entries.size());

        for (const auto& entry : entries) {
            FileItem item;
            item.id = entry.remote_path;
            item.name = entry.name;
            item.path = entry.remote_path;
            item.is_directory = entry.is_directory;
            item.size = entry.size;
            item.mime_type = entry.mime_type;
            item.modified_at = entry.modified_at;
            item.is_cached = entry.is_cached;
            current_files_.push_back(item);
        }
    }

    selected_index_ = -1;
    window_->set_files(current_files_);
    window_->set_path(current_path_);

    std::ostringstream ss;
    ss << "Статус: " << (online_ ? "онлайн" : "офлайн")
       << " • объектов: " << current_files_.size();

    const auto pending = pending_store_.list_pending();
    if (!pending.empty()) {
        ss << " • в очереди: " << pending.size();
    }

    window_->set_status(ss.str());
}

void AppController::navigate_up() {
    if (current_path_ == "/") {
        return;
    }

    const std::size_t pos = current_path_.find_last_of('/');
    if (pos == std::string::npos || pos == 0) {
        current_path_ = "/";
    } else {
        current_path_ = current_path_.substr(0, pos);
    }

    load_files(true);
}

void AppController::activate_selected() {
    const FileItem* item = selected_item();
    if (!item) {
        return;
    }

    if (item->is_directory) {
        current_path_ = item->path;
        load_files(true);
        return;
    }

    request_download();
}

void AppController::request_open_selected() {
    activate_selected();
}

void AppController::request_upload() {
    window_->open_file_dialog();
}

bool AppController::queue_upload(const std::string& local_path, const std::string& remote_dir, const std::string& display_name) {
    std::string remote_path = (remote_dir == "/") ? "/" + display_name : remote_dir + "/" + display_name;

    MetadataEntry entry;
    entry.remote_path = remote_path;
    entry.parent_path = parent_path_of(remote_path);
    entry.name = display_name;
    entry.is_directory = false;
    entry.size = 0;
    entry.mime_type = "application/octet-stream";
    entry.modified_at = "offline";
    entry.is_cached = true;
    entry.sync_state = SyncState::PendingUpload;

    bool ok1 = metadata_store_.upsert_entry(entry);
    bool ok2 = pending_store_.enqueue(
        PendingOperationType::UploadFile,
        remote_path,
        "",
        local_path
    );

    return ok1 && ok2;
}

void AppController::handle_local_file_chosen(const std::string& local_path, const std::string& display_name) {
    if (online_) {
        const YandexResult result = yandex_client_->upload_file(local_path, current_path_, display_name);
        window_->show_info_dialog(result.title, result.message);

        if (result.success) {
            notification_manager_.show_success("Файл загружен", "Файл \"" + display_name + "\" загружен.");
        } else {
            notification_manager_.show_error("Ошибка загрузки", "Не удалось загрузить \"" + display_name + "\".");
        }

        load_files(true);
        return;
    }

    if (queue_upload(local_path, current_path_, display_name)) {
        notification_manager_.show_info("Офлайн режим", "Файл \"" + display_name + "\" добавлен в очередь.");
        window_->show_info_dialog("Офлайн загрузка", "Файл поставлен в очередь на загрузку.");
        load_files(false);
        return;
    }

    window_->show_info_dialog("Ошибка", "Не удалось добавить файл в очередь.");
    notification_manager_.show_error("Ошибка", "Не удалось добавить файл в очередь.");
}

void AppController::request_download() {
    const FileItem* item = selected_item();
    if (!item || item->is_directory) {
        return;
    }

    std::string target;
    if (!ensure_cached_file(*item, target)) {
        window_->show_info_dialog("Офлайн", "Файл не закеширован и недоступен без интернета.");
        return;
    }

    notification_manager_.show_success("Файл доступен", "Файл \"" + item->name + "\" сохранён в кеш.");

    if (!system_integration_.open_file(target)) {
        window_->show_info_dialog("Ошибка открытия", "Не удалось открыть файл.");
    }

    load_files(true);
}

std::string AppController::ensure_local_export_path(const std::string& remote_path) {
    for (const auto& item : current_files_) {
        if (item.path == remote_path && !item.is_directory) {
            std::string local_path;
            if (ensure_cached_file(item, local_path)) {
                return local_path;
            }
            return {};
        }
    }

    MetadataEntry entry {};
    if (!metadata_store_.get_entry(remote_path, entry) || entry.is_directory) {
        return {};
    }

    FileItem item;
    item.id = entry.remote_path;
    item.name = entry.name;
    item.path = entry.remote_path;
    item.is_directory = entry.is_directory;
    item.size = entry.size;
    item.mime_type = entry.mime_type;
    item.modified_at = entry.modified_at;
    item.is_cached = entry.is_cached;

    std::string local_path;
    if (ensure_cached_file(item, local_path)) {
        return local_path;
    }

    return {};
}

void AppController::request_delete() {
    const FileItem* item = selected_item();
    if (!item) {
        return;
    }

    std::ostringstream detail;
    detail << "Путь: " << item->path << "\n\n";
    detail << (online_ ? "Объект будет удалён с Яндекс.Диска." : "Сейчас офлайн. Удаление будет поставлено в очередь.");

    window_->show_delete_confirmation(item->name, detail.str());
}

void AppController::confirm_delete(bool confirmed) {
    if (!confirmed) {
        return;
    }

    const FileItem* item = selected_item();
    if (!item) {
        return;
    }

    if (online_) {
        const YandexResult result = yandex_client_->delete_item(item->path);
        window_->show_info_dialog(result.title, result.message);

        if (result.success) {
            notification_manager_.show_info("Удаление", "\"" + item->name + "\" удалён.");
        } else {
            notification_manager_.show_error("Ошибка удаления", "Не удалось удалить \"" + item->name + "\".");
        }

        load_files(true);
        return;
    }

    const bool removed = item->is_directory
        ? metadata_store_.remove_subtree(item->path)
        : metadata_store_.remove_entry(item->path);

    const bool queued = pending_store_.enqueue(
        PendingOperationType::DeleteItem,
        item->path,
        "",
        ""
    );

    if (removed && queued) {
        notification_manager_.show_info("Офлайн режим", "Удаление поставлено в очередь.");
        load_files(false);
        return;
    }

    notification_manager_.show_error("Ошибка", "Не удалось поставить удаление в очередь.");
}

void AppController::request_delete_from_cache() {
    const FileItem* item = selected_item();
    if (!item || item->is_directory) {
        return;
    }

    if (!cache_manager_.is_cached(item->path)) {
        window_->show_info_dialog("Кеш", "Файл не закеширован.");
        return;
    }

    if (cache_manager_.remove_file(item->path)) {
        metadata_store_.mark_cached(item->path, false);
        notification_manager_.show_info("Кеш", "Файл \"" + item->name + "\" удалён из кеша.");
    } else {
        notification_manager_.show_error("Кеш", "Не удалось удалить файл из кеша.");
    }

    load_files(false);
}

void AppController::request_share() {
    const FileItem* item = selected_item();
    if (!item) {
        return;
    }

    if (!online_) {
        window_->show_info_dialog("Офлайн", "Поделиться ссылкой можно только онлайн.");
        return;
    }

    const YandexResult result = yandex_client_->create_share_link(item->path);

    if (result.success) {
        window_->copy_text_to_clipboard(result.message);
        notification_manager_.show_success("Ссылка", "Ссылка скопирована в буфер обмена.");
        window_->show_info_dialog("Ссылка скопирована", result.message);
        return;
    }

    window_->show_info_dialog(result.title, result.message);
    notification_manager_.show_error("Ошибка", "Не удалось получить ссылку.");
}

void AppController::request_refresh() {
    load_files(true);
}

void AppController::request_open_cache_folder() {
    if (!system_integration_.open_directory(cache_manager_.cache_root())) {
        window_->show_info_dialog("Ошибка", "Не удалось открыть папку кеша.");
    }
}

void AppController::request_copy_selected() {
    const FileItem* item = selected_item();
    if (!item) {
        window_->show_info_dialog("Копирование", "Сначала выбери файл или папку.");
        return;
    }

    clipboard_manager_.set_copy({item->path});
    notification_manager_.show_info("Копирование", "Объект \"" + item->name + "\" помещён в буфер.");
    window_->set_status("Статус: объект скопирован в буфер");
}

void AppController::request_paste_into_current() {
    if (!clipboard_manager_.has_data()) {
        window_->show_info_dialog("Вставка", "Буфер копирования пуст.");
        return;
    }

    if (clipboard_manager_.mode() != ClipboardManager::Mode::Copy) {
        window_->show_info_dialog("Вставка", "Поддерживается только режим копирования.");
        return;
    }

    bool pasted_any = false;

    for (const auto& source_path : clipboard_manager_.paths()) {
        MetadataEntry entry {};
        FileItem temp_item;
        const FileItem* source_item = nullptr;

        for (const auto& item : current_files_) {
            if (item.path == source_path) {
                source_item = &item;
                break;
            }
        }

        if (source_item == nullptr) {
            if (!metadata_store_.get_entry(source_path, entry)) {
                continue;
            }

            temp_item.id = entry.remote_path;
            temp_item.name = entry.name;
            temp_item.path = entry.remote_path;
            temp_item.is_directory = entry.is_directory;
            temp_item.size = entry.size;
            temp_item.mime_type = entry.mime_type;
            temp_item.modified_at = entry.modified_at;
            temp_item.is_cached = entry.is_cached;
            source_item = &temp_item;
        }

        std::string target_path =
            (current_path_ == "/")
                ? "/" + source_item->name
                : current_path_ + "/" + source_item->name;

        if (target_path == source_item->path) {
            continue;
        }

        if (online_) {
            const YandexResult result = yandex_client_->copy_item(source_item->path, target_path);
            if (result.success) {
                pasted_any = true;
            }
        } else {
            MetadataEntry copy_entry;
            copy_entry.remote_path = target_path;
            copy_entry.parent_path = parent_path_of(target_path);
            copy_entry.name = source_item->name;
            copy_entry.is_directory = source_item->is_directory;
            copy_entry.size = source_item->size;
            copy_entry.mime_type = source_item->mime_type;
            copy_entry.modified_at = "offline";
            copy_entry.is_cached = false;
            copy_entry.sync_state = SyncState::PendingMove;

            const bool upserted = metadata_store_.upsert_entry(copy_entry);
            const bool queued = pending_store_.enqueue(
                PendingOperationType::CopyItem,
                source_item->path,
                target_path,
                ""
            );

            if (upserted && queued) {
                pasted_any = true;
            }
        }
    }

    if (pasted_any) {
        notification_manager_.show_success("Вставка", "Копирование выполнено.");
    } else {
        notification_manager_.show_error("Вставка", "Не удалось вставить объект.");
    }

    load_files(true);
}

void AppController::handle_create_folder_named(const std::string& folder_name) {
    if (folder_name.empty()) {
        window_->show_info_dialog("Ошибка", "Имя папки не может быть пустым.");
        return;
    }

    std::string remote_path = (current_path_ == "/") ? "/" + folder_name : current_path_ + "/" + folder_name;

    if (online_) {
        const YandexResult result = yandex_client_->create_directory(remote_path);
        window_->show_info_dialog(result.title, result.message);

        if (result.success) {
            notification_manager_.show_success("Папка", "Папка \"" + folder_name + "\" создана.");
        } else {
            notification_manager_.show_error("Ошибка", "Не удалось создать папку.");
        }

        load_files(true);
        return;
    }

    MetadataEntry entry;
    entry.remote_path = remote_path;
    entry.parent_path = parent_path_of(remote_path);
    entry.name = folder_name;
    entry.is_directory = true;
    entry.size = 0;
    entry.mime_type = "inode/directory";
    entry.modified_at = "offline";
    entry.is_cached = false;
    entry.sync_state = SyncState::PendingCreateDir;

    bool ok1 = metadata_store_.upsert_entry(entry);
    bool ok2 = pending_store_.enqueue(
        PendingOperationType::CreateDir,
        remote_path,
        "",
        ""
    );

    if (ok1 && ok2) {
        notification_manager_.show_info("Офлайн режим", "Папка \"" + folder_name + "\" добавлена в очередь.");
        load_files(false);
        return;
    }

    notification_manager_.show_error("Ошибка", "Не удалось поставить создание папки в очередь.");
}

void AppController::handle_rename_selected(const std::string& new_name) {
    const FileItem* item = selected_item();
    if (!item || new_name.empty()) {
        return;
    }

    fs::path old_remote_path(item->path);
    std::string parent_remote = old_remote_path.parent_path().generic_string();
    if (parent_remote.empty()) {
        parent_remote = "/";
    }

    std::string new_remote_path = (parent_remote == "/") ? "/" + new_name : parent_remote + "/" + new_name;

    if (online_) {
        const YandexResult result = yandex_client_->rename_item(item->path, new_remote_path);
        window_->show_info_dialog(result.title, result.message);

        if (result.success) {
            if (!item->is_directory && cache_manager_.is_cached(item->path)) {
                std::string cached_content;
                if (cache_manager_.get_file(item->path, cached_content)) {
                    cache_manager_.put_file(new_remote_path, cached_content);
                    cache_manager_.remove_file(item->path);
                }
            }
            notification_manager_.show_success("Переименование", "Объект переименован.");
        } else {
            notification_manager_.show_error("Ошибка", "Не удалось переименовать объект.");
        }

        load_files(true);
        return;
    }

    MetadataEntry entry;
    entry.remote_path = new_remote_path;
    entry.parent_path = parent_path_of(new_remote_path);
    entry.name = new_name;
    entry.is_directory = item->is_directory;
    entry.size = item->size;
    entry.mime_type = item->mime_type;
    entry.modified_at = "offline";
    entry.is_cached = item->is_cached;
    entry.sync_state = SyncState::PendingRename;

    bool removed = item->is_directory
        ? metadata_store_.remove_subtree(item->path)
        : metadata_store_.remove_entry(item->path);

    bool upserted = metadata_store_.upsert_entry(entry);
    bool queued = pending_store_.enqueue(
        PendingOperationType::RenameItem,
        item->path,
        new_remote_path,
        ""
    );

    if (removed && upserted && queued) {
        if (!item->is_directory && cache_manager_.is_cached(item->path)) {
            std::string cached_content;
            if (cache_manager_.get_file(item->path, cached_content)) {
                cache_manager_.put_file(new_remote_path, cached_content);
                cache_manager_.remove_file(item->path);
                metadata_store_.mark_cached(new_remote_path, true);
            }
        }

        notification_manager_.show_info("Офлайн режим", "Переименование поставлено в очередь.");
        load_files(false);
        return;
    }

    notification_manager_.show_error("Ошибка", "Не удалось поставить переименование в очередь.");
}

void AppController::handle_copy_selected(const std::string& target_dir) {
    const FileItem* item = selected_item();
    if (!item || target_dir.empty()) {
        return;
    }

    std::string normalized = target_dir;
    if (normalized.front() != '/') {
        normalized = "/" + normalized;
    }
    if (normalized.size() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }

    std::string new_remote_path = (normalized == "/")
        ? "/" + item->name
        : normalized + "/" + item->name;

    if (online_) {
        const YandexResult result = yandex_client_->copy_item(item->path, new_remote_path);
        window_->show_info_dialog(result.title, result.message);

        if (result.success) {
            notification_manager_.show_success("Копирование", "Объект скопирован.");
        } else {
            notification_manager_.show_error("Ошибка", "Не удалось скопировать объект.");
        }

        load_files(true);
        return;
    }

    MetadataEntry entry;
    entry.remote_path = new_remote_path;
    entry.parent_path = parent_path_of(new_remote_path);
    entry.name = fs::path(new_remote_path).filename().string();
    entry.is_directory = item->is_directory;
    entry.size = item->size;
    entry.mime_type = item->mime_type;
    entry.modified_at = "offline";
    entry.is_cached = false;
    entry.sync_state = SyncState::PendingMove;

    bool upserted = metadata_store_.upsert_entry(entry);
    bool queued = pending_store_.enqueue(
        PendingOperationType::CopyItem,
        item->path,
        new_remote_path,
        ""
    );

    if (upserted && queued) {
        notification_manager_.show_info("Офлайн режим", "Копирование поставлено в очередь.");
        load_files(false);
        return;
    }

    notification_manager_.show_error("Ошибка", "Не удалось поставить копирование в очередь.");
}

void AppController::handle_move_selected(const std::string& target_dir) {
    const FileItem* item = selected_item();
    if (!item || target_dir.empty()) {
        return;
    }

    std::string normalized = target_dir;
    if (normalized.front() != '/') {
        normalized = "/" + normalized;
    }
    if (normalized.size() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }

    std::string new_remote_path = (normalized == "/")
        ? "/" + item->name
        : normalized + "/" + item->name;

    if (online_) {
        const YandexResult result = yandex_client_->move_item(item->path, new_remote_path);
        window_->show_info_dialog(result.title, result.message);

        if (result.success) {
            if (!item->is_directory && cache_manager_.is_cached(item->path)) {
                std::string cached_content;
                if (cache_manager_.get_file(item->path, cached_content)) {
                    cache_manager_.put_file(new_remote_path, cached_content);
                    cache_manager_.remove_file(item->path);
                }
            }
            notification_manager_.show_success("Перемещение", "Объект перемещён.");
        } else {
            notification_manager_.show_error("Ошибка", "Не удалось переместить объект.");
        }

        load_files(true);
        return;
    }

    MetadataEntry entry;
    entry.remote_path = new_remote_path;
    entry.parent_path = parent_path_of(new_remote_path);
    entry.name = fs::path(new_remote_path).filename().string();
    entry.is_directory = item->is_directory;
    entry.size = item->size;
    entry.mime_type = item->mime_type;
    entry.modified_at = "offline";
    entry.is_cached = item->is_cached;
    entry.sync_state = SyncState::PendingMove;

    bool removed = item->is_directory
        ? metadata_store_.remove_subtree(item->path)
        : metadata_store_.remove_entry(item->path);

    bool upserted = metadata_store_.upsert_entry(entry);
    bool queued = pending_store_.enqueue(
        PendingOperationType::MoveItem,
        item->path,
        new_remote_path,
        ""
    );

    if (removed && upserted && queued) {
        if (!item->is_directory && cache_manager_.is_cached(item->path)) {
            std::string cached_content;
            if (cache_manager_.get_file(item->path, cached_content)) {
                cache_manager_.put_file(new_remote_path, cached_content);
                cache_manager_.remove_file(item->path);
                metadata_store_.mark_cached(new_remote_path, true);
            }
        }

        notification_manager_.show_info("Офлайн режим", "Перемещение поставлено в очередь.");
        load_files(false);
        return;
    }

    notification_manager_.show_error("Ошибка", "Не удалось поставить перемещение в очередь.");
}

bool AppController::handle_drop_move_to_directory(const std::string& source_path, const std::string& target_directory) {
    if (source_path.empty() || target_directory.empty()) {
        return false;
    }

    if (source_path == target_directory) {
        return false;
    }

    const FileItem* source_item = nullptr;
    int source_index = -1;

    for (std::size_t i = 0; i < current_files_.size(); ++i) {
        if (current_files_[i].path == source_path) {
            source_item = &current_files_[i];
            source_index = static_cast<int>(i);
            break;
        }
    }

    if (source_item == nullptr) {
        MetadataEntry entry {};
        if (!metadata_store_.get_entry(source_path, entry)) {
            return false;
        }
        return false;
    }

    std::string normalized_target = target_directory;
    if (normalized_target.back() == '/' && normalized_target.size() > 1) {
        normalized_target.pop_back();
    }

    const std::string new_target_path =
        (normalized_target == "/")
            ? "/" + source_item->name
            : normalized_target + "/" + source_item->name;

    if (new_target_path == source_item->path) {
        return false;
    }

    selected_index_ = source_index;
    handle_move_selected(normalized_target);
    return true;
}