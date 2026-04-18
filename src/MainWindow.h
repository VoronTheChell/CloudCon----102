#pragma once

#include <memory>
#include <string>
#include <vector>
#include <gtk/gtk.h>
#include "FileItem.h"
#include <unordered_map>

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
    void show_create_folder_dialog();
    void show_rename_dialog(const std::string& current_name);
    void show_copy_dialog(const std::string& initial_path);
    void show_move_dialog(const std::string& initial_path);
    void open_file_dialog();
    void copy_text_to_clipboard(const std::string& text);

private:
    enum class TextInputAction {
        None,
        CreateFolder,
        Rename,
        Copy,
        Move
    };

    enum class ContextMenuMode {
        Background,
        File,
        Directory
    };

    GtkApplication* app_ {};
    GtkWidget* window_ {};

    GtkWidget* root_box_ {};
    GtkWidget* sidebar_ {};
    GtkWidget* content_box_ {};

    GtkWidget* header_box_ {};
    GtkWidget* path_box_ {};
    GtkWidget* path_title_label_ {};
    GtkWidget* path_subtitle_label_ {};
    GtkWidget* back_button_ {};

    GtkWidget* upload_button_ {};
    GtkWidget* create_button_ {};

    GtkWidget* flowbox_ {};
    GtkWidget* status_label_ {};

    GtkWidget* context_popover_ {};
    GtkWidget* context_open_button_ {};
    GtkWidget* context_rename_button_ {};
    GtkWidget* context_copy_button_ {};
    GtkWidget* context_move_button_ {};
    GtkWidget* context_refresh_button_ {};
    GtkWidget* context_open_cache_button_ {};
    GtkWidget* context_new_folder_button_ {};
    GtkWidget* context_upload_button_ {};
    GtkWidget* context_download_button_ {};
    GtkWidget* context_delete_button_ {};
    GtkWidget* context_delete_cache_button_ {};
    GtkWidget* context_share_button_ {};

    GtkGesture* right_click_gesture_ {};
    GtkDropTarget* drop_target_ {};

    GtkWidget* text_input_window_ {};
    GtkWidget* text_input_entry_ {};
    TextInputAction pending_text_action_ {TextInputAction::None};

    GtkWidget* context_paste_button_ {};

    std::unordered_map<GtkWidget*, std::string> tile_path_map_;
    std::unordered_map<GtkWidget*, bool> tile_directory_map_;

    ContextMenuMode context_mode_ {ContextMenuMode::Background};

    std::vector<FileItem> files_;
    std::unique_ptr<AppController> controller_;

    void build_ui();
    void build_sidebar();
    void build_header();
    void build_file_area();
    void build_context_menu();
    void load_css();

    void rebuild_file_grid();
    GtkWidget* create_file_tile(const FileItem& item);
    void popup_context_menu(double x, double y);
    void update_context_menu_state();

    void show_text_input_dialog(
        const std::string& title,
        const std::string& message,
        const std::string& initial_text,
        TextInputAction action
    );

    const FileItem* selected_item() const;

    static void on_back_clicked_static(GtkButton* button, gpointer user_data);
    static void on_upload_clicked_static(GtkButton* button, gpointer user_data);
    static void on_create_clicked_static(GtkButton* button, gpointer user_data);

    static void on_flowbox_selected_children_changed(GtkFlowBox* box, gpointer user_data);
    static void on_flowbox_child_activated(GtkFlowBox* box, GtkFlowBoxChild* child, gpointer user_data);

    static void on_context_open_clicked(GtkButton* button, gpointer user_data);
    static void on_context_rename_clicked(GtkButton* button, gpointer user_data);
    static void on_context_copy_clicked(GtkButton* button, gpointer user_data);
    static void on_context_move_clicked(GtkButton* button, gpointer user_data);
    static void on_context_refresh_clicked(GtkButton* button, gpointer user_data);
    static void on_context_open_cache_clicked(GtkButton* button, gpointer user_data);
    static void on_context_new_folder_clicked(GtkButton* button, gpointer user_data);
    static void on_context_upload_clicked(GtkButton* button, gpointer user_data);
    static void on_context_download_clicked(GtkButton* button, gpointer user_data);
    static void on_context_delete_clicked(GtkButton* button, gpointer user_data);
    static void on_context_delete_cache_clicked(GtkButton* button, gpointer user_data);
    static void on_context_share_clicked(GtkButton* button, gpointer user_data);

    static void on_right_click_pressed(GtkGestureClick* gesture, int n_press, double x, double y, gpointer user_data);

    static gboolean on_drop_accept(GtkDropTarget* target, GValue* value, gpointer user_data);
    static gboolean on_drop_perform(GtkDropTarget* target, const GValue* value, double x, double y, gpointer user_data);
    static void on_file_opened(GObject* source_object, GAsyncResult* res, gpointer user_data);
    static void on_delete_confirmed(GObject* source_object, GAsyncResult* res, gpointer user_data);

    static void on_text_input_ok_clicked(GtkButton* button, gpointer user_data);
    static void on_text_input_cancel_clicked(GtkButton* button, gpointer user_data);
    static GdkContentProvider* on_tile_drag_prepare(GtkDragSource* source, double x, double y, gpointer user_data);
    static gboolean on_tile_drop(GtkDropTarget* target, const GValue* value, double x, double y, gpointer user_data);
    static void on_context_paste_clicked(GtkButton* button, gpointer user_data);
    static gboolean on_text_input_close_request(GtkWindow* window, gpointer user_data);
};