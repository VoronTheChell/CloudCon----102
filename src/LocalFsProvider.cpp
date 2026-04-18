#include "LocalFsProvider.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::string format_time(const fs::file_time_type& file_time) {
    try {
        const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );

        const std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm {};

#if defined(__unix__)
        localtime_r(&cftime, &tm);
#else
        tm = *std::localtime(&cftime);
#endif

        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M");
        return ss.str();
    } catch (...) {
        return "unknown";
    }
}

} // namespace

LocalFsProvider::LocalFsProvider(const std::string& root_path)
    : root_path_(root_path) {
}

std::string LocalFsProvider::absolute_path_from_remote(const std::string& remote_path) const {
    if (remote_path.empty() || remote_path == "/") {
        return root_path_;
    }

    std::string normalized = remote_path;
    while (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
    }

    return (fs::path(root_path_) / normalized).string();
}

std::string LocalFsProvider::remote_path_from_absolute(const std::string& absolute_path) const {
    const fs::path abs = fs::absolute(absolute_path);
    const fs::path root = fs::absolute(root_path_);

    std::error_code ec;
    const fs::path rel = fs::relative(abs, root, ec);

    if (ec || rel.empty() || rel == ".") {
        return "/";
    }

    return "/" + rel.generic_string();
}

std::string LocalFsProvider::detect_mime_type(const fs::path& path, bool is_directory) const {
    if (is_directory) {
        return "inode/directory";
    }

    const std::string ext = path.extension().string();

    if (ext == ".txt") return "text/plain";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".zip") return "application/zip";
    if (ext == ".docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == ".pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";

    return "application/octet-stream";
}

std::vector<FileItem> LocalFsProvider::list_files(const std::string& path) {
    std::vector<FileItem> items;

    const fs::path absolute_dir = absolute_path_from_remote(path);

    std::error_code ec;
    if (!fs::exists(absolute_dir, ec) || !fs::is_directory(absolute_dir, ec)) {
        return items;
    }

    for (const auto& entry : fs::directory_iterator(absolute_dir, ec)) {
        if (ec) {
            break;
        }

        const fs::path entry_path = entry.path();
        const bool is_dir = entry.is_directory(ec);

        std::uint64_t size = 0;
        if (!is_dir) {
            size = static_cast<std::uint64_t>(entry.file_size(ec));
        }

        FileItem item;
        item.id = entry_path.string();
        item.name = entry_path.filename().string();
        item.path = remote_path_from_absolute(entry_path.string());
        item.is_directory = is_dir;
        item.size = size;
        item.mime_type = detect_mime_type(entry_path, is_dir);
        item.modified_at = format_time(entry.last_write_time(ec));
        item.is_cached = true;

        items.push_back(item);
    }

    std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
        if (a.is_directory != b.is_directory) {
            return a.is_directory > b.is_directory;
        }
        return a.name < b.name;
    });

    return items;
}

OperationResult LocalFsProvider::upload_file(
    const std::string& local_path,
    const std::string& remote_path,
    const std::string& display_name
) {
    OperationResult result;
    result.success = true;
    result.title = "Загрузка";

    std::ostringstream ss;
    ss << "Файл выбран для будущей синхронизации.\n\n"
       << "Локальный файл:\n" << local_path << "\n\n"
       << "Целевая папка:\n" << remote_path << "\n\n"
       << "Имя:\n" << display_name;

    result.message = ss.str();
    return result;
}

OperationResult LocalFsProvider::download_file(
    const std::string& remote_path,
    const std::string& display_name
) {
    OperationResult result;
    result.success = true;
    result.title = "Скачивание";

    std::ostringstream ss;
    ss << "Локальный файл уже доступен в рабочей папке.\n\n"
       << "Файл:\n" << display_name << "\n\n"
       << "Путь:\n" << absolute_path_from_remote(remote_path);

    result.message = ss.str();
    return result;
}

OperationResult LocalFsProvider::delete_item(
    const std::string& remote_path,
    const std::string& display_name,
    bool is_directory
) {
    const fs::path absolute_path = absolute_path_from_remote(remote_path);

    std::error_code ec;
    const auto removed = fs::remove_all(absolute_path, ec);

    OperationResult result;
    if (ec) {
        result.success = false;
        result.title = "Ошибка удаления";
        result.message = "Не удалось удалить объект из рабочей папки.";
        return result;
    }

    std::ostringstream ss;
    ss << (is_directory ? "Папка" : "Файл")
       << " удалён.\n\n"
       << "Имя:\n" << display_name << "\n\n"
       << "Удалено объектов: " << removed;

    result.success = true;
    result.title = "Удаление";
    result.message = ss.str();
    return result;
}

OperationResult LocalFsProvider::create_share_link(
    const std::string& remote_path,
    const std::string& display_name
) {
    OperationResult result;
    result.success = true;
    result.title = "Шаринг";

    std::ostringstream ss;
    ss << "Пока шаринг не подключён.\n\n"
       << "Выбран объект:\n" << display_name << "\n\n"
       << "Путь:\n" << remote_path;

    result.message = ss.str();
    return result;
}

OperationResult LocalFsProvider::open_file(
    const std::string& remote_path,
    const std::string& display_name
) {
    OperationResult result;
    result.success = true;
    result.title = "Открытие";

    std::ostringstream ss;
    ss << "Файл доступен в рабочей папке.\n\n"
       << "Имя:\n" << display_name << "\n\n"
       << "Путь:\n" << absolute_path_from_remote(remote_path);

    result.message = ss.str();
    return result;
}