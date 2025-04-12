#ifndef MODEL_H
#define MODEL_H
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdbool.h>

#define BUF_SIZE 4096
#define SHARED_FILE_PATH "mymsgbuf"
#define MAX_COMMAND_LENGTH 256
#define MAX_PROCESSES 10

// Process bilgilerini tutan yapı
typedef struct {
    pid_t pid;          // Process ID
    char command[MAX_COMMAND_LENGTH];  // Komut metni
    int status;         // Çalışıyor/sonlandı durumu
} ProcessInfo;

// Paylaşılan bellek yapısı
typedef struct shmbuf {
    #ifndef __APPLE__
    sem_t sem;     // Okuma/yazma kontrolü için semafor (Linux için)
    #endif
    sem_t *sem_ptr; // macOS için named semaphore işaretçisi
    size_t cnt;    // 'msgbuf' içinde kullanılan bayt sayısı
    int fd;        // Dosya tanımlayıcısı
    size_t buf_size;
    size_t last_read_pos; // Son okunan mesaj pozisyonu
    char msgbuf[]; // Transfer edilen veri
} ShmBuf;

// Model fonksiyonları
ShmBuf* model_init();
int model_execute_command(const char* command, char* output, size_t output_size);
ShmBuf* model_send_message(ShmBuf* shmp, const char* message);
int model_read_messages(ShmBuf* shmp, char* buffer, size_t buffer_size);
void model_cleanup(ShmBuf* shmp);

#endif /* MODEL_H */


/*
stdio.h ve stdlib.h: Standart giriş/çıkış ve bellek yönetimi için.
unistd.h: fork(), pipe(), dup2() gibi POSIX fonksiyonları için.
string.h: strncpy(), strlen() gibi string işlemleri için.
fcntl.h: shm_open() ve dosya işlemleri için.
sys/types.h: pid_t gibi tipler için.
sys/wait.h: wait() ve waitpid() için.
sys/mman.h: mmap() ve munmap() için.
semaphore.h: sem_t ve semafor fonksiyonları için.
*/

/*
sem: Semafor, süreçler arası senkronizasyon için.
cnt: Tamponda kullanılan bayt sayısını tutar. Esnek diziyle uyumlu.
fd: Paylaşılan bellek dosyasının tanımlayıcısı, temizlik için gerekli
msgbuf[]: Esnek dizi */

/*
ShmBuf* model_init(): Paylaşılan belleği başlatır. Parametresiz, dönüş tipi pointer. .
int model_execute_command(const char* command, char* output, size_t output_size): Komut çalıştırır, çıktıyı output'a yazar. int dönüş tipiyle başarı/başarısızlık bildirilebilir. .
int model_send_message(ShmBuf* shmp, const char* message): Mesaj gönderir. .
int model_read_messages(ShmBuf* shmp, char* buffer, size_t buffer_size): Mesajları okur. int ile okunan bayt sayısı veya hata kodu dönebilir.
void model_cleanup(ShmBuf* shmp): Belleği temizler.
*/