#include "view.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

// Yardımcı fonksiyonlar
static void scroll_to_end(GtkTextView *text_view) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_view_scroll_to_iter(text_view, &iter, 0.0, TRUE, 0.0, 1.0);
}

// Terminal sekmesi kapatma verisi
typedef struct {
    int terminal_id;
    View *view;
} CloseTerminalData;

// Kapatma verisi için özel temizleme fonksiyonu
static void close_data_free(gpointer data, GClosure *closure) {
    (void)closure; // Kullanılmayan parametre uyarısını önlemek için
    g_free(data);
}

// View yapısını başlatır ve ana pencereyi oluşturur
View* view_init(GtkApplication *app, ShmBuf *shm_buffer) {
    // View yapısını oluştur ve başlangıç değerlerini ata
    View *view = g_new0(View, 1);
    if (!view) return NULL;
    
    view->shm_buffer = shm_buffer;
    view->max_terminals = 10;
    view->terminal_count = 0;
    view->active_terminal = -1;
    
    // Terminal dizilerini oluştur
    view->terminal_views = g_new0(GtkWidget*, view->max_terminals);
    view->terminal_buffers = g_new0(GtkTextBuffer*, view->max_terminals);
    view->input_entries = g_new0(GtkWidget*, view->max_terminals);
    
    if (!view->terminal_views || !view->terminal_buffers || !view->input_entries) {
        view_cleanup(view);
        return NULL;
    }
    
    // Ana pencereyi oluştur
    view->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(view->window), "Multi-User Communicating Shells");
    gtk_window_set_default_size(GTK_WINDOW(view->window), 1000, 700);
    
    // Ana dikey düzen kutusu
    view->main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_window_set_child(GTK_WINDOW(view->window), view->main_box);
    
    // Terminal alanı (sol taraf)
    GtkWidget *terminal_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_hexpand(terminal_area, TRUE);
    gtk_box_append(GTK_BOX(view->main_box), terminal_area);
    
    // Üst kısım: "+" butonu ile yeni sekme ekleme
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(terminal_area), header_box);
    
    GtkWidget *add_tab_button = gtk_button_new_with_label("+");
    gtk_box_append(GTK_BOX(header_box), add_tab_button);
    
    // Terminal sekmeleri
    view->terminal_notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(view->terminal_notebook, TRUE);
    gtk_box_append(GTK_BOX(terminal_area), view->terminal_notebook);
    
    // Mesaj paneli (sağ taraf)
    view->message_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(view->message_panel, 250, -1);
    gtk_box_append(GTK_BOX(view->main_box), view->message_panel);
    
    // Mesaj paneli başlığı
    GtkWidget *message_header = gtk_label_new("Shared Messages");
    gtk_box_append(GTK_BOX(view->message_panel), message_header);
    
    // Mesaj görüntüleme alanı
    GtkWidget *message_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(message_scroll, TRUE);
    gtk_box_append(GTK_BOX(view->message_panel), message_scroll);
    
    view->message_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view->message_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view->message_view), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(message_scroll), view->message_view);
    
    view->message_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view->message_view));
    
    // Sinyal bağlantıları
    g_signal_connect(add_tab_button, "clicked", G_CALLBACK(on_new_terminal_clicked), view);
    g_signal_connect(view->terminal_notebook, "switch-page", G_CALLBACK(on_terminal_switch_page), view);
    
    // İlk terminali ekle
    view_add_terminal(view, "Terminal 1");
    
    // NOT: Periyodik mesaj güncelleme zamanlayıcısı controller tarafından yönetilecek
    // Bu satır kaldırıldı: g_timeout_add(1000, update_messages_timeout, view);
    
    return view;
}

