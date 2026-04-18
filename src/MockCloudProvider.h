#pragma once

#include "CloudProvider.h"

class MockCloudProvider final : public CloudProvider {
public:
    std::vector<FileItem> list_files(const std::string& path) override;
};