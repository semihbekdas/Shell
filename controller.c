#include "controller.h"
#include "model.h"
#include "view.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Global controller referansı - view.c'den erişim için
Controller *g_controller = NULL;

// Mesaj yapısı
typedef struct {
    char content[4096];
    int sender_terminal;
    time_t timestamp;
} Message;

// Son mesajları takip etmek için
#define MAX_MESSAGES 20
static Message message_log[MAX_MESSAGES];
static int message_count = 0;

// Controller yapısını başlatır
Controller* controller_init(GtkApplication *app) {
    // Controller yapısını oluştur
    Controller *controller = g_new0(Controller, 1);
    if (!controller) return NULL;
    
    // Uygulama referansını kaydet
    controller->app = app;
    
    // Model başlat
    controller->model = model_init();
    if (!controller->model) {
        g_free(controller);
        return NULL;
    }
    
    // Başlangıç değerlerini ata
    controller->running = FALSE;
    controller->message_update_timer_id = 0;
    
    // Mesaj geçmişini temizle
    memset(message_log, 0, sizeof(message_log));
    message_count = 0;
    
    // Global controller referansını ayarla
    g_controller = controller;
    
    return controller;
}

// Uygulamayı başlatır ve ana döngüye girer
int controller_run(Controller *controller, int argc, char **argv) {
    if (!controller || !controller->app) {
        return -1;
    }
    
    // Uygulama etkinleştirme sinyalini bağla
    g_signal_connect(controller->app, "activate", G_CALLBACK(controller_activate), controller);
    
    // Uygulamayı çalıştır
    controller->running = TRUE;
    int status = g_application_run(G_APPLICATION(controller->app), argc, argv);
    
    // Kaynakları temizle
    controller_cleanup(controller);
    
    return status;
}

// Kullanıcı girişini işler
int controller_process_input(Controller *controller, int terminal_id, const char *input) {
    if (!controller || !input || terminal_id < 0) {
        return -1;
    }
    
    // Girişin boş olup olmadığını kontrol et
    if (strlen(input) == 0) {
        return 0;
    }
    
    // Mesaj komutu mu kontrol et
    if (strncmp(input, "@msg ", 5) == 0) {
        const char *message = input + 5;
        return controller_send_message(controller, terminal_id, message);
    } else if (strcmp(input, "clear") == 0) {
        // Terminal temizleme komutu
        return view_clear_terminal(controller->view, terminal_id);
    } else if (strcmp(input, "exit") == 0) {
        // Terminal kapatma komutu - view.c'de işleniyor
        return 0;
    } else {
        // Normal shell komutu
        return controller_execute_command(controller, terminal_id, input);
    }
}

// Komut çalıştırır
int controller_execute_command(Controller *controller, int terminal_id, const char *command) {
    if (!controller || !command || terminal_id < 0) {
        return -1;
    }
    
    // Komutu çalıştır
    char output[4096] = {0};
    int result = model_execute_command(command, output, sizeof(output));
    
    // Çıktıyı görüntüle
    if (result == 0) {
        // Komut başarılı
        view_update_terminal_output(controller->view, terminal_id, output, true);
    } else {
        // Komut hatası
        char error_msg[4096];
        snprintf(error_msg, sizeof(error_msg), "Komut çalıştırma hatası (kod: %d)\n%s\n", result, output);
        view_update_terminal_output(controller->view, terminal_id, error_msg, true);
    }
    
    return result;
}

// Mesaj gönderir
int controller_send_message(Controller *controller, int terminal_id, const char *message) {
    if (!controller || !message || terminal_id < 0) {
        return -1;
    }
    
    // Mesajı formatlayarak terminal ID'sini ekle
    char formatted_message[4096];
    snprintf(formatted_message, sizeof(formatted_message), "T%d:%s", terminal_id, message);
    
    // Mesajı paylaşılan belleğe gönder
    ShmBuf* result = model_send_message(controller->model, formatted_message);
    
    if (result == NULL) {
        // Mesaj gönderme hatası
        view_update_terminal_output(controller->view, terminal_id, "Mesaj gönderilemedi!\n", true);
        return -1;
    }
    
    // Mesajı geçmişe ekle
    if (message_count >= MAX_MESSAGES) {
        // Eski mesajları kaydır
        memmove(&message_log[0], &message_log[1], (MAX_MESSAGES - 1) * sizeof(Message));
        message_count = MAX_MESSAGES - 1;
    }
    
    // Yeni mesajı ekle
    strncpy(message_log[message_count].content, message, sizeof(message_log[message_count].content) - 1);
    message_log[message_count].content[sizeof(message_log[message_count].content) - 1] = '\0';
    message_log[message_count].sender_terminal = terminal_id;
    message_log[message_count].timestamp = time(NULL);
    message_count++;
    
    // Mesaj panelini güncelle - sadece gönderen terminal için
    char sender_name[32];
    snprintf(sender_name, sizeof(sender_name), "Terminal %d", terminal_id + 1);
    view_update_message_panel(controller->view, message, sender_name);
    
    return 0;
}

