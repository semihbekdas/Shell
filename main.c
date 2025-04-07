#include "controller.h"
#include <gtk/gtk.h>
#include <stdio.h>

int main(int argc, char **argv) {
    // GTK uygulamasını oluştur
    GtkApplication *app = gtk_application_new("com.example.MultiUserShells", G_APPLICATION_DEFAULT_FLAGS);
    
    // Controller başlat
    Controller *controller = controller_init(app);
    if (!controller) {
        g_printerr("Controller başlatılamadı!\n");
        g_object_unref(app);
        return 1;
    }
    
    // Uygulamayı çalıştır
    int status = controller_run(controller, argc, argv);
    
    // Uygulama referansını temizle
    g_object_unref(app);
    
    return status;
}
