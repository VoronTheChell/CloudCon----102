#include "MainWindow.h"
#include "AppController.h"

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <pango/pango.h>
#include <sstream>

namespace {
bool can_render_image_preview(const FileItem& item) {
    return !item.is_directory && !item.local_preview_path.empty();
}
}

MainWindow::MainWindow(GtkApplication* app) : app_(app) {
    build_ui();
    controller_ = std::make_unique<AppController>(this);
    controller_->initialize();
}

GtkWidget* MainWindow::widget() const {
    return window_;
}

const FileItem* MainWindow::selected_item() const {
    GList* selected = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(flowbox_));
    if (selected == nullptr) {
        return nullptr;
    }

    GtkFlowBoxChild* child = GTK_FLOW_BOX_CHILD(selected->data);
    g_list_free(selected);

    if (child == nullptr) {
        return nullptr;
    }

    const int index = gtk_flow_box_child_get_index(child);
    if (index < 0 || static_cast<std::size_t>(index) >= files_.size()) {
        return nullptr;
    }

    return &files_[static_cast<std::size_t>(index)];
}

void MainWindow::load_css() {
    GtkCssProvider* provider = gtk_css_provider_new();

    const char* css = R"css(
        window {
            background: #1f2128;
            color: #f2f2f2;
        }

        .sidebar {
            background: linear-gradient(180deg, #2a1818 0%, #1f1b22 100%);
            padding: 12px;
        }

        .sidebar-title {
            font-size: 20px;
            font-weight: 700;
            color: #ffffff;
        }

        .primary-action {
            background: #f2f2f2;
            color: #1a1a1a;
            border-radius: 14px;
            padding: 10px;
            font-weight: 700;
        }

        .secondary-action {
            background: alpha(#ffffff, 0.08);
            color: #ffffff;
            border-radius: 14px;
            padding: 10px;
            font-weight: 600;
        }

        .nav-button {
            background: transparent;
            color: #d9d9d9;
            border-radius: 12px;
            padding: 10px 12px;
            font-weight: 600;
        }

        .nav-button:hover {
            background: alpha(#ffffff, 0.08);
        }

        .nav-button.active {
            background: alpha(#ffffff, 0.10);
            color: #ffffff;
        }

        .content {
            background: #22242b;
        }

        .path-title {
            font-size: 20px;
            font-weight: 700;
            color: #ffffff;
        }

        .path-subtitle {
            font-size: 13px;
            color: #b8bcc8;
        }

        flowboxchild.tile-child {
            background: transparent;
            border-radius: 18px;
            padding: 10px;
        }

        flowboxchild.tile-child:hover {
            background: alpha(#ffffff, 0.06);
        }

        flowboxchild.tile-child:selected {
            background: alpha(#ffffff, 0.10);
        }

        .tile-name {
            font-weight: 600;
            color: #ffffff;
        }

        .tile-meta {
            font-size: 12px;
            color: #b6bbc8;
        }

        .tile-preview-frame {
            background: alpha(#ffffff, 0.05);
            border-radius: 18px;
            padding: 4px;
        }

        .tile-preview {
            border-radius: 14px;
        }

        .status-bar {
            color: #b8bcc8;
            font-size: 12px;
            padding-top: 6px;
        }

        popover contents {
            background: #2c2f37;
            border-radius: 16px;
            padding: 8px;
        }

        popover button {
            border-radius: 10px;
            margin: 2px 0;
        }
    )css";

    gtk_css_provider_load_from_string(provider, css);

    GdkDisplay* display = gdk_display_get_default();
    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    g_object_unref(provider);
}

void MainWindow::build_sidebar() {
    sidebar_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(sidebar_, 250, -1);
    gtk_widget_add_css_class(sidebar_, "sidebar");

    GtkWidget* title = gtk_label_new("Диск");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_add_css_class(title, "sidebar-title");

    upload_button_ = gtk_button_new_with_label("Загрузить");
    gtk_widget_add_css_class(upload_button_, "primary-action");

    create_button_ = gtk_button_new_with_label("Создать");
    gtk_widget_add_css_class(create_button_, "secondary-action");

    connect_button_ = gtk_button_new_with_label("Подключить Яндекс.Диск");
    gtk_widget_add_css_class(connect_button_, "secondary-action");

    GtkWidget* nav_files = gtk_button_new_with_label("Файлы");
    gtk_widget_add_css_class(nav_files, "nav-button");
    gtk_widget_add_css_class(nav_files, "active");

    gtk_box_append(GTK_BOX(sidebar_), title);
    gtk_box_append(GTK_BOX(sidebar_), upload_button_);
    gtk_box_append(GTK_BOX(sidebar_), create_button_);
    gtk_box_append(GTK_BOX(sidebar_), connect_button_);
    gtk_box_append(GTK_BOX(sidebar_), nav_files);

    g_signal_connect(upload_button_, "clicked", G_CALLBACK(MainWindow::on_upload_clicked_static), this);
    g_signal_connect(create_button_, "clicked", G_CALLBACK(MainWindow::on_create_clicked_static), this);
    g_signal_connect(connect_button_, "clicked", G_CALLBACK(MainWindow::on_connect_clicked_static), this);
}

void MainWindow::build_header() {
    header_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(header_box_, 18);
    gtk_widget_set_margin_start(header_box_, 18);
    gtk_widget_set_margin_end(header_box_, 18);

    back_button_ = gtk_button_new_from_icon_name("go-previous-symbolic");

    GtkWidget* right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(right_box, TRUE);

    path_title_label_ = gtk_label_new("Файлы");
    gtk_widget_set_halign(path_title_label_, GTK_ALIGN_START);
    gtk_widget_add_css_class(path_title_label_, "path-title");

    path_subtitle_label_ = gtk_label_new("/");
    gtk_widget_set_halign(path_subtitle_label_, GTK_ALIGN_START);
    gtk_widget_add_css_class(path_subtitle_label_, "path-subtitle");

    gtk_box_append(GTK_BOX(right_box), path_title_label_);
    gtk_box_append(GTK_BOX(right_box), path_subtitle_label_);

    gtk_box_append(GTK_BOX(header_box_), back_button_);
    gtk_box_append(GTK_BOX(header_box_), right_box);

    g_signal_connect(back_button_, "clicked", G_CALLBACK(MainWindow::on_back_clicked_static), this);
}

void MainWindow::build_file_area() {
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_margin_start(scrolled, 8);
    gtk_widget_set_margin_end(scrolled, 8);
    gtk_widget_set_margin_top(scrolled, 4);

    flowbox_ = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox_), GTK_SELECTION_SINGLE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox_), 6);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox_), 12);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox_), 8);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox_), 8);

    gtk_widget_set_margin_top(flowbox_, 4);
    gtk_widget_set_margin_bottom(flowbox_, 4);
    gtk_widget_set_margin_start(flowbox_, 4);
    gtk_widget_set_margin_end(flowbox_, 4);
    gtk_widget_set_valign(flowbox_, GTK_ALIGN_START);
    gtk_widget_set_vexpand(flowbox_, FALSE);

    g_signal_connect(flowbox_, "selected-children-changed",
                     G_CALLBACK(MainWindow::on_flowbox_selected_children_changed), this);
    g_signal_connect(flowbox_, "child-activated",
                     G_CALLBACK(MainWindow::on_flowbox_child_activated), this);

    right_click_gesture_ = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click_gesture_), GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(flowbox_, GTK_EVENT_CONTROLLER(right_click_gesture_));
    g_signal_connect(right_click_gesture_, "pressed",
                     G_CALLBACK(MainWindow::on_right_click_pressed), this);

    drop_target_ = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY);

    GType drop_types[] = { GDK_TYPE_FILE_LIST, G_TYPE_FILE };
    gtk_drop_target_set_gtypes(drop_target_, drop_types, 2);

    g_signal_connect(drop_target_, "drop", G_CALLBACK(MainWindow::on_drop_perform), this);

    gtk_widget_add_controller(content_box_, GTK_EVENT_CONTROLLER(drop_target_));

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), flowbox_);

    status_label_ = gtk_label_new("Статус: приложение запущено");
    gtk_widget_set_halign(status_label_, GTK_ALIGN_START);
    gtk_widget_set_margin_start(status_label_, 12);
    gtk_widget_set_margin_end(status_label_, 12);
    gtk_widget_set_margin_bottom(status_label_, 8);
    gtk_widget_add_css_class(status_label_, "status-bar");

    gtk_box_append(GTK_BOX(content_box_), header_box_);
    gtk_box_append(GTK_BOX(content_box_), scrolled);
    gtk_box_append(GTK_BOX(content_box_), status_label_);
}

