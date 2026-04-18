#include "MainWindow.h"
#include "AppController.h"

#include <sstream>

MainWindow::MainWindow(GtkApplication* app) : app_(app) {
    build_ui();
    controller_ = std::make_unique<AppController>(this);
    controller_->initialize();
}

GtkWidget* MainWindow::widget() const {
    return window_;
}

void MainWindow::build_ui() {
    window_ = gtk_application_window_new(app_);
    gtk_window_set_title(GTK_WINDOW(window_), "Cloud Client");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1100, 700);

    GtkWidget* header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header_bar), TRUE);

    GtkWidget* title_label = gtk_label_new("Мультиоблачный клиент");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), title_label);
    gtk_window_set_titlebar(GTK_WINDOW(window_), header_bar);

    root_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window_), root_box_);

    sidebar_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(sidebar_, 240, -1);
    gtk_widget_set_margin_top(sidebar_, 12);
    gtk_widget_set_margin_bottom(sidebar_, 12);
    gtk_widget_set_margin_start(sidebar_, 12);
    gtk_widget_set_margin_end(sidebar_, 6);

    GtkWidget* sidebar_title = gtk_label_new("Подключённые облака");
    gtk_widget_set_halign(sidebar_title, GTK_ALIGN_START);

    GtkWidget* yandex_button = gtk_button_new_with_label("Yandex Disk");
    GtkWidget* nextcloud_button = gtk_button_new_with_label("Nextcloud");
    GtkWidget* add_cloud_button = gtk_button_new_with_label("Добавить облако");

    gtk_box_append(GTK_BOX(sidebar_), sidebar_title);
    gtk_box_append(GTK_BOX(sidebar_), yandex_button);
    gtk_box_append(GTK_BOX(sidebar_), nextcloud_button);
    gtk_box_append(GTK_BOX(sidebar_), add_cloud_button);

    content_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_hexpand(content_box_, TRUE);
    gtk_widget_set_vexpand(content_box_, TRUE);
    gtk_widget_set_margin_top(content_box_, 12);
    gtk_widget_set_margin_bottom(content_box_, 12);
    gtk_widget_set_margin_start(content_box_, 6);
    gtk_widget_set_margin_end(content_box_, 12);

    toolbar_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    back_button_ = gtk_button_new_with_label("Назад");
    upload_button_ = gtk_button_new_with_label("Загрузить");
    download_button_ = gtk_button_new_with_label("Скачать");
    delete_button_ = gtk_button_new_with_label("Удалить");
    share_button_ = gtk_button_new_with_label("Поделиться");

    gtk_box_append(GTK_BOX(toolbar_), back_button_);
    gtk_box_append(GTK_BOX(toolbar_), upload_button_);
    gtk_box_append(GTK_BOX(toolbar_), download_button_);
    gtk_box_append(GTK_BOX(toolbar_), delete_button_);
    gtk_box_append(GTK_BOX(toolbar_), share_button_);

    g_signal_connect(back_button_, "clicked", G_CALLBACK(MainWindow::on_back_clicked_static), this);
    g_signal_connect(upload_button_, "clicked", G_CALLBACK(MainWindow::on_upload_clicked_static), this);
    g_signal_connect(download_button_, "clicked", G_CALLBACK(MainWindow::on_download_clicked_static), this);
    g_signal_connect(delete_button_, "clicked", G_CALLBACK(MainWindow::on_delete_clicked_static), this);
    g_signal_connect(share_button_, "clicked", G_CALLBACK(MainWindow::on_share_clicked_static), this);

    path_label_ = gtk_label_new("/");
    gtk_widget_set_halign(path_label_, GTK_ALIGN_START);

    GtkWidget* files_frame = gtk_frame_new("Файлы");
    gtk_widget_set_hexpand(files_frame, TRUE);
    gtk_widget_set_vexpand(files_frame, TRUE);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    files_list_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(files_list_), GTK_SELECTION_SINGLE);
    gtk_list_box_set_show_separators(GTK_LIST_BOX(files_list_), TRUE);

    g_signal_connect(files_list_, "row-selected", G_CALLBACK(MainWindow::on_row_selected), this);
    g_signal_connect(files_list_, "row-activated", G_CALLBACK(MainWindow::on_row_activated), this);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), files_list_);
    gtk_frame_set_child(GTK_FRAME(files_frame), scrolled);

    status_label_ = gtk_label_new("Статус: приложение запущено");
    gtk_widget_set_halign(status_label_, GTK_ALIGN_START);

    gtk_box_append(GTK_BOX(content_box_), toolbar_);
    gtk_box_append(GTK_BOX(content_box_), path_label_);
    gtk_box_append(GTK_BOX(content_box_), files_frame);
    gtk_box_append(GTK_BOX(content_box_), status_label_);

    gtk_box_append(GTK_BOX(root_box_), sidebar_);
    gtk_box_append(GTK_BOX(root_box_), content_box_);
}

