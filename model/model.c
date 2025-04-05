#include "model.h"

// Model başlatma
ShmBuf* model_init() {
    ShmBuf* shmp;
    

    // Toplam boyutu hesapla: sabit kısım + esnek dizi için alan
    size_t total_size = sizeof(ShmBuf) + BUF_SIZE;

    // Paylaşılan bellek dosyasını aç
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
    shmp->cnt = 0;
    shmp->buf_size = BUF_SIZE;
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
        execl("/bin/sh", "sh", "-c", command, NULL);
        
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
    if (sem_wait(&shmp->sem) == -1) {
        perror("sem_wait failed");
        return NULL;
    }
    // Mesajı paylaşılan belleğe kopyala
    if (shmp->cnt + msg_len + 1 > shmp->buf_size ) {
        size_t new_buf_size = shmp->buf_size * 2;  // Örneğin, boyutu iki katına çıkar
        size_t old_total_size = sizeof(ShmBuf) + shmp->buf_size;
        size_t new_total_size = sizeof(ShmBuf) + new_buf_size;
        // Dosya boyutunu arttır
        if (ftruncate(shmp->fd, new_total_size) == -1) {
            perror("ftruncate failed during resize");
            sem_post(&shmp->sem);
            return NULL;
        }
        // Belleği yeniden eşle
        ShmBuf* new_shmp = mremap(shmp, old_total_size, new_total_size, MREMAP_MAYMOVE);
        if (new_shmp == MAP_FAILED) {
            perror("mremap failed");
            sem_post(&shmp->sem);
            return NULL;
        }
        // mremap yeni adresi döndürdüğü için shmp'yi güncelleyin.
        shmp = new_shmp;
        shmp->buf_size = new_buf_size;
    }
    
    memcpy(&shmp->msgbuf[shmp->cnt], message, msg_len);
    shmp->cnt += msg_len;
    shmp->msgbuf[shmp->cnt] = '\0';  // Satır sonu ekle
    shmp->cnt++;
    
    // Semafor serbest bırak
    if (sem_post(&shmp->sem) == -1) {
        perror("sem_post failed");
        return NULL;
    }
    
    return shmp;
}

// Mesajları okuma
int model_read_messages(ShmBuf* shmp, char* last_message, size_t buffer_size) {
    char buffer[shmp->buf_size];
    if (!shmp || !buffer) {
        return -1;
    }
    
    // Semafor kilitle
    if (sem_wait(&shmp->sem) == -1) {
        perror("sem_wait failed");
        return -1;
    }
    
    // Mesajları buffer'a kopyala
    size_t copy_size = (shmp->cnt < buffer_size - 1) ? shmp->cnt : buffer_size - 1;
    memcpy(buffer, shmp->msgbuf, copy_size);

    size_t pos = 0;
    while (pos < copy_size) {
        printf("%s\n", buffer + pos);
        strcpy(last_message,buffer+pos);
        pos += strlen(buffer + pos) + 1;
    }
    
    // Semafor serbest bırak
    if (sem_post(&shmp->sem) == -1) {
        perror("sem_post failed");
        return -1;
    }
    
    return copy_size;
}

// Temizleme
void model_cleanup(ShmBuf* shmp) {
    if (!shmp) return;

    // Semafor yok et    
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


}





//yapılacaklar tampon doluysa yer ayırma ,string-null sonlandırma ve exec shı düzeltme
/*
int model_execute_command(const char* command, char* output, size_t output_size) {
 fonksiyonunda sha göndermeden kendin redirection yapabilirsin

 Proje, fork(), execvp() ve yönlendirmeleri (dup2() ile) kendi kodunla yönetmeni bekliyor gibi görünüyor. /bin/sh -c kullanmak, bu işleri shell’e yaptırdığı için, sistem programlama becerilerini tam olarak sergilemene engel olabilir.
Özellikle "redirection and pipes" için dup2() ve pipe() kullanımına dair örnekler verilmiş, bu da kendi implementasyonunu yapman gerektiğine işaret ediyor.
*/

/*
mesaj gönderme de
Eksik veya İyileştirilecek Noktalar:
Tampon Dolduğunda Davranış: Şu an cnt = 0 ile eski veriler siliniyor. Bu, basit ama veri kaybına yol açıyor. Alternatif olarak:
Dikkat: Bu yaklaşım, mevcut mesajların üzerine yazılmasına neden olur. Yani eski mesajlar kaybolur. Proje gereksinimlerinde "mesaj geçmişi" tutulması bekleniyorsa, bu bir sorun olabilir. Alternatif olarak:
Tampon doluysa hata döndürebilirsin (return -1).
Ya da eski mesajları kaydırıp yenisine yer açabilirsin (ring buffer tarzı)
Mesajı reddet (return -2 gibi).
Döngüsel tampon uygula (baş ve son işaretçileri ile).
Büyük Mesajlar: msg_len > BUF_SIZE - sizeof(ShmBuf) durumunda mesajı kırpmak veya hata döndürmek iyi olabilir.
*/