void MainWindow::build_context_menu() {
    context_popover_ = gtk_popover_new();
    gtk_widget_set_parent(context_popover_, content_box_);

    GtkWidget* menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(menu_box, 6);
    gtk_widget_set_margin_bottom(menu_box, 6);
    gtk_widget_set_margin_start(menu_box, 6);
    gtk_widget_set_margin_end(menu_box, 6);

    context_open_button_ = gtk_button_new_with_label("Открыть");
    context_rename_button_ = gtk_button_new_with_label("Переименовать");
    context_copy_button_ = gtk_button_new_with_label("Копировать");
    context_move_button_ = gtk_button_new_with_label("Переместить в...");
    context_refresh_button_ = gtk_button_new_with_label("Обновить");
    context_open_cache_button_ = gtk_button_new_with_label("Открыть папку кеша");
    context_new_folder_button_ = gtk_button_new_with_label("Новая папка");
    context_upload_button_ = gtk_button_new_with_label("Загрузить файл");
    context_paste_button_ = gtk_button_new_with_label("Вставить");
    context_download_button_ = gtk_button_new_with_label("Скачать в кеш");
    context_delete_button_ = gtk_button_new_with_label("Удалить с диска");
    context_delete_cache_button_ = gtk_button_new_with_label("Удалить из кеша");
    context_share_button_ = gtk_button_new_with_label("Поделиться");

    gtk_box_append(GTK_BOX(menu_box), context_open_button_);
    gtk_box_append(GTK_BOX(menu_box), context_rename_button_);
    gtk_box_append(GTK_BOX(menu_box), context_copy_button_);
    gtk_box_append(GTK_BOX(menu_box), context_move_button_);
    gtk_box_append(GTK_BOX(menu_box), context_refresh_button_);
    gtk_box_append(GTK_BOX(menu_box), context_open_cache_button_);
    gtk_box_append(GTK_BOX(menu_box), context_new_folder_button_);
    gtk_box_append(GTK_BOX(menu_box), context_upload_button_);
    gtk_box_append(GTK_BOX(menu_box), context_paste_button_);
    gtk_box_append(GTK_BOX(menu_box), context_download_button_);
    gtk_box_append(GTK_BOX(menu_box), context_delete_button_);
    gtk_box_append(GTK_BOX(menu_box), context_delete_cache_button_);
    gtk_box_append(GTK_BOX(menu_box), context_share_button_);

    gtk_popover_set_child(GTK_POPOVER(context_popover_), menu_box);

    g_signal_connect(context_open_button_, "clicked", G_CALLBACK(MainWindow::on_context_open_clicked), this);
    g_signal_connect(context_rename_button_, "clicked", G_CALLBACK(MainWindow::on_context_rename_clicked), this);
    g_signal_connect(context_copy_button_, "clicked", G_CALLBACK(MainWindow::on_context_copy_clicked), this);
    g_signal_connect(context_move_button_, "clicked", G_CALLBACK(MainWindow::on_context_move_clicked), this);
    g_signal_connect(context_refresh_button_, "clicked", G_CALLBACK(MainWindow::on_context_refresh_clicked), this);
    g_signal_connect(context_open_cache_button_, "clicked", G_CALLBACK(MainWindow::on_context_open_cache_clicked), this);
    g_signal_connect(context_new_folder_button_, "clicked", G_CALLBACK(MainWindow::on_context_new_folder_clicked), this);
    g_signal_connect(context_upload_button_, "clicked", G_CALLBACK(MainWindow::on_context_upload_clicked), this);
    g_signal_connect(context_paste_button_, "clicked", G_CALLBACK(MainWindow::on_context_paste_clicked), this);
    g_signal_connect(context_download_button_, "clicked", G_CALLBACK(MainWindow::on_context_download_clicked), this);
    g_signal_connect(context_delete_button_, "clicked", G_CALLBACK(MainWindow::on_context_delete_clicked), this);
    g_signal_connect(context_delete_cache_button_, "clicked", G_CALLBACK(MainWindow::on_context_delete_cache_clicked), this);
    g_signal_connect(context_share_button_, "clicked", G_CALLBACK(MainWindow::on_context_share_clicked), this);
}

