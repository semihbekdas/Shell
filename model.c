#include "model.h"
#include <errno.h>
#include <limits.h>  // PATH_MAX için
#include <signal.h>  // Sinyal işleme için
#include <fcntl.h>   // O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, O_APPEND için

// Global değişken - her terminal için paylaşılan bellek işaretçisi
static ShmBuf* g_shm_buffer = NULL;

// Pipe tamponlarını temizle - daha agresif temizleme
void flush_pipe(int pipe_fd) {
    if (pipe_fd < 0) return;
    
    // Non-blocking moda geçir
    int flags = fcntl(pipe_fd, F_GETFL, 0);
    fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Pipe'ı boşalt - daha büyük tampon ve daha fazla döngü
    char buffer[8192]; // Daha büyük tampon (8KB)
    ssize_t bytes;
    int total_flushed = 0;
    int flush_attempts = 0;
    
    // Daha fazla veri temizleme ve daha fazla deneme
    while (flush_attempts < 100) { // 100 deneme
        bytes = read(pipe_fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            total_flushed += bytes;
            // 50MB'dan fazla veri temizlendiyse döngüden çık (sonsuz döngü önlemi)
            if (total_flushed > 50 * 1024 * 1024) break;
        } else if (bytes == 0 || (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
            // Veri kalmadı veya hazır değil, kısa bir süre bekle ve tekrar dene
            usleep(1000); // 1ms bekle
            flush_attempts++;
        } else {
            // Diğer hatalar
            break;
        }
    }
    
    // Orijinal moda geri dön
    fcntl(pipe_fd, F_SETFL, flags);
}

// Komut geçerliliğini kontrol et
int is_valid_command(const char* command) {
    if (!command || strlen(command) == 0) return 0;
    
    // Sadece boşluk karakterlerinden oluşan komutları kontrol et
    int only_whitespace = 1;
    for (const char* p = command; *p; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            only_whitespace = 0;
            break;
        }
    }
    
    return !only_whitespace;
}

// İnteraktif komut kontrolü
int is_interactive_command(const char* command) {
    if (!command) return 0;
    
    // Komut argümanlarını ayır
    char cmd_copy[4096];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    char* args[64] = {0}; // En fazla 64 argüman
    int arg_count = 0;
    
    char* token = strtok(cmd_copy, " \t");
    while (token && arg_count < 63) {
        args[arg_count++] = token;
        token = strtok(NULL, " \t");
    }
    
    if (arg_count == 0) return 0;
    
    // İnteraktif komutları kontrol et
    if (strcmp(args[0], "cat") == 0 && arg_count == 1) {
        // Argümansız cat komutu interaktiftir
        return 1;
    }
    
    // Diğer interaktif komutlar da eklenebilir
    if (strcmp(args[0], "read") == 0) return 1;
    if (strcmp(args[0], "more") == 0) return 1;
    if (strcmp(args[0], "less") == 0) return 1;
    if (strcmp(args[0], "nano") == 0) return 1;
    if (strcmp(args[0], "vim") == 0) return 1;
    if (strcmp(args[0], "vi") == 0) return 1;
    if (strcmp(args[0], "wc") == 0 && arg_count == 1) return 1;
    
    return 0;
}

// CD komutu kontrolü
int is_cd_command(const char* command, char* directory) {
    if (!command || !directory) return 0;
    
    // CD komutu kontrolü
    if (strncmp(command, "cd", 2) == 0 && (command[2] == ' ' || command[2] == '\0')) {
        const char* dir = command + 2;
        
        // Baştaki boşlukları atla
        while (*dir == ' ') dir++;
        
        // Boş dizin kontrolü
        if (strlen(dir) == 0) {
            dir = getenv("HOME") ? getenv("HOME") : ".";
        }
        
        // Dizini kopyala
        strcpy(directory, dir);
        return 1;
    }
    
    return 0;
}

