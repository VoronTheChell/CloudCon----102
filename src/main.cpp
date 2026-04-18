#include <gtk/gtk.h>
#include "MainWindow.h"

static void on_activate(GtkApplication* app, gpointer /* user_data */) {
    MainWindow* main_window = new MainWindow(app);
    gtk_window_present(GTK_WINDOW(main_window->widget()));
}

int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new(
        "com.example.cloudclient",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}