void MainWindow::set_files(const std::vector<FileItem>& files) {
    files_ = files;
    rebuild_file_list();
}

void MainWindow::set_path(const std::string& path) {
    gtk_label_set_text(GTK_LABEL(path_label_), path.c_str());
}

void MainWindow::set_status(const std::string& text) {
    gtk_label_set_text(GTK_LABEL(status_label_), text.c_str());
}

void MainWindow::rebuild_file_list() {
    while (GtkWidget* child = gtk_widget_get_first_child(files_list_)) {
        gtk_list_box_remove(GTK_LIST_BOX(files_list_), child);
    }

    for (const auto& item : files_) {
        GtkWidget* row = create_file_row(item);
        gtk_list_box_append(GTK_LIST_BOX(files_list_), row);
    }
}

GtkWidget* MainWindow::create_file_row(const FileItem& item) {
    GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(row_box, 8);
    gtk_widget_set_margin_bottom(row_box, 8);
    gtk_widget_set_margin_start(row_box, 10);
    gtk_widget_set_margin_end(row_box, 10);

    const char* icon_text = item.is_directory ? "📁" : "📄";
    GtkWidget* icon_label = gtk_label_new(icon_text);
    gtk_widget_set_valign(icon_label, GTK_ALIGN_CENTER);

    GtkWidget* text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(text_box, TRUE);

    GtkWidget* name_label = gtk_label_new(item.name.c_str());
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);

    std::ostringstream meta;
    if (item.is_directory) {
        meta << "Папка";
    } else {
        meta << item.size << " байт";
    }

    if (item.is_cached) {
        meta << " • Кеширован";
    }

    GtkWidget* meta_label = gtk_label_new(meta.str().c_str());
    gtk_widget_set_halign(meta_label, GTK_ALIGN_START);

    gtk_box_append(GTK_BOX(text_box), name_label);
    gtk_box_append(GTK_BOX(text_box), meta_label);

    gtk_box_append(GTK_BOX(row_box), icon_label);
    gtk_box_append(GTK_BOX(row_box), text_box);

    GtkWidget* modified_label = gtk_label_new(item.modified_at.c_str());
    gtk_widget_set_valign(modified_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row_box), modified_label);

    return row_box;
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

void MainWindow::on_row_selected(GtkListBox* /* box */, GtkListBoxRow* row, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);

    if (row == nullptr) {
        self->controller_->select_index(-1);
        return;
    }

    self->controller_->select_index(gtk_list_box_row_get_index(row));
}

void MainWindow::on_row_activated(GtkListBox* /* box */, GtkListBoxRow* /* row */, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->activate_selected();
}

void MainWindow::on_upload_clicked_static(GtkButton* /* button */, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->request_upload();
}

void MainWindow::on_download_clicked_static(GtkButton* /* button */, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->request_download();
}

void MainWindow::on_delete_clicked_static(GtkButton* /* button */, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->request_delete();
}

void MainWindow::on_share_clicked_static(GtkButton* /* button */, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->request_share();
}

void MainWindow::on_back_clicked_static(GtkButton* /* button */, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    self->controller_->navigate_up();
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