void MainWindow::build_ui() {
    window_ = gtk_application_window_new(app_);
    gtk_window_set_title(GTK_WINDOW(window_), "Cloud Client");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1360, 860);

    load_css();

    root_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window_), root_box_);

    build_sidebar();

    content_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(content_box_, TRUE);
    gtk_widget_set_vexpand(content_box_, TRUE);
    gtk_widget_add_css_class(content_box_, "content");

    build_header();
    build_file_area();
    build_context_menu();

    gtk_box_append(GTK_BOX(root_box_), sidebar_);
    gtk_box_append(GTK_BOX(root_box_), content_box_);
}

void MainWindow::set_files(const std::vector<FileItem>& files) {
    files_ = files;
    rebuild_file_grid();
    update_context_menu_state();
}

void MainWindow::set_path(const std::string& path) {
    gtk_label_set_text(GTK_LABEL(path_subtitle_label_), path.c_str());

    if (path == "/") {
        gtk_label_set_text(GTK_LABEL(path_title_label_), "Файлы");
    } else {
        std::string title = path;
        const std::size_t pos = title.find_last_of('/');
        if (pos != std::string::npos && pos + 1 < title.size()) {
            title = title.substr(pos + 1);
        }
        gtk_label_set_text(GTK_LABEL(path_title_label_), title.c_str());
    }
}

void MainWindow::set_status(const std::string& text) {
    gtk_label_set_text(GTK_LABEL(status_label_), text.c_str());
}

void MainWindow::rebuild_file_grid() {
    tile_path_map_.clear();
    tile_directory_map_.clear();

    GtkWidget* child = gtk_widget_get_first_child(flowbox_);

    while (child != nullptr) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_flow_box_remove(GTK_FLOW_BOX(flowbox_), child);
        child = next;
    }

    for (const auto& item : files_) {
        GtkWidget* tile = create_file_tile(item);
        gtk_flow_box_insert(GTK_FLOW_BOX(flowbox_), tile, -1);
    }
}

