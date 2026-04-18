#pragma once

#include <cstdint>
#include <string>

struct FileItem {
    std::string id;
    std::string name;
    std::string path;
    bool is_directory {false};
    std::uint64_t size {0};
    std::string mime_type;
    std::string modified_at;
    bool is_cached {false};
    std::string local_preview_path;
};