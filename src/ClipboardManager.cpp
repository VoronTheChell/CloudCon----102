#include "ClipboardManager.h"

void ClipboardManager::clear() {
    mode_ = Mode::None;
    paths_.clear();
}

void ClipboardManager::set_copy(const std::vector<std::string>& paths) {
    mode_ = Mode::Copy;
    paths_ = paths;
}

bool ClipboardManager::has_data() const {
    return mode_ != Mode::None && !paths_.empty();
}

ClipboardManager::Mode ClipboardManager::mode() const {
    return mode_;
}

const std::vector<std::string>& ClipboardManager::paths() const {
    return paths_;
}