GtkWidget* MainWindow::create_file_tile(const FileItem& item) {
    GtkWidget* child_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(child_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(child_box, GTK_ALIGN_START);
    gtk_widget_set_size_request(child_box, 160, 160);

    GtkWidget* visual = nullptr;

    if (can_render_image_preview(item)) {
        GtkWidget* preview_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_halign(preview_frame, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(preview_frame, 8);
        gtk_widget_set_size_request(preview_frame, 128, 96);
        gtk_widget_add_css_class(preview_frame, "tile-preview-frame");

        GtkWidget* picture = gtk_picture_new_for_filename(item.local_preview_path.c_str());
        gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
        gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_size_request(picture, 120, 88);
        gtk_widget_set_hexpand(picture, TRUE);
        gtk_widget_set_vexpand(picture, TRUE);
        gtk_widget_add_css_class(picture, "tile-preview");

        gtk_box_append(GTK_BOX(preview_frame), picture);
        visual = preview_frame;
    } else {
        GtkWidget* icon = gtk_label_new(item.is_directory ? "📁" : "📄");
        gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(icon, 8);

        PangoAttrList* attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_scale_new(3.0));
        gtk_label_set_attributes(GTK_LABEL(icon), attrs);
        pango_attr_list_unref(attrs);
        visual = icon;
    }

    GtkWidget* name = gtk_label_new(item.name.c_str());
    gtk_label_set_wrap(GTK_LABEL(name), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(name), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_justify(GTK_LABEL(name), GTK_JUSTIFY_CENTER);
    gtk_label_set_max_width_chars(GTK_LABEL(name), 14);
    gtk_widget_set_halign(name, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(name, "tile-name");

    std::ostringstream meta;
    if (item.is_directory) {
        meta << "Папка";
    } else {
        meta << (item.is_cached ? "Кеширован" : "Не кеширован");
    }

    GtkWidget* meta_label = gtk_label_new(meta.str().c_str());
    gtk_widget_set_halign(meta_label, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(meta_label, "tile-meta");

    gtk_box_append(GTK_BOX(child_box), visual);
    gtk_box_append(GTK_BOX(child_box), name);
    gtk_box_append(GTK_BOX(child_box), meta_label);

    GtkWidget* flow_child = gtk_flow_box_child_new();
    gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(flow_child), child_box);

    gtk_widget_add_css_class(flow_child, "tile-child");
    gtk_widget_set_size_request(flow_child, 180, 180);
    gtk_widget_set_halign(flow_child, GTK_ALIGN_START);
    gtk_widget_set_valign(flow_child, GTK_ALIGN_START);

    tile_path_map_[flow_child] = item.path;
    tile_directory_map_[flow_child] = item.is_directory;

    GtkDragSource* drag_source = gtk_drag_source_new();
    //gtk_drag_source_set_actions(drag_source, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    g_signal_connect(drag_source, "prepare", G_CALLBACK(MainWindow::on_tile_drag_prepare), this);
    gtk_widget_add_controller(flow_child, GTK_EVENT_CONTROLLER(drag_source));

    GtkDropTarget* drop_target = gtk_drop_target_new(G_TYPE_STRING, GDK_ACTION_MOVE);
    g_signal_connect(drop_target, "drop", G_CALLBACK(MainWindow::on_tile_drop), this);
    gtk_widget_add_controller(flow_child, GTK_EVENT_CONTROLLER(drop_target));

    return flow_child;
}

void MainWindow::update_context_menu_state() {
    const FileItem* item = selected_item();

    const bool is_background = (context_mode_ == ContextMenuMode::Background);
    const bool is_file = (context_mode_ == ContextMenuMode::File && item != nullptr);
    const bool is_directory = (context_mode_ == ContextMenuMode::Directory && item != nullptr);

    gtk_widget_set_visible(context_open_button_, is_file || is_directory);
    gtk_widget_set_visible(context_rename_button_, is_file || is_directory);
    gtk_widget_set_visible(context_copy_button_, is_file || is_directory);
    gtk_widget_set_visible(context_move_button_, is_file || is_directory);

    gtk_widget_set_visible(context_refresh_button_, is_background);
    gtk_widget_set_visible(context_open_cache_button_, is_background);
    gtk_widget_set_visible(context_new_folder_button_, is_background);
    gtk_widget_set_visible(context_upload_button_, is_background);

    gtk_widget_set_visible(context_paste_button_, is_background || is_directory);

    gtk_widget_set_visible(context_download_button_, is_file && item != nullptr && !item->is_cached);
    gtk_widget_set_visible(context_delete_cache_button_, is_file && item != nullptr && item->is_cached);

    gtk_widget_set_visible(context_delete_button_, is_file || is_directory);
    gtk_widget_set_visible(context_share_button_, is_file || is_directory);

    gtk_widget_set_sensitive(context_open_button_, is_file || is_directory);
    gtk_widget_set_sensitive(context_rename_button_, is_file || is_directory);
    gtk_widget_set_sensitive(context_copy_button_, is_file || is_directory);
    gtk_widget_set_sensitive(context_move_button_, is_file || is_directory);
    gtk_widget_set_sensitive(context_refresh_button_, is_background);
    gtk_widget_set_sensitive(context_open_cache_button_, is_background);
    gtk_widget_set_sensitive(context_new_folder_button_, is_background);
    gtk_widget_set_sensitive(context_upload_button_, is_background);
    gtk_widget_set_sensitive(context_paste_button_, is_background || is_directory);
    gtk_widget_set_sensitive(context_download_button_, is_file && item != nullptr && !item->is_cached);
    gtk_widget_set_sensitive(context_delete_cache_button_, is_file && item != nullptr && item->is_cached);
    gtk_widget_set_sensitive(context_delete_button_, is_file || is_directory);
    gtk_widget_set_sensitive(context_share_button_, is_file || is_directory);
}

void MainWindow::popup_context_menu(double x, double y) {
    update_context_menu_state();

    GdkRectangle rect {};
    rect.x = static_cast<int>(x);
    rect.y = static_cast<int>(y);
    rect.width = 1;
    rect.height = 1;

    gtk_popover_set_pointing_to(GTK_POPOVER(context_popover_), &rect);
    gtk_popover_popup(GTK_POPOVER(context_popover_));
}

void MainWindow::show_info_dialog(const std::string& message, const std::string& detail) {
    GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", message.c_str());
    gtk_alert_dialog_set_modal(dialog, TRUE);

    if (!detail.empty()) {
        gtk_alert_dialog_set_detail(dialog, detail.c_str());
    }

    gtk_alert_dialog_show(dialog, GTK_WINDOW(window_));
    g_object_unref(dialog);
}

void MainWindow::show_delete_confirmation(const std::string& item_name, const std::string& detail) {
    GtkAlertDialog* dialog = gtk_alert_dialog_new("Удалить \"%s\"?", item_name.c_str());
    gtk_alert_dialog_set_modal(dialog, TRUE);

    const char* buttons[] = {"Отмена", "Удалить", nullptr};
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 1);
    gtk_alert_dialog_set_detail(dialog, detail.c_str());

    gtk_alert_dialog_choose(
        dialog,
        GTK_WINDOW(window_),
        nullptr,
        MainWindow::on_delete_confirmed,
        this
    );

    g_object_unref(dialog);
}

void MainWindow::show_text_input_dialog(
    const std::string& title,
    const std::string& message,
    const std::string& initial_text,
    TextInputAction action
) {
    pending_text_action_ = action;

    text_input_window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(text_input_window_), title.c_str());
    gtk_window_set_modal(GTK_WINDOW(text_input_window_), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(text_input_window_), GTK_WINDOW(window_));
    gtk_window_set_default_size(GTK_WINDOW(text_input_window_), 440, 150);
    gtk_window_set_resizable(GTK_WINDOW(text_input_window_), FALSE);

    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(outer, 14);
    gtk_widget_set_margin_bottom(outer, 14);
    gtk_widget_set_margin_start(outer, 14);
    gtk_widget_set_margin_end(outer, 14);

    GtkWidget* label = gtk_label_new(message.c_str());
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    text_input_entry_ = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(text_input_entry_), initial_text.c_str());

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);

    GtkWidget* cancel_button = gtk_button_new_with_label("Отмена");
    GtkWidget* ok_button = gtk_button_new_with_label("OK");

    gtk_box_append(GTK_BOX(buttons), cancel_button);
    gtk_box_append(GTK_BOX(buttons), ok_button);

    gtk_box_append(GTK_BOX(outer), label);
    gtk_box_append(GTK_BOX(outer), text_input_entry_);
    gtk_box_append(GTK_BOX(outer), buttons);

    gtk_window_set_child(GTK_WINDOW(text_input_window_), outer);

    g_signal_connect(ok_button, "clicked", G_CALLBACK(MainWindow::on_text_input_ok_clicked), this);
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(MainWindow::on_text_input_cancel_clicked), this);
    g_signal_connect(text_input_window_, "close-request", G_CALLBACK(MainWindow::on_text_input_close_request), this);

    gtk_window_present(GTK_WINDOW(text_input_window_));
}

