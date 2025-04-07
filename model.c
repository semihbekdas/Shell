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
    if (!command || !output || output_size == 0) {
        return -1;
    }
    
    // Boş komut kontrolü
    if (strlen(command) == 0) {
        output[0] = '\0';
        return 0;
    }
    
    // Boru operatörlerini kontrol et
    int has_pipe = 0;
    for (const char* p = command; *p; p++) {
        if (*p == '|') {
            has_pipe = 1;
            break;
        }
    }
    
    // Yönlendirme operatörlerini kontrol et
    int has_input_redir = 0;
    int has_output_redir = 0;
    int append_mode = 0;
    char input_file[256] = {0};
    char output_file[256] = {0};
    
    // Komut kopyası oluştur
    char cmd_copy[4096];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    // Yönlendirme operatörlerini işle
    char* cmd_part = cmd_copy;
    char* input_redir = strstr(cmd_copy, "<");
    char* output_redir = strstr(cmd_copy, ">");
    
    // Giriş yönlendirme
    if (input_redir) {
        has_input_redir = 1;
        *input_redir = '\0'; // Komut kısmını ayır
        
        // Dosya adını al
        char* file_start = input_redir + 1;
        while (*file_start && (*file_start == ' ' || *file_start == '\t')) file_start++;
        
        // Dosya adının sonunu bul
        char* file_end = file_start;
        while (*file_end && *file_end != ' ' && *file_end != '\t' && *file_end != '>' && *file_end != '|') file_end++;
        
        // Dosya adını kopyala
        if (file_end > file_start) {
            char temp = *file_end;
            *file_end = '\0';
            strncpy(input_file, file_start, sizeof(input_file) - 1);
            input_file[sizeof(input_file) - 1] = '\0';
            *file_end = temp;
        }
    }
    
    // Çıkış yönlendirme
    if (output_redir) {
        has_output_redir = 1;
        
        // Ekleme modunu kontrol et
        if (output_redir[1] == '>') {
            append_mode = 1;
            output_redir++;
        }
        
        *output_redir = '\0'; // Komut kısmını ayır
        
        // Dosya adını al
        char* file_start = output_redir + 1;
        while (*file_start && (*file_start == ' ' || *file_start == '\t')) file_start++;
        
        // Dosya adının sonunu bul
        char* file_end = file_start;
        while (*file_end && *file_end != ' ' && *file_end != '\t' && *file_end != '<' && *file_end != '|') file_end++;
        
        // Dosya adını kopyala
        if (file_end > file_start) {
            char temp = *file_end;
            *file_end = '\0';
            strncpy(output_file, file_start, sizeof(output_file) - 1);
            output_file[sizeof(output_file) - 1] = '\0';
            *file_end = temp;
        }
    }
    
    // Borulu komut işleme
    if (has_pipe) {
        // Komutları ayır
        char* commands[64] = {0}; // En fazla 64 komut
        int cmd_count = 0;
        
        char* token = strtok(cmd_copy, "|");
        while (token && cmd_count < 64) {
            // Baştaki ve sondaki boşlukları temizle
            char* start = token;
            while (*start && (*start == ' ' || *start == '\t')) start++;
            
            char* end = start + strlen(start) - 1;
            while (end > start && (*end == ' ' || *end == '\t')) {
                *end = '\0';
                end--;
            }
            
            if (*start) {
                commands[cmd_count++] = start;
            }
            
            token = strtok(NULL, "|");
        }
        
        if (cmd_count == 0) {
            return -1;
        }
        
        // Borular oluştur
        int pipes[64][2]; // En fazla 64 boru
        
        for (int i = 0; i < cmd_count - 1; i++) {
            if (pipe(pipes[i]) == -1) {
                perror("pipe failed");
                return -1;
            }
        }
        
        // Son komutun çıktısını yakalamak için boru
        int final_pipe[2];
        if (pipe(final_pipe) == -1) {
            perror("final pipe failed");
            return -1;
        }
        
        // Her komut için çocuk süreç oluştur
        pid_t pids[64]; // En fazla 64 süreç
        
        for (int i = 0; i < cmd_count; i++) {
            pids[i] = fork();
            
            if (pids[i] == -1) {
                perror("fork failed");
                return -1;
            }
            
            if (pids[i] == 0) {
                // Çocuk süreç
                
                // Giriş yönlendirme (ilk komut için)
                if (i == 0 && has_input_redir) {
                    int fd = open(input_file, O_RDONLY);
                    if (fd == -1) {
                        perror("open input file failed");
                        exit(EXIT_FAILURE);
                    }
                    
                    if (dup2(fd, STDIN_FILENO) == -1) {
                        perror("dup2 input file failed");
                        exit(EXIT_FAILURE);
                    }
                    
                    close(fd);
                } else if (i > 0) {
                    // Önceki komutun çıkışını bu komutun girişine bağla
                    if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
                        perror("dup2 pipe input failed");
                        exit(EXIT_FAILURE);
                    }
                }
                
                // Çıkış yönlendirme (son komut için)
                if (i == cmd_count - 1) {
                    if (has_output_redir) {
                        int flags = O_WRONLY | O_CREAT;
                        if (append_mode) {
                            flags |= O_APPEND;
                        } else {
                            flags |= O_TRUNC;
                        }
                        
                        int fd = open(output_file, flags, 0666);
                        if (fd == -1) {
                            perror("open output file failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            perror("dup2 output file failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        if (dup2(fd, STDERR_FILENO) == -1) {
                            perror("dup2 output file for stderr failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        close(fd);
                    } else {
                        // Son komutun çıkışını final_pipe'a yönlendir
                        if (dup2(final_pipe[1], STDOUT_FILENO) == -1) {
                            perror("dup2 final pipe output failed");
                            exit(EXIT_FAILURE);
                        }
                        
                        if (dup2(final_pipe[1], STDERR_FILENO) == -1) {
                            perror("dup2 final pipe output for stderr failed");
                            exit(EXIT_FAILURE);
                        }
                    }
                } else {
                    // Ara komutların çıkışını sonraki komutun girişine bağla
                    if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                        perror("dup2 pipe output failed");
                        exit(EXIT_FAILURE);
                    }
                }
                
                // Tüm boruları kapat
                for (int j = 0; j < cmd_count - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                close(final_pipe[0]);
                close(final_pipe[1]);
                
                // Komutu argümanlara ayır
                char* args[64] = {0}; // En fazla 64 argüman
                int arg_count = 0;
                
                char* cmd_token = strtok(commands[i], " \t");
                while (cmd_token && arg_count < 63) {
                    args[arg_count++] = cmd_token;
                    cmd_token = strtok(NULL, " \t");
                }
                args[arg_count] = NULL; // Son eleman NULL olmalı
                
                // Komutu çalıştır
                execvp(args[0], args);
                
                // execvp başarısız olursa buraya ulaşır
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
        }
        
        // Ebeveyn süreç: tüm boruları kapat
        for (int i = 0; i < cmd_count - 1; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
        close(final_pipe[1]); // Yazma ucunu kapat
        
        // Son komutun çıktısını oku (eğer çıkış yönlendirme yoksa)
        if (!has_output_redir) {
            ssize_t bytes_read = read(final_pipe[0], output, output_size - 1);
            if (bytes_read == -1) {
                perror("read failed");
                close(final_pipe[0]);
                return -1;
            }
            
            // Null-terminate
            output[bytes_read] = '\0';
        } else {
            // Çıkış yönlendirme varsa, çıktı dosyaya yazılır
            output[0] = '\0';
        }
        
        close(final_pipe[0]);
        
        // Tüm çocuk süreçlerin tamamlanmasını bekle
        int last_status = 0;
        for (int i = 0; i < cmd_count; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            
            // Son komutun çıkış kodunu kaydet
            if (i == cmd_count - 1) {
                last_status = WEXITSTATUS(status);
            }
        }
        
        return last_status;
    } else {
        // Tek komut (boru olmadan)
        
        // Komut argümanlarını ayır
        char* args[64] = {0}; // En fazla 64 argüman
        int arg_count = 0;
        
        char* token = strtok(cmd_part, " \t");
        while (token && arg_count < 63) {
            args[arg_count++] = token;
            token = strtok(NULL, " \t");
        }
        args[arg_count] = NULL; // Son eleman NULL olmalı
        
        if (arg_count == 0) {
            return -1;
        }
        
        // Çıktıyı yakalamak için boru oluştur (eğer çıkış yönlendirme yoksa)
        int pipefd[2];
        if (!has_output_redir) {
            if (pipe(pipefd) == -1) {
                perror("pipe failed");
                return -1;
            }
        }
        
        // Çocuk süreç oluştur
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork failed");
            if (!has_output_redir) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            return -1;
        }
        
        if (pid == 0) {
            // Çocuk süreç
            
            // Giriş yönlendirme
            if (has_input_redir) {
                int fd = open(input_file, O_RDONLY);
                if (fd == -1) {
                    perror("open input file failed");
                    exit(EXIT_FAILURE);
                }
                
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2 input file failed");
                    exit(EXIT_FAILURE);
                }
                
                close(fd);
            }
            
            // Çıkış yönlendirme
            if (has_output_redir) {
                int flags = O_WRONLY | O_CREAT;
                if (append_mode) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }
                
                int fd = open(output_file, flags, 0666);
                if (fd == -1) {
                    perror("open output file failed");
                    exit(EXIT_FAILURE);
                }
                
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2 output file failed");
                    exit(EXIT_FAILURE);
                }
                
                if (dup2(fd, STDERR_FILENO) == -1) {
                    perror("dup2 output file for stderr failed");
                    exit(EXIT_FAILURE);
                }
                
                close(fd);
            } else {
                // Çıktıyı boruya yönlendir
                close(pipefd[0]); // Okuma ucunu kapat
                
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    perror("dup2 pipe output failed");
                    exit(EXIT_FAILURE);
                }
                
                if (dup2(pipefd[1], STDERR_FILENO) == -1) {
                    perror("dup2 pipe output for stderr failed");
                    exit(EXIT_FAILURE);
                }
                
                close(pipefd[1]);
            }
            
            // Komutu çalıştır
            execvp(args[0], args);
            
            // execvp başarısız olursa buraya ulaşır
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else {
            // Ebeveyn süreç
            
            if (!has_output_redir) {
                close(pipefd[1]); // Yazma ucunu kapat
                
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
            } else {
                // Çıkış yönlendirme varsa, çıktı dosyaya yazılır
                output[0] = '\0';
            }
            
            // Çocuk sürecin tamamlanmasını bekle
            int status;
            waitpid(pid, &status, 0);
            
            return WEXITSTATUS(status);
        }
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
