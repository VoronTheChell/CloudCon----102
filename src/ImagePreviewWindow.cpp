#include "ImagePreviewWindow.h"

ImagePreviewWindow::ImagePreviewWindow(GtkApplication* app)
    : app_(app) {
    window_ = gtk_application_window_new(app_);
    gtk_window_set_default_size(GTK_WINDOW(window_), 960, 720);
    gtk_window_set_title(GTK_WINDOW(window_), "Просмотр изображения");

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(window_), scrolled);

    picture_ = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(picture_), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture_), GTK_CONTENT_FIT_CONTAIN);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), picture_);
}

void ImagePreviewWindow::show_file(const std::string& path, const std::string& title) {
    gtk_window_set_title(GTK_WINDOW(window_), title.c_str());

    GFile* file = g_file_new_for_path(path.c_str());
    gtk_picture_set_file(GTK_PICTURE(picture_), file);
    g_object_unref(file);

    gtk_window_present(GTK_WINDOW(window_));
}