// Terminal süreç fonksiyonu
void terminal_process_main(int terminal_id, int read_pipe, int write_pipe) {
    char command[4096];
    char output[65536]; // Çıktı tampon boyutu artırıldı (64KB)
    char current_dir[PATH_MAX];
    
    // Başlangıç dizinini al
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        strcpy(current_dir, getenv("HOME") ? getenv("HOME") : ".");
    }
    
    // Başlangıç mesajı gönder
    snprintf(output, sizeof(output), "Terminal %d başlatıldı. Çalışma dizini: %s\n", terminal_id + 1, current_dir);
    write(write_pipe, output, strlen(output) + 1);
    
    // Ana döngü
    while (1) {
        ssize_t bytes_read;
        
        // Komut bekle
        memset(command, 0, sizeof(command));
        bytes_read = read(read_pipe, command, sizeof(command) - 1);
        
        if (bytes_read <= 0) {
            // Pipe kapandı veya hata oluştu, terminali sonlandır
            break;
        }
        
        // Komut geçerliliğini kontrol et
        if (!is_valid_command(command)) {
            snprintf(output, sizeof(output), "Geçersiz komut: Boş veya sadece boşluk karakterleri içeren komut.\n");
            write(write_pipe, output, strlen(output) + 1);
            continue;
        }
        
        // Çıkış komutu kontrolü
        if (strcmp(command, "exit") == 0) {
            snprintf(output, sizeof(output), "Terminal %d kapatılıyor...\n", terminal_id + 1);
            write(write_pipe, output, strlen(output) + 1);
            break;
        }
        
        // Sıfırlama komutu kontrolü - reset komutu kaldırıldı
        if (strcmp(command, "clear") == 0) {
            // Terminali sıfırla ve pipe'ları temizle
            flush_pipe(read_pipe);
            
            // Çıktı tamponunu temizle
            memset(output, 0, sizeof(output));
            
            // Özel sıfırlama mesajı gönder - özel bir işaretleyici ile
            snprintf(output, sizeof(output), "\x1B[2J\x1B[H\x1B[3J\nTerminal %d sıfırlandı.\n", terminal_id + 1);
            write(write_pipe, output, strlen(output) + 1);
            
            // Pipe'ı tamamen temizlemek için boş bir null terminatör gönder
            char null_term = '\0';
            write(write_pipe, &null_term, 1);
            
            continue;
        }
        
        // CD komutu kontrolü
        char directory[PATH_MAX];
        if (is_cd_command(command, directory)) {
            // Dizini değiştir
            if (chdir(directory) == 0) {
                // Başarılı
                if (getcwd(current_dir, sizeof(current_dir)) != NULL) {
                    snprintf(output, sizeof(output), "Dizin değiştirildi: %s\n", current_dir);
                } else {
                    snprintf(output, sizeof(output), "Dizin değiştirildi: %s\n", directory);
                }
            } else {
                // Başarısız
                snprintf(output, sizeof(output), "Dizin değiştirilemedi: %s (%s)\n", directory, strerror(errno));
            }
            
            write(write_pipe, output, strlen(output) + 1);
            continue;
        }
        
        // İnteraktif komut kontrolü
        if (is_interactive_command(command)) {
            // İnteraktif komut için özel mesaj
            snprintf(output, sizeof(output), 
                    "İnteraktif komut tespit edildi: %s\n"
                    "Bu komut kullanıcı girdisi bekliyor ve terminal arayüzünde doğrudan çalıştırılamaz.\n"
                    "Lütfen dosya yönlendirme kullanın (örn: cat < dosya.txt) veya argüman belirtin (örn: cat dosya.txt).\n", 
                    command);
            write(write_pipe, output, strlen(output) + 1);
            continue;
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
                snprintf(output, sizeof(output), "Geçersiz borulu komut.\n");
                write(write_pipe, output, strlen(output) + 1);
                continue;
            }
            
            // Borular oluştur
            int pipes[64][2]; // En fazla 64 boru
            int pipes_created = 0;
            
            for (int i = 0; i < cmd_count - 1; i++) {
                if (pipe(pipes[i]) == -1) {
                    snprintf(output, sizeof(output), "Pipe oluşturulamadı: %s\n", strerror(errno));
                    // Oluşturulan boruları temizle
                    for (int j = 0; j < pipes_created; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                    write(write_pipe, output, strlen(output) + 1);
                    goto next_command;
                }
                pipes_created++;
            }
            
            // Son komutun çıktısını yakalamak için boru
            int final_pipe[2];
            if (pipe(final_pipe) == -1) {
                snprintf(output, sizeof(output), "Final pipe oluşturulamadı: %s\n", strerror(errno));
                // Oluşturulan boruları temizle
                for (int j = 0; j < pipes_created; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                write(write_pipe, output, strlen(output) + 1);
                goto next_command;
            }
            
            // Her komut için çocuk süreç oluştur
            pid_t pids[64]; // En fazla 64 süreç
            int processes_created = 0;
            
            for (int i = 0; i < cmd_count; i++) {
                pids[i] = fork();
                
                if (pids[i] == -1) {
                    snprintf(output, sizeof(output), "Fork hatası: %s\n", strerror(errno));
                    // Oluşturulan süreçleri sonlandır
                    for (int j = 0; j < processes_created; j++) {
                        kill(pids[j], SIGTERM);
                    }
                    // Oluşturulan boruları temizle
                    for (int j = 0; j < pipes_created; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                    close(final_pipe[0]);
                    close(final_pipe[1]);
                    write(write_pipe, output, strlen(output) + 1);
                    goto next_command;
                }
                
                if (pids[i] == 0) {
                    // Çocuk süreç
                    
                    // Giriş yönlendirme (ilk komut için)
                    if (i == 0 && has_input_redir) {
                        int fd = open(input_file, O_RDONLY);
                        if (fd == -1) {
                            fprintf(stderr, "Giriş dosyası açılamadı: %s (%s)\n", input_file, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        
                        if (dup2(fd, STDIN_FILENO) == -1) {
                            fprintf(stderr, "dup2 giriş dosyası hatası: %s\n", strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        
                        close(fd);
                    } else if (i > 0) {
                        // Önceki komutun çıkışını bu komutun girişine bağla
                        if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
                            fprintf(stderr, "dup2 pipe giriş hatası: %s\n", strerror(errno));
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
                                fprintf(stderr, "Çıkış dosyası açılamadı: %s (%s)\n", output_file, strerror(errno));
                                exit(EXIT_FAILURE);
                            }
                            
                            if (dup2(fd, STDOUT_FILENO) == -1) {
                                fprintf(stderr, "dup2 çıkış dosyası hatası: %s\n", strerror(errno));
                                exit(EXIT_FAILURE);
                            }
                            
                            if (dup2(fd, STDERR_FILENO) == -1) {
                                fprintf(stderr, "dup2 çıkış dosyası stderr hatası: %s\n", strerror(errno));
                                exit(EXIT_FAILURE);
                            }
                            
                            close(fd);
                        } else {
                            // Son komutun çıkışını final_pipe'a yönlendir
                            if (dup2(final_pipe[1], STDOUT_FILENO) == -1) {
                                fprintf(stderr, "dup2 final pipe çıkış hatası: %s\n", strerror(errno));
                                exit(EXIT_FAILURE);
                            }
                            
                            if (dup2(final_pipe[1], STDERR_FILENO) == -1) {
                                fprintf(stderr, "dup2 final pipe stderr çıkış hatası: %s\n", strerror(errno));
                                exit(EXIT_FAILURE);
                            }
                        }
                    } else {
                        // Ara komutların çıkışını sonraki komutun girişine bağla
                        if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                            fprintf(stderr, "dup2 pipe çıkış hatası: %s\n", strerror(errno));
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
                    
                    char cmd_token_copy[1024];
                    strncpy(cmd_token_copy, commands[i], sizeof(cmd_token_copy) - 1);
                    cmd_token_copy[sizeof(cmd_token_copy) - 1] = '\0';
                    
                    char* cmd_token = strtok(cmd_token_copy, " \t");
                    while (cmd_token && arg_count < 63) {
                        args[arg_count++] = cmd_token;
                        cmd_token = strtok(NULL, " \t");
                    }
                    args[arg_count] = NULL; // Son eleman NULL olmalı
                    
                    // Çalışma dizinini ayarla
                    chdir(current_dir);
                    
                    // Komutu çalıştır
                    if (arg_count > 0) {
                        execvp(args[0], args);
                        // execvp başarısız olursa buraya ulaşır
                        fprintf(stderr, "Komut çalıştırılamadı: %s (%s)\n", args[0], strerror(errno));
                    } else {
                        fprintf(stderr, "Geçersiz komut: Argüman bulunamadı\n");
                    }
                    
                    exit(EXIT_FAILURE);
                }
                processes_created++;
            }
            
            // Ebeveyn süreç
            
            // Tüm boruları kapat
            for (int i = 0; i < cmd_count - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            close(final_pipe[1]);
            
            // Çıkış yönlendirme varsa, çıktı dosyaya yazılır
            if (has_output_redir) {
                snprintf(output, sizeof(output), "Çıktı '%s' dosyasına yönlendirildi.\n", output_file);
                write(write_pipe, output, strlen(output) + 1);
            } else {
                // Çıktıyı oku
                char temp_buffer[8192]; // Geçici tampon
                
                // Tüm çocuk süreçlerin tamamlanmasını bekle
                for (int i = 0; i < cmd_count; i++) {
                    int status;
                    waitpid(pids[i], &status, 0);
                }
                
                // Çıktıyı doğrudan ilet - tampon kullanmadan
                ssize_t bytes;
                int total_sent = 0;
                int has_output = 0;
                
                while ((bytes = read(final_pipe[0], temp_buffer, sizeof(temp_buffer) - 1)) > 0) {
                    has_output = 1;
                    temp_buffer[bytes] = '\0';
                    write(write_pipe, temp_buffer, bytes);
                    total_sent += bytes;
                    
                    // Çok büyük çıktılar için güvenlik kontrolü
                    if (total_sent > 10 * 1024 * 1024) { // 10MB'dan fazla veri
                        char overflow_msg[] = "\n... (çıktı çok büyük, kalan kısım kesildi) ...\n";
                        write(write_pipe, overflow_msg, strlen(overflow_msg));
                        break;
                    }
                }
                
                // Çıktı yoksa bilgi mesajı
                if (!has_output) {
                    strcpy(temp_buffer, "Komut tamamlandı (çıktı yok)\n");
                    write(write_pipe, temp_buffer, strlen(temp_buffer) + 1);
                } else {
                    // Null terminatör gönder
                    char null_term = '\0';
                    write(write_pipe, &null_term, 1);
                }
            }
            
            close(final_pipe[0]);
            
        } else {
            // Tek komut (boru olmadan)
            
            // Diğer komutları çalıştır
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                snprintf(output, sizeof(output), "Pipe oluşturulamadı: %s\n", strerror(errno));
                write(write_pipe, output, strlen(output) + 1);
                continue;
            }
            
            pid_t pid = fork();
            
            if (pid == -1) {
                snprintf(output, sizeof(output), "Fork hatası: %s\n", strerror(errno));
                write(write_pipe, output, strlen(output) + 1);
                close(pipefd[0]);
                close(pipefd[1]);
                continue;
            }
            
            if (pid == 0) {
                // Çocuk süreç
                close(pipefd[0]);
                
                // Giriş yönlendirme
                if (has_input_redir) {
                    int fd = open(input_file, O_RDONLY);
                    if (fd == -1) {
                        fprintf(stderr, "Giriş dosyası açılamadı: %s (%s)\n", input_file, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    
                    if (dup2(fd, STDIN_FILENO) == -1) {
                        fprintf(stderr, "dup2 giriş dosyası hatası: %s\n", strerror(errno));
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
                        fprintf(stderr, "Çıkış dosyası açılamadı: %s (%s)\n", output_file, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    
                    if (dup2(fd, STDOUT_FILENO) == -1) {
                        fprintf(stderr, "dup2 çıkış dosyası hatası: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    
                    if (dup2(fd, STDERR_FILENO) == -1) {
                        fprintf(stderr, "dup2 çıkış dosyası stderr hatası: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    
                    close(fd);
                } else {
                    // Standart çıkışı pipe'a yönlendir
                    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                        perror("dup2 stdout failed");
                        exit(EXIT_FAILURE);
                    }
                    
                    if (dup2(pipefd[1], STDERR_FILENO) == -1) {
                        perror("dup2 stderr failed");
                        exit(EXIT_FAILURE);
                    }
                }
                
                close(pipefd[1]);
                
                // Çalışma dizinini ayarla
                chdir(current_dir);
                
                // Komutu çalıştır
                // Doğrudan execvp kullanarak komutu çalıştır
                char* args[64] = {0}; // En fazla 64 argüman
                int arg_count = 0;
                
                char cmd_part_copy[4096];
                strncpy(cmd_part_copy, cmd_part, sizeof(cmd_part_copy) - 1);
                cmd_part_copy[sizeof(cmd_part_copy) - 1] = '\0';
                
                char* cmd_token = strtok(cmd_part_copy, " \t");
                while (cmd_token && arg_count < 63) {
                    args[arg_count++] = cmd_token;
                    cmd_token = strtok(NULL, " \t");
                }
                args[arg_count] = NULL; // Son eleman NULL olmalı
                
                if (arg_count > 0) {
                    execvp(args[0], args);
                    // execvp başarısız olursa buraya ulaşır
                    fprintf(stderr, "Komut çalıştırılamadı: %s (%s)\n", args[0], strerror(errno));
                } else {
                    fprintf(stderr, "Geçersiz komut: Argüman bulunamadı\n");
                }
                
                exit(EXIT_FAILURE);
            }
            
            // Ebeveyn süreç
            close(pipefd[1]);
            
            // Çıkış yönlendirme varsa, çıktı dosyaya yazılır
            if (has_output_redir) {
                snprintf(output, sizeof(output), "Çıktı '%s' dosyasına yönlendirildi.\n", output_file);
                write(write_pipe, output, strlen(output) + 1);
            } else {
                // Çıktıyı oku
                char temp_buffer[8192]; // Geçici tampon
                
                // Çocuk sürecin tamamlanmasını bekle
                int status;
                waitpid(pid, &status, 0);
                
                // Çıktıyı doğrudan ilet - tampon kullanmadan
                ssize_t bytes;
                int total_sent = 0;
                int has_output = 0;
                
                while ((bytes = read(pipefd[0], temp_buffer, sizeof(temp_buffer) - 1)) > 0) {
                    has_output = 1;
                    temp_buffer[bytes] = '\0';
                    write(write_pipe, temp_buffer, bytes);
                    total_sent += bytes;
                    
                    // Çok büyük çıktılar için güvenlik kontrolü
                    if (total_sent > 10 * 1024 * 1024) { // 10MB'dan fazla veri
                        char overflow_msg[] = "\n... (çıktı çok büyük, kalan kısım kesildi) ...\n";
                        write(write_pipe, overflow_msg, strlen(overflow_msg));
                        break;
                    }
                }
                
                // Çıktı yoksa bilgi mesajı
                if (!has_output) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        snprintf(temp_buffer, sizeof(temp_buffer), "Komut çalıştırma hatası (kod: %d)\n", WEXITSTATUS(status));
                    } else {
                        strcpy(temp_buffer, "Komut tamamlandı (çıktı yok)\n");
                    }
                    write(write_pipe, temp_buffer, strlen(temp_buffer) + 1);
                } else {
                    // Null terminatör gönder
                    char null_term = '\0';
                    write(write_pipe, &null_term, 1);
                }
            }
            
            close(pipefd[0]);
        }
        
next_command:
        // Bir sonraki komuta geç
        continue;
    }
    
    // Terminal sürecini sonlandır
    exit(EXIT_SUCCESS);
}

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
    
    // Terminal süreç bilgilerini başlat
    shmp->terminal_count = 0;
    for (int i = 0; i < MAX_TERMINALS; i++) {
        shmp->terminal_processes[i].terminal_id = -1;
        shmp->terminal_processes[i].process_id = -1;
        shmp->terminal_processes[i].pipe_to_terminal[0] = -1;
        shmp->terminal_processes[i].pipe_to_terminal[1] = -1;
        shmp->terminal_processes[i].pipe_from_terminal[0] = -1;
        shmp->terminal_processes[i].pipe_from_terminal[1] = -1;
        shmp->terminal_processes[i].active = false;
    }
    
    // Global değişkene kaydet
    g_shm_buffer = shmp;
    
    return shmp;
}

// Terminal sürecini oluştur
int model_create_terminal_process(ShmBuf* shmp, int terminal_id) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp || terminal_id < 0 || shmp->terminal_count >= MAX_TERMINALS) {
        return -1;
    }
    
    // Terminal zaten kayıtlı mı kontrol et
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].terminal_id == terminal_id && shmp->terminal_processes[i].active) {
            return 0; // Zaten aktif
        }
    }
    
    // Yeni terminal için indeks bul
    int idx = shmp->terminal_count;
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (!shmp->terminal_processes[i].active) {
            idx = i;
            break;
        }
    }
    
    // Pipe'ları oluştur
    if (pipe(shmp->terminal_processes[idx].pipe_to_terminal) == -1) {
        perror("pipe to terminal failed");
        return -1;
    }
    
    if (pipe(shmp->terminal_processes[idx].pipe_from_terminal) == -1) {
        perror("pipe from terminal failed");
        close(shmp->terminal_processes[idx].pipe_to_terminal[0]);
        close(shmp->terminal_processes[idx].pipe_to_terminal[1]);
        return -1;
    }
    
    // Terminal sürecini oluştur
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork failed");
        close(shmp->terminal_processes[idx].pipe_to_terminal[0]);
        close(shmp->terminal_processes[idx].pipe_to_terminal[1]);
        close(shmp->terminal_processes[idx].pipe_from_terminal[0]);
        close(shmp->terminal_processes[idx].pipe_from_terminal[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Çocuk süreç (terminal süreci)
        
        // Kullanılmayan pipe uçlarını kapat
        close(shmp->terminal_processes[idx].pipe_to_terminal[1]);
        close(shmp->terminal_processes[idx].pipe_from_terminal[0]);
        
        // Terminal süreç fonksiyonunu çağır
        terminal_process_main(
            terminal_id,
            shmp->terminal_processes[idx].pipe_to_terminal[0],
            shmp->terminal_processes[idx].pipe_from_terminal[1]
        );
        
        // Bu noktaya ulaşılmamalı
        exit(EXIT_FAILURE);
    }
    
    // Ebeveyn süreç
    
    // Kullanılmayan pipe uçlarını kapat
    close(shmp->terminal_processes[idx].pipe_to_terminal[0]);
    close(shmp->terminal_processes[idx].pipe_from_terminal[1]);
    
    // Terminal bilgilerini kaydet
    shmp->terminal_processes[idx].terminal_id = terminal_id;
    shmp->terminal_processes[idx].process_id = pid;
    shmp->terminal_processes[idx].active = true;
    
    // Terminal sayısını güncelle
    if (idx == shmp->terminal_count) {
        shmp->terminal_count++;
    }
    
    return 0;
}

// Terminal sürecine komut gönder
int model_send_command_to_terminal(ShmBuf* shmp, int terminal_id, const char* command) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp || !command || terminal_id < 0) {
        return -1;
    }
    
    // Terminal indeksini bul
    int idx = -1;
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].terminal_id == terminal_id && shmp->terminal_processes[i].active) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return -1; // Terminal bulunamadı
    }
    
    // Komut gönder
    ssize_t bytes_written = write(shmp->terminal_processes[idx].pipe_to_terminal[1], command, strlen(command) + 1);
    
    if (bytes_written <= 0) {
        return -1; // Yazma hatası
    }
    
    return 0;
}