void MainWindow::show_create_folder_dialog() {
    show_text_input_dialog(
        "Новая папка",
        "Введите имя новой папки:",
        "",
        TextInputAction::CreateFolder
    );
}

void MainWindow::show_rename_dialog(const std::string& current_name) {
    show_text_input_dialog(
        "Переименовать",
        "Введите новое имя:",
        current_name,
        TextInputAction::Rename
    );
}

void MainWindow::show_copy_dialog(const std::string& initial_path) {
    show_text_input_dialog(
        "Копировать в",
        "Введите путь папки назначения, например /Archive:",
        initial_path,
        TextInputAction::Copy
    );
}

void MainWindow::show_move_dialog(const std::string& initial_path) {
    show_text_input_dialog(
        "Переместить в",
        "Введите путь папки назначения, например /Archive:",
        initial_path,
        TextInputAction::Move
    );
}

void MainWindow::open_file_dialog() {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Выберите файл для загрузки");

    gtk_file_dialog_open(
        dialog,
        GTK_WINDOW(window_),
        nullptr,
        MainWindow::on_file_opened,
        this
    );

    g_object_unref(dialog);
}

void MainWindow::copy_text_to_clipboard(const std::string& text) {
    GdkDisplay* display = gtk_widget_get_display(window_);
    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    gdk_clipboard_set_text(clipboard, text.c_str());
}

