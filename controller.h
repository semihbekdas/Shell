#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "model.h"
#include "view.h"
#include <gtk/gtk.h>

// Controller yapısı
typedef struct {
    // Model ve View referansları
    ShmBuf *model;
    View *view;
    
    // Uygulama referansı
    GtkApplication *app;
    
    // Zamanlayıcı ID'si
    guint message_update_timer_id;
    
    // Durum bilgileri
    gboolean running;
} Controller;

/**
 * Controller yapısını başlatır
 * 
 * @param app GTK uygulaması
 * @return Başlatılan Controller yapısı
 */
Controller* controller_init(GtkApplication *app);

/**
 * Uygulamayı başlatır ve ana döngüye girer
 * 
 * @param controller Controller yapısı
 * @param argc Komut satırı argüman sayısı
 * @param argv Komut satırı argümanları
 * @return Çıkış kodu
 */
int controller_run(Controller *controller, int argc, char **argv);

/**
 * Kullanıcı girişini işler
 * 
 * @param controller Controller yapısı
 * @param terminal_id Terminal ID'si
 * @param input Kullanıcı girişi
 * @return İşlem başarı durumu (0: başarılı, -1: başarısız)
 */
int controller_process_input(Controller *controller, int terminal_id, const char *input);

/**
 * Komut çalıştırır
 * 
 * @param controller Controller yapısı
 * @param terminal_id Terminal ID'si
 * @param command Çalıştırılacak komut
 * @return İşlem başarı durumu (0: başarılı, -1: başarısız)
 */
int controller_execute_command(Controller *controller, int terminal_id, const char *command);

/**
 * Mesaj gönderir
 * 
 * @param controller Controller yapısı
 * @param terminal_id Terminal ID'si
 * @param message Gönderilecek mesaj
 * @return İşlem başarı durumu (0: başarılı, -1: başarısız)
 */
int controller_send_message(Controller *controller, int terminal_id, const char *message);

/**
 * Mesajları günceller
 * 
 * @param controller Controller yapısı
 * @return İşlem başarı durumu (0: başarılı, -1: başarısız)
 */
int controller_update_messages(Controller *controller);

/**
 * Yeni terminal oluşturur
 * 
 * @param controller Controller yapısı
 * @return Oluşturulan terminal ID'si, başarısızlık durumunda -1
 */
int controller_create_terminal(Controller *controller);

/**
 * Terminali kapatır
 * 
 * @param controller Controller yapısı
 * @param terminal_id Kapatılacak terminal ID'si
 * @return İşlem başarı durumu (0: başarılı, -1: başarısız)
 */
int controller_close_terminal(Controller *controller, int terminal_id);

/**
 * Controller yapısını temizler ve kaynakları serbest bırakır
 * 
 * @param controller Controller yapısı
 */
void controller_cleanup(Controller *controller);

/**
 * Uygulama etkinleştirildiğinde çağrılan callback fonksiyonu
 * 
 * @param app GTK uygulaması
 * @param user_data Kullanıcı verisi (Controller yapısı)
 */
void controller_activate(GtkApplication *app, gpointer user_data);

/**
 * Periyodik mesaj güncelleme zamanlayıcısı callback fonksiyonu
 * 
 * @param user_data Kullanıcı verisi (Controller yapısı)
 * @return Zamanlayıcının devam edip etmeyeceği (TRUE: devam, FALSE: durdur)
 */
gboolean controller_message_update_timer(gpointer user_data);

#endif /* CONTROLLER_H */
