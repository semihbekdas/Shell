#include "model.h"

// Model başlatma
ShmBuf* model_init() {
    ShmBuf* shmp;
    
    // Toplam boyutu hesapla: sabit kısım + esnek dizi için alan
    size_t total_size = sizeof(ShmBuf) + BUF_SIZE;

    #ifdef __APPLE__
    // macOS için alternatif paylaşılan bellek yaklaşımı
    // Dosya tabanlı paylaşılan bellek yerine doğrudan bellek ayırma
    shmp = (ShmBuf*)malloc(total_size);
    if (!shmp) {
        perror("malloc failed");
        return NULL;
    }
    
    // Named semaphore oluştur
    sem_unlink("/mysem"); // Önceki semafor varsa temizle
    sem_t *sem = sem_open("/mysem", O_CREAT, 0600, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        free(shmp);
        return NULL;
    }
    
    // Semafor işaretçisini kaydet
    shmp->sem_ptr = sem;
    shmp->fd = -1; // macOS'ta dosya kullanmıyoruz
    
    #else
    // Linux için orijinal paylaşılan bellek yaklaşımı
    int fd = shm_open(SHARED_FILE_PATH, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open failed");
        return NULL;
    }
    
    // Dosya boyutunu ayarla
    if (ftruncate(fd, total_size) == -1) {
        perror("ftruncate failed");
        close(fd);
        return NULL;
    }
    
    // Paylaşılan belleği eşle
    shmp = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmp == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return NULL;
    }
    
    // Semafor başlatma
    if (sem_init(&shmp->sem, 1, 1) == -1) {
        perror("sem_init failed");
        munmap(shmp, total_size);
        close(fd);
        return NULL;
    }
    
    shmp->fd = fd;
    shmp->sem_ptr = NULL;
    #endif
    
    // Ortak başlatma
    shmp->cnt = 0;
    shmp->buf_size = BUF_SIZE;
    shmp->last_read_pos = 0;
    memset(shmp->msgbuf, 0, BUF_SIZE);
    
    return shmp;
}

// Komut çalıştırma
int model_execute_command(const char* command, char* output, size_t output_size) {
    int pipefd[2];
    pid_t pid;
    
    // Boru oluştur
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return -1;
    }
    
    // Çocuk süreç oluştur
    pid = fork();
    if (pid == -1) {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Çocuk süreç
        close(pipefd[0]);  // Okuma ucunu kapat
        
        // stdout'u boruya yönlendir
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        
        // stderr'i boruya yönlendir
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        
        close(pipefd[1]);  // Artık gerekli değil
        
        // Komutu çalıştır
        #ifdef __APPLE__
        execl("/bin/bash", "bash", "-c", command, NULL);
        #else
        execl("/bin/sh", "sh", "-c", command, NULL);
        #endif
        
        // execl başarısız olursa buraya ulaşır
        perror("execl failed");
        exit(EXIT_FAILURE);
    } else {
        // Ebeveyn süreç
        close(pipefd[1]);  // Yazma ucunu kapat
        
        // Çocuk sürecin çıktısını oku
        ssize_t bytes_read = read(pipefd[0], output, output_size - 1);
        if (bytes_read == -1) {
            perror("read failed");
            close(pipefd[0]);
            return -1;
        }
        
        // Null-terminate
        output[bytes_read] = '\0';
        
        close(pipefd[0]);
        
        // Çocuk sürecin tamamlanmasını bekle
        int status;
        waitpid(pid, &status, 0);
        
        return WEXITSTATUS(status);
    }
}

// Mesaj gönderme
ShmBuf* model_send_message(ShmBuf* shmp, const char* message) {
    if (!shmp || !message) {
        return NULL;
    }
    
    size_t msg_len = strlen(message);
    
    // Semafor kilitle
    #ifdef __APPLE__
    if (sem_wait(shmp->sem_ptr) == -1) {
    #else
    if (sem_wait(&shmp->sem) == -1) {
    #endif
        perror("sem_wait failed");
        return NULL;
    }
    
    // Tampon doluysa başa dön
    if (shmp->cnt + msg_len + 1 > shmp->buf_size) {
        // Buffer dolu, başa dön
        shmp->cnt = 0;
        shmp->last_read_pos = 0;
    }
    
    // Mesajı paylaşılan belleğe kopyala
    memcpy(&shmp->msgbuf[shmp->cnt], message, msg_len);
    shmp->cnt += msg_len;
    shmp->msgbuf[shmp->cnt] = '\0';  // Null terminator ekle
    shmp->cnt++;
    
    // Semafor serbest bırak
    #ifdef __APPLE__
    if (sem_post(shmp->sem_ptr) == -1) {
    #else
    if (sem_post(&shmp->sem) == -1) {
    #endif
        perror("sem_post failed");
        return NULL;
    }
    
    return shmp;
}

// Mesajları okuma
int model_read_messages(ShmBuf* shmp, char* last_message, size_t buffer_size) {
    if (!shmp) {
        return -1;
    }
    
    // Semafor kilitle
    #ifdef __APPLE__
    if (sem_wait(shmp->sem_ptr) == -1) {
    #else
    if (sem_wait(&shmp->sem) == -1) {
    #endif
        perror("sem_wait failed");
        return -1;
    }
    
    // Yeni mesaj var mı kontrol et
    if (shmp->last_read_pos >= shmp->cnt) {
        // Yeni mesaj yok
        #ifdef __APPLE__
        if (sem_post(shmp->sem_ptr) == -1) {
        #else
        if (sem_post(&shmp->sem) == -1) {
        #endif
            perror("sem_post failed");
        }
        return 0;
    }
    
    // Son mesajı bul
    size_t pos = shmp->last_read_pos;
    size_t last_pos = pos;
    
    while (pos < shmp->cnt) {
        last_pos = pos;
        pos += strlen(&shmp->msgbuf[pos]) + 1;
    }
    
    // Son mesajı kopyala
    if (last_pos < shmp->cnt) {
        strncpy(last_message, &shmp->msgbuf[last_pos], buffer_size - 1);
        last_message[buffer_size - 1] = '\0';
        
        // Son okunan pozisyonu güncelle
        shmp->last_read_pos = shmp->cnt;
    }
    
    // Semafor serbest bırak
    #ifdef __APPLE__
    if (sem_post(shmp->sem_ptr) == -1) {
    #else
    if (sem_post(&shmp->sem) == -1) {
    #endif
        perror("sem_post failed");
        return -1;
    }
    
    return 1; // Bir mesaj okundu
}

// Temizleme
void model_cleanup(ShmBuf* shmp) {
    if (!shmp) return;

    #ifdef __APPLE__
    // macOS'ta named semaphore temizle
    if (shmp->sem_ptr) {
        sem_close(shmp->sem_ptr);
        sem_unlink("/mysem");
    }
    
    // macOS'ta doğrudan bellek temizle
    free(shmp);
    #else
    // Linux'ta unnamed semaphore temizle
    if (sem_destroy(&shmp->sem) == -1) {
        perror("model_cleanup: sem_destroy failed");
    }

    // Paylaşılan bellek dosyasını kapat ve kaldır
    if (close(shmp->fd) == -1) {
        perror("model_cleanup: close failed");
    }
    
    // Paylaşılan belleği kaldır
    if (munmap(shmp, sizeof(ShmBuf) + BUF_SIZE) == -1) {
        perror("model_cleanup: munmap failed");
    }

    if (shm_unlink(SHARED_FILE_PATH) == -1) {
        perror("model_cleanup: shm_unlink failed");
    }
    #endif
}
