#pragma once

#include <memory>
#include <string>
#include <vector>
#include <gtk/gtk.h>

#include "FileItem.h"

class AppController;

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    GtkWidget* widget() const;

    void set_files(const std::vector<FileItem>& files);
    void set_path(const std::string& path);
    void set_status(const std::string& text);

    void show_info_dialog(const std::string& message, const std::string& detail = "");
    void show_delete_confirmation(const std::string& item_name, const std::string& detail);
    void open_file_dialog();

private:
    GtkApplication* app_ {};
    GtkWidget* window_ {};
    GtkWidget* root_box_ {};
    GtkWidget* sidebar_ {};
    GtkWidget* content_box_ {};
    GtkWidget* toolbar_ {};
    GtkWidget* path_label_ {};
    GtkWidget* back_button_ {};
    GtkWidget* upload_button_ {};
    GtkWidget* download_button_ {};
    GtkWidget* delete_button_ {};
    GtkWidget* share_button_ {};
    GtkWidget* files_list_ {};
    GtkWidget* status_label_ {};

    std::vector<FileItem> files_;
    std::unique_ptr<AppController> controller_;

    void build_ui();
    void rebuild_file_list();
    GtkWidget* create_file_row(const FileItem& item);

    static void on_row_selected(GtkListBox* box, GtkListBoxRow* row, gpointer user_data);
    static void on_row_activated(GtkListBox* box, GtkListBoxRow* row, gpointer user_data);

    static void on_upload_clicked_static(GtkButton* button, gpointer user_data);
    static void on_download_clicked_static(GtkButton* button, gpointer user_data);
    static void on_delete_clicked_static(GtkButton* button, gpointer user_data);
    static void on_share_clicked_static(GtkButton* button, gpointer user_data);
    static void on_back_clicked_static(GtkButton* button, gpointer user_data);

    static void on_file_opened(GObject* source_object, GAsyncResult* res, gpointer user_data);
    static void on_delete_confirmed(GObject* source_object, GAsyncResult* res, gpointer user_data);
};