// Yeni bir terminal sekmesi oluşturur
int view_add_terminal(View *view, const char *title) {
    if (!view || view->terminal_count >= view->max_terminals) {
        return -1;
    }
    
    int terminal_id = view->terminal_count;
    
    // Terminal sekmesi için ana konteyner
    GtkWidget *terminal_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    
    // Terminal çıktı alanı
    GtkWidget *terminal_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(terminal_scroll, TRUE);
    gtk_box_append(GTK_BOX(terminal_box), terminal_scroll);
    
    view->terminal_views[terminal_id] = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view->terminal_views[terminal_id]), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view->terminal_views[terminal_id]), TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view->terminal_views[terminal_id]), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(terminal_scroll), view->terminal_views[terminal_id]);
    
    view->terminal_buffers[terminal_id] = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view->terminal_views[terminal_id]));
    
    // Komut giriş alanı
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(terminal_box), input_box);
    
    GtkWidget *prompt_label = gtk_label_new("$ ");
    gtk_box_append(GTK_BOX(input_box), prompt_label);
    
    view->input_entries[terminal_id] = gtk_entry_new();
    gtk_widget_set_hexpand(view->input_entries[terminal_id], TRUE);
    gtk_box_append(GTK_BOX(input_box), view->input_entries[terminal_id]);
    
    // Sekme başlığı için kutu
    GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *tab_label = gtk_label_new(title);
    gtk_box_append(GTK_BOX(tab_box), tab_label);
    
    // Sekme kapatma düğmesi
    GtkWidget *close_button = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_set_valign(close_button, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(tab_box), close_button);
    
    // Kapatma düğmesi için veri yapısı
    CloseTerminalData *close_data = g_new(CloseTerminalData, 1);
    close_data->terminal_id = terminal_id;
    close_data->view = view;
    
    // GTK4'te g_signal_connect_data kullanırken özel temizleme fonksiyonu kullan
    g_signal_connect_data(close_button, "clicked", G_CALLBACK(on_close_terminal_clicked), 
                          close_data, close_data_free, 0);
    
    // Komut girişi için sinyal bağlantısı
    g_object_set_data(G_OBJECT(view->input_entries[terminal_id]), "terminal_id", GINT_TO_POINTER(terminal_id));
    g_signal_connect(view->input_entries[terminal_id], "activate", G_CALLBACK(on_command_entry_activated), view);
    
    // Sekmeyi ekle
    gtk_notebook_append_page(GTK_NOTEBOOK(view->terminal_notebook), terminal_box, tab_box);
    
    // Sekmeyi görünür yap ve aktif et
    gtk_widget_set_visible(tab_box, TRUE);
    gtk_widget_set_visible(terminal_box, TRUE);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(view->terminal_notebook), terminal_id);
    view->active_terminal = terminal_id;
    view->terminal_count++;
    
    // Hoş geldiniz mesajı
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(view->terminal_buffers[terminal_id], &iter);
    gtk_text_buffer_insert(view->terminal_buffers[terminal_id], &iter, 
                          "Terminal başlatıldı. Komut girmek için aşağıdaki giriş alanını kullanın.\n"
                          "Mesaj göndermek için @msg ile başlayan komutlar kullanın.\n\n", -1);
    
    return terminal_id;
}

// Terminal çıktısını günceller
int view_update_terminal_output(View *view, int terminal_id, const char *output) {
    if (!view || terminal_id < 0 || terminal_id >= view->terminal_count || !output) {
        return -1;
    }
    
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(view->terminal_buffers[terminal_id], &iter);
    gtk_text_buffer_insert(view->terminal_buffers[terminal_id], &iter, output, -1);
    
    // Otomatik kaydırma
    scroll_to_end(GTK_TEXT_VIEW(view->terminal_views[terminal_id]));
    
    return 0;
}

// Mesaj panelini günceller
int view_update_message_panel(View *view, const char *message, const char *username) {
    if (!view || !message) {
        return -1;
    }
    
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(view->message_buffer, &iter);
    
    // Kullanıcı adı varsa, renkli olarak ekle
    if (username && strlen(username) > 0) {
        // Kullanıcı adı için etiket oluştur
        GtkTextTag *username_tag = gtk_text_buffer_create_tag(view->message_buffer, NULL,
                                                            "foreground", "blue",
                                                            "weight", PANGO_WEIGHT_BOLD,
                                                            NULL);
        
        // Kullanıcı adını ekle
        gtk_text_buffer_insert_with_tags(view->message_buffer, &iter, username, -1, username_tag, NULL);
        gtk_text_buffer_insert(view->message_buffer, &iter, ": ", -1);
    }
    
    // Mesajı ekle
    gtk_text_buffer_insert(view->message_buffer, &iter, message, -1);
    gtk_text_buffer_insert(view->message_buffer, &iter, "\n", -1);
    
    // Otomatik kaydırma
    scroll_to_end(GTK_TEXT_VIEW(view->message_view));
    
    return 0;
}