// Mesajları günceller
int controller_update_messages(Controller *controller) {
    if (!controller) {
        return -1;
    }
    
    // Paylaşılan bellekten mesajları oku
    char formatted_message[4096] = {0};
    int result = model_read_messages(controller->model, formatted_message, sizeof(formatted_message));
    
    // Mesaj var mı kontrol et
    if (result > 0 && strlen(formatted_message) > 0) {
        // Mesaj formatını ayrıştır: T{terminal_id}:{message}
        int sender_terminal = -1;
        char message_content[4096] = {0};
        
        if (sscanf(formatted_message, "T%d:%[^\n]", &sender_terminal, message_content) == 2) {
            // Aktif terminal ID'sini al
            int active_terminal_index = controller->view->active_terminal;
            int active_terminal_id = -1;
            
            if (active_terminal_index >= 0 && active_terminal_index < controller->view->terminal_count) {
                active_terminal_id = controller->view->terminal_ids[active_terminal_index];
            }
            
            // Mesaj kendi terminalimizden değilse göster
            if (sender_terminal != active_terminal_id) {
                // Son 5 saniye içinde aynı mesaj gösterildi mi kontrol et
                time_t now = time(NULL);
                int already_shown = 0;
                
                for (int i = 0; i < message_count; i++) {
                    if (message_log[i].sender_terminal == sender_terminal && 
                        strcmp(message_log[i].content, message_content) == 0 &&
                        now - message_log[i].timestamp < 5) {
                        already_shown = 1;
                        break;
                    }
                }
                
                if (!already_shown) {
                    // Mesaj panelini güncelle
                    char sender_name[32];
                    snprintf(sender_name, sizeof(sender_name), "Terminal %d", sender_terminal + 1);
                    view_update_message_panel(controller->view, message_content, sender_name);
                }
            }
        }
    }
    
    return result;
}

// Yeni terminal oluşturur
int controller_create_terminal(Controller *controller) {
    if (!controller || !controller->view) {
        return -1;
    }
    
    // Yeni terminal sekmesi için başlık oluştur
    char title[32];
    snprintf(title, sizeof(title), "Terminal %d", controller->view->next_terminal_id + 1);
    
    // Yeni terminal ekle
    return view_add_terminal(controller->view, title);
}

// Terminali kapatır
int controller_close_terminal(Controller *controller, int terminal_id) {
    if (!controller || !controller->view || terminal_id < 0) {
        return -1;
    }
    
    // Son terminali kapatmaya çalışıyorsa, izin verme
    if (controller->view->terminal_count <= 1) {
        return -1;
    }
    
    // Terminali kapat
    return view_close_terminal(controller->view, terminal_id);
}

// Controller yapısını temizler ve kaynakları serbest bırakır
void controller_cleanup(Controller *controller) {
    if (!controller) return;
    
    // Zamanlayıcıyı durdur
    if (controller->message_update_timer_id > 0) {
        g_source_remove(controller->message_update_timer_id);
        controller->message_update_timer_id = 0;
    }
    
    // View temizle
    if (controller->view) {
        view_cleanup(controller->view);
        controller->view = NULL;
    }
    
    // Model temizle
    if (controller->model) {
        model_cleanup(controller->model);
        controller->model = NULL;
    }
    
    // Global controller referansını temizle
    g_controller = NULL;
    
    // Controller yapısını serbest bırak
    g_free(controller);
}

// Uygulama etkinleştirildiğinde çağrılan callback fonksiyonu
void controller_activate(GtkApplication *app, gpointer user_data) {
    Controller *controller = (Controller *)user_data;
    
    // View başlat
    controller->view = view_init(app, controller->model);
    if (!controller->view) {
        g_printerr("View başlatılamadı!\n");
        return;
    }
    
    // Pencereyi göster
    gtk_window_present(GTK_WINDOW(controller->view->window));
    
    // Mesaj güncelleme zamanlayıcısını başlat (her 1 saniyede bir)
    controller->message_update_timer_id = g_timeout_add(1000, controller_message_update_timer, controller);
}

// Periyodik mesaj güncelleme zamanlayıcısı callback fonksiyonu
gboolean controller_message_update_timer(gpointer user_data) {
    Controller *controller = (Controller *)user_data;
    
    // Mesajları güncelle
    controller_update_messages(controller);
    
    // Zamanlayıcıyı devam ettir
    return G_SOURCE_CONTINUE;
}
