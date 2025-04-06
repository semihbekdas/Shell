#ifndef VIEW_H
#define VIEW_H

#include <gtk/gtk.h>
#include "model.h"

// View yapısı
typedef struct {
    // Ana pencere ve konteynerler
    GtkWidget *window;
    GtkWidget *main_box;
    
    // Terminal emülatör widget'ları
    GtkWidget *terminal_notebook;     // Sekme konteynerı
    GtkWidget **terminal_views;       // Terminal çıktı alanları (GtkTextView)
    GtkTextBuffer **terminal_buffers; // Terminal çıktı tamponları
    GtkWidget **input_entries;        // Komut giriş alanları (GtkEntry)
    
    // Mesaj paneli widget'ları
    GtkWidget *message_panel;         // Mesaj paneli konteynerı
    GtkWidget *message_view;          // Mesaj görüntüleme alanı (GtkTextView)
    GtkTextBuffer *message_buffer;    // Mesaj tamponu
    
    // Durum bilgileri
    int active_terminal;              // Aktif terminal indeksi
    int terminal_count;               // Toplam terminal sayısı
    int max_terminals;                // Maksimum terminal sayısı
    
    // Model referansı
    ShmBuf *shm_buffer;               // Paylaşılan bellek tamponu
} View;

// View fonksiyonları

/**
 * View yapısını başlatır ve ana pencereyi oluşturur
 * 
 * @param app GTK uygulaması
 * @param shm_buffer Model tarafından oluşturulan paylaşılan bellek
 * @return Başlatılan View yapısı
 */
View* view_init(GtkApplication *app, ShmBuf *shm_buffer);

/**
 * Yeni bir terminal sekmesi oluşturur
 * 
 * @param view View yapısı
 * @param title Terminal sekmesi başlığı
 * @return Oluşturulan terminal indeksi, başarısızlık durumunda -1
 */
int view_add_terminal(View *view, const char *title);

/**
 * Terminal çıktısını günceller
 * 
 * @param view View yapısı
 * @param terminal_id Terminal indeksi
 * @param output Gösterilecek çıktı
 * @return Başarı durumu (0: başarılı, -1: başarısız)
 */
int view_update_terminal_output(View *view, int terminal_id, const char *output);

/**
 * Mesaj panelini günceller
 * 
 * @param view View yapısı
 * @param message Gösterilecek mesaj
 * @param username Mesajı gönderen kullanıcı adı (opsiyonel)
 * @return Başarı durumu (0: başarılı, -1: başarısız)
 */
int view_update_message_panel(View *view, const char *message, const char *username);

/**
 * Aktif terminal indeksini değiştirir
 * 
 * @param view View yapısı
 * @param terminal_id Yeni aktif terminal indeksi
 * @return Başarı durumu (0: başarılı, -1: başarısız)
 */
int view_set_active_terminal(View *view, int terminal_id);

/**
 * Terminal sekmesini kapatır
 * 
 * @param view View yapısı
 * @param terminal_id Kapatılacak terminal indeksi
 * @return Başarı durumu (0: başarılı, -1: başarısız)
 */
int view_close_terminal(View *view, int terminal_id);

/**
 * View yapısını temizler ve kaynakları serbest bırakır
 * 
 * @param view View yapısı
 */
void view_cleanup(View *view);

// Callback fonksiyon prototipleri

/**
 * Komut giriş alanında Enter tuşuna basıldığında çağrılır
 * 
 * @param entry Komut giriş alanı
 * @param user_data Kullanıcı verisi (View yapısı)
 */
void on_command_entry_activated(GtkEntry *entry, gpointer user_data);

/**
 * Terminal sekmesi değiştirildiğinde çağrılır
 * 
 * @param notebook Terminal sekme konteynerı
 * @param page Aktif sayfa
 * @param page_num Sayfa numarası
 * @param user_data Kullanıcı verisi (View yapısı)
 */
void on_terminal_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);

/**
 * Yeni terminal sekmesi oluşturma düğmesine tıklandığında çağrılır
 * 
 * @param button Düğme
 * @param user_data Kullanıcı verisi (View yapısı)
 */
void on_new_terminal_clicked(GtkButton *button, gpointer user_data);

/**
 * Terminal sekmesi kapatma düğmesine tıklandığında çağrılır
 * 
 * @param button Düğme
 * @param user_data Kullanıcı verisi (terminal_id ve View yapısı)
 */
void on_close_terminal_clicked(GtkButton *button, gpointer user_data);

/**
 * Uygulama periyodik olarak mesajları güncellemek için çağrılır
 * 
 * @param user_data Kullanıcı verisi (View yapısı)
 * @return Periyodik çağrının devam edip etmeyeceği (TRUE: devam, FALSE: durdur)
 */
gboolean update_messages_timeout(gpointer user_data);

#endif /* VIEW_H */