void MainWindow::show_yandex_connection_dialog(
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& access_token,
    const std::string& remote_root
) {
    if (yandex_connect_window_ != nullptr) {
        gtk_window_present(GTK_WINDOW(yandex_connect_window_));
        return;
    }

    yandex_connect_window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(yandex_connect_window_), "Подключение к Yandex Disk");
    gtk_window_set_modal(GTK_WINDOW(yandex_connect_window_), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(yandex_connect_window_), GTK_WINDOW(window_));
    gtk_window_set_default_size(GTK_WINDOW(yandex_connect_window_), 560, 420);
    gtk_window_set_resizable(GTK_WINDOW(yandex_connect_window_), FALSE);

    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(outer, 14);
    gtk_widget_set_margin_bottom(outer, 14);
    gtk_widget_set_margin_start(outer, 14);
    gtk_widget_set_margin_end(outer, 14);

    GtkWidget* intro = gtk_label_new(
        "Можно вставить готовый OAuth-токен либо выполнить вход по client_id/client_secret и коду подтверждения.\n"
        "Для получения кода у приложения в Yandex OAuth должен быть настроен Redirect URI: https://oauth.yandex.com/verification_code"
    );
    gtk_widget_set_halign(intro, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(intro), TRUE);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    auto add_row = [&](int row, const char* title, GtkWidget** out_entry, const std::string& value, bool hidden) {
        GtkWidget* label = gtk_label_new(title);
        gtk_widget_set_halign(label, GTK_ALIGN_START);

        GtkWidget* entry = gtk_entry_new();
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_editable_set_text(GTK_EDITABLE(entry), value.c_str());
        if (hidden) {
            gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
        }

        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
        *out_entry = entry;
    };

    add_row(0, "OAuth токен", &yandex_token_entry_, access_token, false);
    add_row(1, "Client ID", &yandex_client_id_entry_, client_id, false);
    add_row(2, "Client Secret", &yandex_client_secret_entry_, client_secret, true);
    add_row(3, "Код подтверждения", &yandex_code_entry_, "", false);
    add_row(4, "Remote root", &yandex_remote_root_entry_, remote_root.empty() ? "disk:/CloudClient" : remote_root, false);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);

    GtkWidget* cancel_button = gtk_button_new_with_label("Закрыть");
    GtkWidget* open_auth_button = gtk_button_new_with_label("Открыть авторизацию");
    GtkWidget* connect_code_button = gtk_button_new_with_label("Подключить по коду");
    GtkWidget* connect_token_button = gtk_button_new_with_label("Сохранить токен");

    gtk_box_append(GTK_BOX(buttons), cancel_button);
    gtk_box_append(GTK_BOX(buttons), open_auth_button);
    gtk_box_append(GTK_BOX(buttons), connect_code_button);
    gtk_box_append(GTK_BOX(buttons), connect_token_button);

    gtk_box_append(GTK_BOX(outer), intro);
    gtk_box_append(GTK_BOX(outer), grid);
    gtk_box_append(GTK_BOX(outer), buttons);
    gtk_window_set_child(GTK_WINDOW(yandex_connect_window_), outer);

    g_signal_connect(cancel_button, "clicked", G_CALLBACK(MainWindow::on_yandex_connect_cancel_clicked), this);
    g_signal_connect(open_auth_button, "clicked", G_CALLBACK(MainWindow::on_yandex_open_auth_clicked), this);
    g_signal_connect(connect_code_button, "clicked", G_CALLBACK(MainWindow::on_yandex_connect_code_clicked), this);
    g_signal_connect(connect_token_button, "clicked", G_CALLBACK(MainWindow::on_yandex_connect_token_clicked), this);
    g_signal_connect(yandex_connect_window_, "close-request", G_CALLBACK(MainWindow::on_yandex_connect_close_request), this);

    gtk_window_present(GTK_WINDOW(yandex_connect_window_));
}

void MainWindow::on_back_clicked_static(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->navigate_up();
}

void MainWindow::on_upload_clicked_static(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->request_upload();
}

void MainWindow::on_create_clicked_static(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->show_create_folder_dialog();
}

void MainWindow::on_connect_clicked_static(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->show_yandex_connection_dialog();
}

void MainWindow::on_flowbox_selected_children_changed(GtkFlowBox* box, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    GList* selected = gtk_flow_box_get_selected_children(box);
    if (selected == nullptr) {
        self->controller_->select_index(-1);
        return;
    }

    GtkFlowBoxChild* child = GTK_FLOW_BOX_CHILD(selected->data);
    g_list_free(selected);

    if (child == nullptr) {
        self->controller_->select_index(-1);
        return;
    }

    self->controller_->select_index(gtk_flow_box_child_get_index(child));
}

void MainWindow::on_flowbox_child_activated(GtkFlowBox* /*box*/, GtkFlowBoxChild* /*child*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->activate_selected();
}

void MainWindow::on_context_open_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_open_selected();
}

void MainWindow::on_context_rename_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));

    const FileItem* item = self->selected_item();
    if (item == nullptr) {
        return;
    }

    self->show_rename_dialog(item->name);
}

void MainWindow::on_context_copy_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_copy_selected();
}

void MainWindow::on_context_move_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->show_move_dialog(gtk_label_get_text(GTK_LABEL(self->path_subtitle_label_)));
}

void MainWindow::on_context_refresh_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_refresh();
}

void MainWindow::on_context_open_cache_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_open_cache_folder();
}

void MainWindow::on_context_new_folder_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->show_create_folder_dialog();
}

void MainWindow::on_context_upload_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_upload();
}

void MainWindow::on_context_paste_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_paste_into_current();
}

void MainWindow::on_context_download_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_download();
}

void MainWindow::on_context_delete_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_delete();
}

void MainWindow::on_context_delete_cache_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_delete_from_cache();
}

void MainWindow::on_context_share_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(self->context_popover_));
    self->controller_->request_share();
}

void MainWindow::on_right_click_pressed(GtkGestureClick* /*gesture*/, int /*n_press*/, double x, double y, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    GtkWidget* picked = gtk_widget_pick(self->flowbox_, x, y, GTK_PICK_DEFAULT);

    if (picked != nullptr) {
        GtkWidget* child_widget = gtk_widget_get_ancestor(picked, GTK_TYPE_FLOW_BOX_CHILD);
        if (child_widget != nullptr) {
            gtk_flow_box_select_child(
                GTK_FLOW_BOX(self->flowbox_),
                GTK_FLOW_BOX_CHILD(child_widget)
            );

            const int index = gtk_flow_box_child_get_index(GTK_FLOW_BOX_CHILD(child_widget));
            if (index >= 0 && static_cast<std::size_t>(index) < self->files_.size()) {
                const FileItem& item = self->files_[static_cast<std::size_t>(index)];
                self->context_mode_ = item.is_directory
                    ? ContextMenuMode::Directory
                    : ContextMenuMode::File;
            } else {
                self->context_mode_ = ContextMenuMode::Background;
            }
        } else {
            gtk_flow_box_unselect_all(GTK_FLOW_BOX(self->flowbox_));
            self->context_mode_ = ContextMenuMode::Background;
        }
    } else {
        gtk_flow_box_unselect_all(GTK_FLOW_BOX(self->flowbox_));
        self->context_mode_ = ContextMenuMode::Background;
    }

    self->popup_context_menu(x, y);
}

