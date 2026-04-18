#pragma once

#include <string>
#include <vector>

class ClipboardManager {
public:
    enum class Mode {
        None,
        Copy
    };

    void clear();

    void set_copy(const std::vector<std::string>& paths);
    bool has_data() const;

    Mode mode() const;
    const std::vector<std::string>& paths() const;

private:
    Mode mode_ {Mode::None};
    std::vector<std::string> paths_;
};