// Aktif terminal indeksini değiştirir
int view_set_active_terminal(View *view, int terminal_id) {
    if (!view || terminal_id < 0 || terminal_id >= view->terminal_count) {
        return -1;
    }
    
    gtk_notebook_set_current_page(GTK_NOTEBOOK(view->terminal_notebook), terminal_id);
    view->active_terminal = terminal_id;
    
    return 0;
}

// Terminal sekmesini kapatır
int view_close_terminal(View *view, int terminal_id) {
    if (!view || terminal_id < 0 || terminal_id >= view->terminal_count) {
        return -1;
    }
    
    // Sekmeyi kaldır
    gtk_notebook_remove_page(GTK_NOTEBOOK(view->terminal_notebook), terminal_id);
    
    // Terminal verilerini kaydır
    for (int i = terminal_id; i < view->terminal_count - 1; i++) {
        view->terminal_views[i] = view->terminal_views[i + 1];
        view->terminal_buffers[i] = view->terminal_buffers[i + 1];
        view->input_entries[i] = view->input_entries[i + 1];
        
        // Terminal ID'lerini güncelle
        if (view->input_entries[i]) {
            g_object_set_data(G_OBJECT(view->input_entries[i]), "terminal_id", GINT_TO_POINTER(i));
        }
    }
    
    view->terminal_count--;
    
    // Aktif terminal ayarlanması
    if (view->terminal_count > 0) {
        if (view->active_terminal >= view->terminal_count) {
            view->active_terminal = view->terminal_count - 1;
        }
    } else {
        view->active_terminal = -1;
    }
    
    return 0;
}

// View yapısını temizler ve kaynakları serbest bırakır
void view_cleanup(View *view) {
    if (!view) return;
    
    // Dizileri temizle
    if (view->terminal_views) g_free(view->terminal_views);
    if (view->terminal_buffers) g_free(view->terminal_buffers);
    if (view->input_entries) g_free(view->input_entries);
    
    // View yapısını serbest bırak
    g_free(view);
}

// Callback fonksiyonları

// Komut giriş alanında Enter tuşuna basıldığında çağrılır
void on_command_entry_activated(GtkEntry *entry, gpointer user_data) {
    View *view = (View *)user_data;
    int terminal_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(entry), "terminal_id"));
    
    // Komutu al - GTK4'te buffer üzerinden alınır
    GtkEntryBuffer *buffer = gtk_entry_get_buffer(entry);
    const char *command = gtk_entry_buffer_get_text(buffer);
    
    if (!command || strlen(command) == 0) {
        return;
    }
    
    // Komutu terminal çıktısına ekle
    char prompt[512];
    snprintf(prompt, sizeof(prompt), "$ %s\n", command);
    view_update_terminal_output(view, terminal_id, prompt);
    
    // Komutu işle
    if (strncmp(command, "@msg ", 5) == 0) {
        // Mesaj gönderme komutu
        const char *message = command + 5;
        if (strlen(message) > 0) {
            // Mesajı paylaşılan belleğe gönder
            ShmBuf* result = model_send_message(view->shm_buffer, message);
            
            if (result != NULL) {
                // Mesaj panelini güncelle (kullanıcı adı olarak terminal ID'sini kullan)
                char sender_name[32];
                snprintf(sender_name, sizeof(sender_name), "Terminal %d", terminal_id + 1);
                view_update_message_panel(view, message, sender_name);
            } else {
                // Mesaj gönderme hatası
                view_update_terminal_output(view, terminal_id, "Mesaj gönderilemedi!\n");
            }
        }
    } else {
        // Shell komutu
        char output[4096] = {0};
        int result = model_execute_command(command, output, sizeof(output));
        
        if (result == 0) {
            // Komut başarılı
            view_update_terminal_output(view, terminal_id, output);
        } else {
            // Komut hatası
            char error_msg[4096];
            // Çıktı boyutunu sınırla
            size_t max_output_len = sizeof(error_msg) - 100; // Hata mesajı için yer bırak
            if (strlen(output) > max_output_len) {
                output[max_output_len] = '\0';
            }
            int written = snprintf(error_msg, sizeof(error_msg), "Komut çalıştırma hatası (kod: %d)\n%s\n", result, output);
        
        // Kesinti olup olmadığını kontrol et
        if (written >= (int)sizeof(error_msg)) {
            // Kesinti oldu, mesajın sonuna kesinti bilgisi ekle
            const char *truncated_msg = "... (çıktı kesildi)";
            size_t truncated_len = strlen(truncated_msg);
            if (sizeof(error_msg) > truncated_len + 1) {
                strcpy(error_msg + sizeof(error_msg) - truncated_len - 1, truncated_msg);
            }
        }
        
        view_update_terminal_output(view, terminal_id, error_msg);
        }
    }
    
    // Giriş alanını temizle - GTK4'te buffer üzerinden yapılır
    gtk_entry_buffer_set_text(buffer, "", 0);
}