// Terminal sürecinden çıktı oku
int model_read_output_from_terminal(ShmBuf* shmp, int terminal_id, char* output, size_t output_size) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp || !output || output_size == 0 || terminal_id < 0) {
        return -1;
    }
    
    // Terminal indeksini bul
    int idx = -1;
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].terminal_id == terminal_id && shmp->terminal_processes[i].active) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return -1; // Terminal bulunamadı
    }
    
    // Non-blocking I/O için dosya tanımlayıcısını ayarla
    int flags = fcntl(shmp->terminal_processes[idx].pipe_from_terminal[0], F_GETFL, 0);
    fcntl(shmp->terminal_processes[idx].pipe_from_terminal[0], F_SETFL, flags | O_NONBLOCK);
    
    // Çıktıyı oku
    memset(output, 0, output_size);
    ssize_t bytes_read = read(shmp->terminal_processes[idx].pipe_from_terminal[0], output, output_size - 1);
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Veri henüz hazır değil
            return 0;
        } else {
            // Okuma hatası
            return -1;
        }
    }
    
    return bytes_read;
}

// Terminal sürecini sonlandır
int model_terminate_terminal_process(ShmBuf* shmp, int terminal_id) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp || terminal_id < 0) {
        return -1;
    }
    
    // Terminal indeksini bul
    int idx = -1;
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].terminal_id == terminal_id && shmp->terminal_processes[i].active) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return -1; // Terminal bulunamadı
    }
    
    // Çıkış komutu gönder
    model_send_command_to_terminal(shmp, terminal_id, "exit");
    
    // Sürecin sonlanmasını bekle (non-blocking)
    int status;
    pid_t result = waitpid(shmp->terminal_processes[idx].process_id, &status, WNOHANG);
    
    if (result == 0) {
        // Süreç hala çalışıyor, SIGTERM gönder
        kill(shmp->terminal_processes[idx].process_id, SIGTERM);
        
        // Kısa bir süre bekle
        usleep(50000); // 50ms - Bekleme süresi azaltıldı
        
        // Tekrar kontrol et
        result = waitpid(shmp->terminal_processes[idx].process_id, &status, WNOHANG);
        
        if (result == 0) {
            // Hala çalışıyor, SIGKILL gönder
            kill(shmp->terminal_processes[idx].process_id, SIGKILL);
            
            // Sürecin sonlanmasını bekle
            waitpid(shmp->terminal_processes[idx].process_id, &status, 0);
        }
    }
    
    // Pipe'ları kapat
    if (shmp->terminal_processes[idx].pipe_to_terminal[1] >= 0) {
        close(shmp->terminal_processes[idx].pipe_to_terminal[1]);
        shmp->terminal_processes[idx].pipe_to_terminal[1] = -1;
    }
    
    if (shmp->terminal_processes[idx].pipe_from_terminal[0] >= 0) {
        close(shmp->terminal_processes[idx].pipe_from_terminal[0]);
        shmp->terminal_processes[idx].pipe_from_terminal[0] = -1;
    }
    
    // Terminal bilgilerini sıfırla
    shmp->terminal_processes[idx].terminal_id = -1;
    shmp->terminal_processes[idx].process_id = -1;
    shmp->terminal_processes[idx].active = false;
    
    return 0;
}

