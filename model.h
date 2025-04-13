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
#include <limits.h>  // PATH_MAX için

#define BUF_SIZE 4096
#define SHARED_FILE_PATH "mymsgbuf"
#define MAX_COMMAND_LENGTH 256
#define MAX_PROCESSES 10
#define MAX_TERMINALS 10  // Maksimum terminal sayısı

// Process bilgilerini tutan yapı
typedef struct {
    pid_t pid;          // Process ID
    char command[MAX_COMMAND_LENGTH];  // Komut metni
    int status;         // Çalışıyor/sonlandı durumu
} ProcessInfo;

// Terminal süreç bilgilerini tutan yapı
typedef struct {
    int terminal_id;                // Terminal ID
    pid_t process_id;               // Terminal süreç ID'si
    int pipe_to_terminal[2];        // Ana süreçten terminal sürecine veri göndermek için pipe
    int pipe_from_terminal[2];      // Terminal sürecinden ana sürece veri göndermek için pipe
    bool active;                    // Terminal aktif mi
} TerminalProcess;

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
    
    // Terminal süreç bilgileri
    TerminalProcess terminal_processes[MAX_TERMINALS];
    int terminal_count;
    
    char msgbuf[]; // Transfer edilen veri
} ShmBuf;

// Model fonksiyonları
ShmBuf* model_init();
int model_execute_command(const char* command, char* output, size_t output_size, int terminal_id);
ShmBuf* model_send_message(ShmBuf* shmp, const char* message);
int model_read_messages(ShmBuf* shmp, char* buffer, size_t buffer_size);
void model_cleanup(ShmBuf* shmp);

// Terminal süreç yönetimi fonksiyonları
int model_create_terminal_process(ShmBuf* shmp, int terminal_id);
int model_send_command_to_terminal(ShmBuf* shmp, int terminal_id, const char* command);
int model_read_output_from_terminal(ShmBuf* shmp, int terminal_id, char* output, size_t output_size);
int model_terminate_terminal_process(ShmBuf* shmp, int terminal_id);
int model_check_terminal_process(ShmBuf* shmp, int terminal_id);

#endif /* MODEL_H */