// Terminal sekmesi değiştirildiğinde çağrılır
void on_terminal_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data) {
    View *view = (View *)user_data;
    (void)notebook; // Kullanılmayan parametre uyarısını önlemek için
    (void)page;     // Kullanılmayan parametre uyarısını önlemek için
    view->active_terminal = page_num;
}

// Yeni terminal sekmesi oluşturma düğmesine tıklandığında çağrılır
void on_new_terminal_clicked(GtkButton *button, gpointer user_data) {
    View *view = (View *)user_data;
    (void)button; // Kullanılmayan parametre uyarısını önlemek için
    
    // Yeni terminal sekmesi için başlık oluştur
    char title[32];
    snprintf(title, sizeof(title), "Terminal %d", view->terminal_count + 1);
    
    // Yeni terminal ekle
    view_add_terminal(view, title);
}

// Terminal sekmesi kapatma düğmesine tıklandığında çağrılır
void on_close_terminal_clicked(GtkButton *button, gpointer user_data) {
    CloseTerminalData *data = (CloseTerminalData *)user_data;
    (void)button; // Kullanılmayan parametre uyarısını önlemek için
    
    // Son terminali kapatmaya çalışıyorsa, izin verme
    if (data->view->terminal_count <= 1) {
        // Eski kod:
        // GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->view->window),
        //                                           GTK_DIALOG_MODAL,
        //                                           GTK_MESSAGE_INFO,
        //                                           GTK_BUTTONS_OK,
        //                                           "En az bir terminal açık kalmalıdır.");
        // g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
        // gtk_window_present(GTK_WINDOW(dialog));
        
        // Yeni kod (GtkAlertDialog kullanarak):
        GtkAlertDialog *alert = gtk_alert_dialog_new("En az bir terminal açık kalmalıdır.");
        gtk_alert_dialog_set_modal(alert, TRUE);
        gtk_alert_dialog_set_detail(alert, "Programın çalışması için en az bir terminal gereklidir.");
        gtk_alert_dialog_set_buttons(alert, (const char*[]){"Tamam", NULL});
        gtk_alert_dialog_set_default_button(alert, 0);
        gtk_alert_dialog_set_cancel_button(alert, 0);
        
        gtk_alert_dialog_choose(alert, GTK_WINDOW(data->view->window), NULL, NULL, NULL);
        g_object_unref(alert);
        return;
    }
    
    // Terminali kapat
    view_close_terminal(data->view, data->terminal_id);
}

// Uygulama periyodik olarak mesajları güncellemek için çağrılır
// Bu fonksiyon artık controller tarafından yönetilecek, ancak view.h'da tanımlı olduğu için burada bırakıyoruz
gboolean update_messages_timeout(gpointer user_data) {
    View *view = (View *)user_data;
    
    // Paylaşılan bellekten mesajları oku
    char last_message[4096] = {0};
    int result = model_read_messages(view->shm_buffer, last_message, sizeof(last_message));
    
    if (result > 0 && strlen(last_message) > 0) {
        // Mesaj formatını ayrıştır: T{terminal_id}:{message}
        int sender_terminal = -1;
        char message_content[4096] = {0};
        
        if (sscanf(last_message, "T%d:%[^\n]", &sender_terminal, message_content) == 2) {
            // Mesaj panelini güncelle (kullanıcı adı olarak terminal ID'sini kullan)
            char sender_name[32];
            snprintf(sender_name, sizeof(sender_name), "Terminal %d", sender_terminal + 1);
            view_update_message_panel(view, message_content, sender_name);
        } else {
            // Format ayrıştırılamadıysa orijinal mesajı göster
            view_update_message_panel(view, last_message, "Unknown");
        }
    }
    
    // Zamanlayıcıyı devam ettir
    return G_SOURCE_CONTINUE;
}