// Terminal sürecinin durumunu kontrol et
int model_check_terminal_process(ShmBuf* shmp, int terminal_id) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp || terminal_id < 0) {
        return -1; // Hata
    }
    
    // Terminal indeksini bul
    int idx = -1;
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].terminal_id == terminal_id && shmp->terminal_processes[i].active) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return -1; // Terminal bulunamadı
    }
    
    // Sürecin durumunu kontrol et
    int status;
    pid_t result = waitpid(shmp->terminal_processes[idx].process_id, &status, WNOHANG);
    
    if (result == 0) {
        // Süreç hala çalışıyor
        return 1;
    } else if (result == shmp->terminal_processes[idx].process_id) {
        // Süreç sonlandı
        if (WIFEXITED(status)) {
            // Normal çıkış
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            // Sinyal ile sonlandırıldı
            return -2;
        } else {
            // Diğer durumlar
            return -3;
        }
    } else {
        // Hata
        return -4;
    }
}

// Komut çalıştır
int model_execute_command(const char* command, char* output, size_t output_size, int terminal_id) {
    if (!command || !output || output_size == 0 || terminal_id < 0) {
        return -1;
    }
    
    // Komut geçerliliğini kontrol et
    if (!is_valid_command(command)) {
        snprintf(output, output_size, "Geçersiz komut: Boş veya sadece boşluk karakterleri içeren komut.\n");
        return 0;
    }
    
    // Global paylaşılan bellek işaretçisini kullan
    ShmBuf* shmp = g_shm_buffer;
    if (!shmp) {
        return -1;
    }
    
    // Terminal sürecinin durumunu kontrol et
    int status = model_check_terminal_process(shmp, terminal_id);
    
    // Terminal süreci çalışmıyor veya hata oluşmuşsa yeniden başlat
    if (status != 1) {
        // Eski terminal sürecini sonlandır (eğer hala varsa)
        model_terminate_terminal_process(shmp, terminal_id);
        
        // Yeni terminal süreci oluştur
        if (model_create_terminal_process(shmp, terminal_id) != 0) {
            snprintf(output, output_size, "Terminal %d başlatılamadı!\n", terminal_id + 1);
            return -1;
        }
        
        // Başlangıç mesajını oku
        char init_output[4096] = {0};
        // Bekleme süresi kaldırıldı
        model_read_output_from_terminal(shmp, terminal_id, init_output, sizeof(init_output));
    }
    
    // Terminal indeksini bul
    int idx = -1;
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].terminal_id == terminal_id && shmp->terminal_processes[i].active) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        snprintf(output, output_size, "Terminal %d bulunamadı!\n", terminal_id + 1);
        return -1;
    }
    
    // Clear komutu için özel işlem
    if (strcmp(command, "clear") == 0) {
        // Önce pipe'ları temizle
        flush_pipe(shmp->terminal_processes[idx].pipe_from_terminal[0]);
        
        // Terminali yeniden başlat
        model_terminate_terminal_process(shmp, terminal_id);
        
        if (model_create_terminal_process(shmp, terminal_id) != 0) {
            snprintf(output, output_size, "Terminal %d yeniden başlatılamadı!\n", terminal_id + 1);
            return -1;
        }
        
        // Başlangıç mesajını oku
        char init_output[4096] = {0};
        model_read_output_from_terminal(shmp, terminal_id, init_output, sizeof(init_output));
        
        // Ekranı temizle mesajı
        snprintf(output, output_size, "\x1B[2J\x1B[H\x1B[3J\nTerminal %d sıfırlandı.\n", terminal_id + 1);
        return 0;
    }
    
    // Pipe'ları temizle
    flush_pipe(shmp->terminal_processes[idx].pipe_from_terminal[0]);
    
    // Komutu gönder
    if (model_send_command_to_terminal(shmp, terminal_id, command) != 0) {
        snprintf(output, output_size, "Komut gönderilemedi!\n");
        return -1;
    }
    
    // Çıktıyı oku
    memset(output, 0, output_size);
    
    // Çıktı için bekleme
    int max_attempts = 100; // Maksimum 100 deneme (1 saniye)
    int attempts = 0;
    ssize_t bytes_read = 0;
    
    while (attempts < max_attempts) {
        bytes_read = model_read_output_from_terminal(shmp, terminal_id, output, output_size);
        
        if (bytes_read > 0) {
            // Veri okundu
            break;
        } else if (bytes_read < 0) {
            // Okuma hatası
            return -1;
        }
        
        // Kısa bir süre bekle
        usleep(10000); // 10ms - Bekleme süresi azaltıldı
        attempts++;
    }
    
    if (bytes_read == 0) {
        // Zaman aşımı
        snprintf(output, output_size, "Komut çıktısı alınamadı (zaman aşımı).\n");
        return -1;
    }
    
    return 0;
}

