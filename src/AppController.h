#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CloudProvider.h"
#include "FileItem.h"
#include "TaskManager.h"

class MainWindow;

class AppController {
public:
    explicit AppController(MainWindow* window);

    void initialize();

    void select_index(int index);
    void activate_selected();
    void navigate_up();

    void request_upload();
    void request_download();
    void request_delete();
    void request_share();

    void handle_local_file_chosen(const std::string& local_path, const std::string& display_name);
    void confirm_delete(bool confirmed);

    const std::vector<FileItem>& files() const;
    const std::string& current_path() const;

private:
    MainWindow* window_ {};
    std::unique_ptr<CloudProvider> provider_;
    std::vector<FileItem> current_files_;
    std::string current_path_ {"/"};
    int selected_index_ {-1};

    TaskManager task_manager_;

    void load_files();
    void navigate_to(const std::string& path);
    const FileItem* selected_item() const;

    void show_result(const OperationResult& result);
};