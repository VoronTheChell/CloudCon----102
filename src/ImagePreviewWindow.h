#pragma once

#include <string>
#include <gtk/gtk.h>

class ImagePreviewWindow {
public:
    explicit ImagePreviewWindow(GtkApplication* app);

    void show_file(const std::string& path, const std::string& title = "Просмотр изображения");

private:
    GtkApplication* app_ {};
    GtkWidget* window_ {};
    GtkWidget* picture_ {};
};