// Mesaj gönder
ShmBuf* model_send_message(ShmBuf* shmp, const char* message) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp || !message) {
        return NULL;
    }
    
    size_t msg_len = strlen(message) + 1; // Null terminatör dahil
    
    // Mesaj çok büyükse hata döndür
    if (msg_len > shmp->buf_size) {
        return NULL;
    }
    
    // Semafor kilitle
    #ifdef __APPLE__
    if (sem_wait(shmp->sem_ptr) == -1) {
        perror("sem_wait failed");
        return NULL;
    }
    #else
    if (sem_wait(&shmp->sem) == -1) {
        perror("sem_wait failed");
        return NULL;
    }
    #endif
    
    // Tampon doluysa başa dön
    if (shmp->cnt + msg_len > shmp->buf_size) {
        shmp->cnt = 0;
    }
    
    // Mesajı kopyala
    memcpy(shmp->msgbuf + shmp->cnt, message, msg_len);
    
    // Sayacı güncelle
    shmp->cnt += msg_len;
    
    // Semafor serbest bırak
    #ifdef __APPLE__
    if (sem_post(shmp->sem_ptr) == -1) {
        perror("sem_post failed");
        return NULL;
    }
    #else
    if (sem_post(&shmp->sem) == -1) {
        perror("sem_post failed");
        return NULL;
    }
    #endif
    
    return shmp;
}

