#pragma once

#include <string>
#include <vector>
#include "FileItem.h"

class CloudProvider {
public:
    virtual ~CloudProvider() = default;

    virtual std::vector<FileItem> list_files(const std::string& path) = 0;
};