GdkContentProvider* MainWindow::on_tile_drag_prepare(GtkDragSource* source, double /*x*/, double /*y*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(source));

    auto it = self->tile_path_map_.find(widget);
    if (it == self->tile_path_map_.end()) {
        return nullptr;
    }

    const std::string remote_path = it->second;
    const auto is_dir_it = self->tile_directory_map_.find(widget);
    const bool is_directory = (is_dir_it != self->tile_directory_map_.end() && is_dir_it->second);

    GdkContentProvider* internal = gdk_content_provider_new_typed(G_TYPE_STRING, remote_path.c_str());

    if (!is_directory) {
        const std::string local_export = self->controller_->ensure_local_export_path(remote_path);
        if (!local_export.empty()) {
            GFile* file = g_file_new_for_path(local_export.c_str());
            GdkContentProvider* external = gdk_content_provider_new_typed(G_TYPE_FILE, file);
            g_object_unref(file);

            GdkContentProvider* providers[] = { internal, external };
            return gdk_content_provider_new_union(providers, 2);
        }
    }

    return internal;
}

gboolean MainWindow::on_tile_drop(GtkDropTarget* target, const GValue* value, double /*x*/, double /*y*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));

    auto it = self->tile_path_map_.find(widget);
    if (it == self->tile_path_map_.end()) {
        return FALSE;
    }

    auto is_dir_it = self->tile_directory_map_.find(widget);
    if (is_dir_it == self->tile_directory_map_.end() || !is_dir_it->second) {
        return FALSE;
    }

    if (!G_VALUE_HOLDS(value, G_TYPE_STRING)) {
        return FALSE;
    }

    const char* source_path = static_cast<const char*>(g_value_get_string(value));
    if (source_path == nullptr) {
        return FALSE;
    }

    return self->controller_->handle_drop_move_to_directory(source_path, it->second) ? TRUE : FALSE;
}

gboolean MainWindow::on_drop_perform(GtkDropTarget* /*target*/, const GValue* value, double /*x*/, double /*y*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
        GdkFileList* file_list = static_cast<GdkFileList*>(g_value_get_boxed(value));
        if (file_list == nullptr) {
            self->set_status("Статус: drop не содержит списка файлов");
            return FALSE;
        }

        GSList* files = gdk_file_list_get_files(file_list);
        if (files == nullptr) {
            self->set_status("Статус: список файлов пуст");
            return FALSE;
        }

        int count = 0;

        for (GSList* node = files; node != nullptr; node = node->next) {
            GFile* file = G_FILE(node->data);
            if (file == nullptr) {
                continue;
            }

            char* path = g_file_get_path(file);
            char* basename = g_file_get_basename(file);

            if (path != nullptr && basename != nullptr) {
                self->controller_->handle_local_file_chosen(path, basename);
                ++count;
            }

            g_free(path);
            g_free(basename);
        }

        g_slist_free(files);

        if (count > 0) {
            std::ostringstream ss;
            ss << "Статус: перетащено файлов — " << count;
            self->set_status(ss.str());
            return TRUE;
        }

        self->set_status("Статус: не удалось обработать список файлов");
        return FALSE;
    }

    if (G_VALUE_HOLDS(value, G_TYPE_FILE)) {
        GFile* file = G_FILE(g_value_get_object(value));
        if (file == nullptr) {
            self->set_status("Статус: drop не содержит файла");
            return FALSE;
        }

        char* path = g_file_get_path(file);
        char* basename = g_file_get_basename(file);

        if (path != nullptr && basename != nullptr) {
            self->controller_->handle_local_file_chosen(path, basename);

            std::ostringstream ss;
            ss << "Статус: перетащен файл — " << basename;
            self->set_status(ss.str());

            g_free(path);
            g_free(basename);
            return TRUE;
        }

        g_free(path);
        g_free(basename);
        self->set_status("Статус: не удалось прочитать dropped file");
        return FALSE;
    }

    self->set_status("Статус: неподдерживаемый формат drop");
    return FALSE;
}

void MainWindow::on_yandex_open_auth_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    const char* client_id = self->yandex_client_id_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_client_id_entry_))
        : "";

    if (!self->controller_->open_yandex_authorization_page(client_id ? client_id : "")) {
        return;
    }

    self->show_info_dialog(
        "Авторизация открыта",
        "Ссылка открыта в браузере и скопирована в буфер обмена. После входа Яндекс покажет код подтверждения — вставь его в поле 'Код подтверждения'."
    );
}

void MainWindow::on_yandex_connect_code_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    const std::string client_id = self->yandex_client_id_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_client_id_entry_))
        : "";
    const std::string client_secret = self->yandex_client_secret_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_client_secret_entry_))
        : "";
    const std::string code = self->yandex_code_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_code_entry_))
        : "";
    const std::string remote_root = self->yandex_remote_root_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_remote_root_entry_))
        : "disk:/CloudClient";

    if (self->controller_->connect_with_confirmation_code(client_id, client_secret, code, remote_root)) {
        if (self->yandex_connect_window_ != nullptr) {
            gtk_window_destroy(GTK_WINDOW(self->yandex_connect_window_));
            self->yandex_connect_window_ = nullptr;
        }
    }
}

void MainWindow::on_yandex_connect_token_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    const std::string token = self->yandex_token_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_token_entry_))
        : "";
    const std::string client_id = self->yandex_client_id_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_client_id_entry_))
        : "";
    const std::string client_secret = self->yandex_client_secret_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_client_secret_entry_))
        : "";
    const std::string remote_root = self->yandex_remote_root_entry_
        ? gtk_editable_get_text(GTK_EDITABLE(self->yandex_remote_root_entry_))
        : "disk:/CloudClient";

    if (self->controller_->connect_with_token(token, remote_root, client_id, client_secret)) {
        if (self->yandex_connect_window_ != nullptr) {
            gtk_window_destroy(GTK_WINDOW(self->yandex_connect_window_));
            self->yandex_connect_window_ = nullptr;
        }
    }
}

void MainWindow::on_yandex_connect_cancel_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    if (self->yandex_connect_window_ != nullptr) {
        gtk_window_destroy(GTK_WINDOW(self->yandex_connect_window_));
        self->yandex_connect_window_ = nullptr;
    }
}

gboolean MainWindow::on_yandex_connect_close_request(GtkWindow* /*window*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->yandex_connect_window_ = nullptr;
    self->yandex_client_id_entry_ = nullptr;
    self->yandex_client_secret_entry_ = nullptr;
    self->yandex_code_entry_ = nullptr;
    self->yandex_token_entry_ = nullptr;
    self->yandex_remote_root_entry_ = nullptr;
    return FALSE;
}

void MainWindow::on_file_opened(GObject* source_object, GAsyncResult* res, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    GtkFileDialog* dialog = GTK_FILE_DIALOG(source_object);

    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(dialog, res, &error);

    if (error != nullptr) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) {
            self->set_status("Статус: ошибка выбора файла");
            self->show_info_dialog("Ошибка выбора файла", error->message);
        } else {
            self->set_status("Статус: выбор файла отменён");
        }

        g_error_free(error);
        return;
    }

    if (file == nullptr) {
        self->set_status("Статус: файл не выбран");
        return;
    }

    char* path = g_file_get_path(file);
    char* basename = g_file_get_basename(file);

    self->controller_->handle_local_file_chosen(
        path ? path : "",
        basename ? basename : "unknown"
    );

    g_free(path);
    g_free(basename);
    g_object_unref(file);
}

void MainWindow::on_delete_confirmed(GObject* source_object, GAsyncResult* res, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    GtkAlertDialog* dialog = GTK_ALERT_DIALOG(source_object);

    GError* error = nullptr;
    const int response = gtk_alert_dialog_choose_finish(dialog, res, &error);

    if (error != nullptr) {
        self->set_status("Статус: ошибка подтверждения удаления");
        self->show_info_dialog("Ошибка", error->message);
        g_error_free(error);
        return;
    }

    self->controller_->confirm_delete(response == 1);
}

void MainWindow::on_text_input_ok_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    if (self->text_input_entry_ != nullptr) {
        const char* text = gtk_editable_get_text(GTK_EDITABLE(self->text_input_entry_));
        const std::string value = text ? text : "";

        if (!value.empty()) {
            if (self->pending_text_action_ == TextInputAction::CreateFolder) {
                self->controller_->handle_create_folder_named(value);
            } else if (self->pending_text_action_ == TextInputAction::Rename) {
                self->controller_->handle_rename_selected(value);
            } else if (self->pending_text_action_ == TextInputAction::Copy) {
                self->controller_->handle_copy_selected(value);
            } else if (self->pending_text_action_ == TextInputAction::Move) {
                self->controller_->handle_move_selected(value);
            }
        }
    }

    self->pending_text_action_ = TextInputAction::None;
    self->text_input_entry_ = nullptr;

    if (self->text_input_window_ != nullptr) {
        gtk_window_destroy(GTK_WINDOW(self->text_input_window_));
        self->text_input_window_ = nullptr;
    }
}

void MainWindow::on_text_input_cancel_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    self->pending_text_action_ = TextInputAction::None;
    self->text_input_entry_ = nullptr;

    if (self->text_input_window_ != nullptr) {
        gtk_window_destroy(GTK_WINDOW(self->text_input_window_));
        self->text_input_window_ = nullptr;
    }
}

gboolean MainWindow::on_text_input_close_request(GtkWindow* /*window*/, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    self->pending_text_action_ = TextInputAction::None;
    self->text_input_entry_ = nullptr;
    self->text_input_window_ = nullptr;

    return FALSE;
}