// Mesajları oku
int model_read_messages(ShmBuf* shmp, char* buffer, size_t buffer_size) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp || !buffer || buffer_size == 0) {
        return -1;
    }
    
    // Semafor kilitle
    #ifdef __APPLE__
    if (sem_wait(shmp->sem_ptr) == -1) {
        perror("sem_wait failed");
        return -1;
    }
    #else
    if (sem_wait(&shmp->sem) == -1) {
        perror("sem_wait failed");
        return -1;
    }
    #endif
    
    // Yeni mesaj var mı kontrol et
    if (shmp->last_read_pos >= shmp->cnt) {
        // Yeni mesaj yok
        buffer[0] = '\0';
        
        // Semafor serbest bırak
        #ifdef __APPLE__
        if (sem_post(shmp->sem_ptr) == -1) {
            perror("sem_post failed");
            return -1;
        }
        #else
        if (sem_post(&shmp->sem) == -1) {
            perror("sem_post failed");
            return -1;
        }
        #endif
        
        return 0;
    }
    
    // Mesajı kopyala
    size_t msg_len = strlen(shmp->msgbuf + shmp->last_read_pos) + 1; // Null terminatör dahil
    
    if (msg_len > buffer_size) {
        // Tampon çok küçük
        buffer[0] = '\0';
        
        // Semafor serbest bırak
        #ifdef __APPLE__
        if (sem_post(shmp->sem_ptr) == -1) {
            perror("sem_post failed");
            return -1;
        }
        #else
        if (sem_post(&shmp->sem) == -1) {
            perror("sem_post failed");
            return -1;
        }
        #endif
        
        return -1;
    }
    
    memcpy(buffer, shmp->msgbuf + shmp->last_read_pos, msg_len);
    
    // Son okunan pozisyonu güncelle
    shmp->last_read_pos += msg_len;
    
    // Semafor serbest bırak
    #ifdef __APPLE__
    if (sem_post(shmp->sem_ptr) == -1) {
        perror("sem_post failed");
        return -1;
    }
    #else
    if (sem_post(&shmp->sem) == -1) {
        perror("sem_post failed");
        return -1;
    }
    #endif
    
    return 1;
}

// Kaynakları temizle
void model_cleanup(ShmBuf* shmp) {
    // Eğer shmp NULL ise, global değişkeni kullan
    if (!shmp) {
        shmp = g_shm_buffer;
    }
    
    if (!shmp) {
        return;
    }
    
    // Tüm terminal süreçlerini sonlandır
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].active) {
            model_terminate_terminal_process(shmp, shmp->terminal_processes[i].terminal_id);
        }
    }
    
    // Paylaşılan belleği temizle
    #ifdef __APPLE__
    // macOS için
    if (shmp->sem_ptr) {
        sem_close(shmp->sem_ptr);
        sem_unlink("/mysem");
    }
    free(shmp);
    #else
    // Linux için
    if (shmp->fd >= 0) {
        sem_destroy(&shmp->sem);
        munmap(shmp, sizeof(ShmBuf) + shmp->buf_size);
        close(shmp->fd);
        shm_unlink(SHARED_FILE_PATH);
    }
    #endif
    
    // Global değişkeni sıfırla
    g_shm_buffer = NULL;
}
