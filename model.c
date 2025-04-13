#include "model.h"
#include <errno.h>
#include <limits.h>  // PATH_MAX için
#include <signal.h>  // Sinyal işleme için
#include <fcntl.h>   // O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, O_APPEND için

// Global değişken - her terminal için paylaşılan bellek işaretçisi
static ShmBuf* g_shm_buffer = NULL;

// Pipe tamponlarını temizle
void flush_pipe(int pipe_fd) {
    if (pipe_fd < 0) return;
    
    // Non-blocking moda geçir
    int flags = fcntl(pipe_fd, F_GETFL, 0);
    fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Pipe'ı boşalt
    char buffer[1024];
    ssize_t bytes;
    while ((bytes = read(pipe_fd, buffer, sizeof(buffer))) > 0) {
        // Veriyi at
    }
    
    // Hata kontrolü (EAGAIN/EWOULDBLOCK beklenen hata)
    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("flush_pipe read error");
    }
    
    // Orijinal moda geri dön
    fcntl(pipe_fd, F_SETFL, flags);
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
    if (strcmp(args[0], "wc") == 0) return 1;


    
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
    char output[8192]; // Çıktı tampon boyutu artırıldı
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
        
        // Çıkış komutu kontrolü
        if (strcmp(command, "exit") == 0) {
            snprintf(output, sizeof(output), "Terminal %d kapatılıyor...\n", terminal_id + 1);
            write(write_pipe, output, strlen(output) + 1);
            break;
        }
        
        // Sıfırlama komutu kontrolü
        if (strcmp(command, "reset") == 0 || strcmp(command, "clear") == 0) {
            // Terminali sıfırla
            snprintf(output, sizeof(output), "Terminal %d sıfırlandı.\n", terminal_id + 1);
            write(write_pipe, output, strlen(output) + 1);
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
            
            for (int i = 0; i < cmd_count - 1; i++) {
                if (pipe(pipes[i]) == -1) {
                    snprintf(output, sizeof(output), "Pipe oluşturulamadı: %s\n", strerror(errno));
                    write(write_pipe, output, strlen(output) + 1);
                    continue;
                }
            }
            
            // Son komutun çıktısını yakalamak için boru
            int final_pipe[2];
            if (pipe(final_pipe) == -1) {
                snprintf(output, sizeof(output), "Final pipe oluşturulamadı: %s\n", strerror(errno));
                write(write_pipe, output, strlen(output) + 1);
                continue;
            }
            
            // Her komut için çocuk süreç oluştur
            pid_t pids[64]; // En fazla 64 süreç
            
            for (int i = 0; i < cmd_count; i++) {
                pids[i] = fork();
                
                if (pids[i] == -1) {
                    snprintf(output, sizeof(output), "Fork hatası: %s\n", strerror(errno));
                    write(write_pipe, output, strlen(output) + 1);
                    continue;
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
                    execvp(args[0], args);
                    
                    // execvp başarısız olursa buraya ulaşır
                    fprintf(stderr, "Komut çalıştırılamadı: %s (%s)\n", args[0], strerror(errno));
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
                // Non-blocking I/O için dosya tanımlayıcısını ayarla
                int flags = fcntl(final_pipe[0], F_GETFL, 0);
                fcntl(final_pipe[0], F_SETFL, flags | O_NONBLOCK);
                
                // Timeout için değişkenler
                struct timeval tv;
                fd_set readfds;
                int ready;
                
                // Okuma için hazırlan
                FD_ZERO(&readfds);
                FD_SET(final_pipe[0], &readfds);
                
                // 2 saniye timeout ayarla
                tv.tv_sec = 2;
                tv.tv_usec = 0;
                
                // select() ile timeout'lu okuma
                ready = select(final_pipe[0] + 1, &readfds, NULL, NULL, &tv);
                
                if (ready == -1) {
                    snprintf(output, sizeof(output), "Select hatası: %s\n", strerror(errno));
                } else if (ready == 0) {
                    // Timeout oluştu - veri yok
                    snprintf(output, sizeof(output), "Komut çalışıyor, ancak henüz çıktı üretmedi.\n");
                } else {
                    // Veri okumaya hazır
                    ssize_t bytes_read = read(final_pipe[0], output, sizeof(output) - 1);
                    if (bytes_read == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // Veri henüz hazır değil
                            snprintf(output, sizeof(output), "Komut çalışıyor, ancak henüz çıktı üretmedi.\n");
                        } else {
                            snprintf(output, sizeof(output), "Okuma hatası: %s\n", strerror(errno));
                        }
                    } else {
                        // Veri okundu
                        output[bytes_read] = '\0';
                    }
                }
            } else {
                // Çıkış yönlendirme varsa, çıktı dosyaya yazılır
                snprintf(output, sizeof(output), "Çıktı '%s' dosyasına yönlendirildi.\n", output_file);
            }
            
            close(final_pipe[0]);
            
            // Tüm çocuk süreçlerin tamamlanmasını bekle (WNOHANG ile non-blocking)
            int last_status = 0;
            for (int i = 0; i < cmd_count; i++) {
                int status;
                // WNOHANG ile non-blocking wait
                if (waitpid(pids[i], &status, WNOHANG) == 0) {
                    // Süreç hala çalışıyor
                    if (i == cmd_count - 1) {
                        // Son komut hala çalışıyorsa, bilgi mesajı ekle
                        if (strlen(output) == 0) {
                            snprintf(output, sizeof(output), "Komut arka planda çalışıyor...\n");
                        }
                    }
                } else {
                    // Süreç tamamlandı
                    if (i == cmd_count - 1) {
                        if (WIFEXITED(status)) {
                            last_status = WEXITSTATUS(status);
                            if (last_status != 0 && strlen(output) == 0) {
                                snprintf(output, sizeof(output), "Komut çalıştırma hatası (kod: %d)\n", last_status);
                            }
                        }
                    }
                }
            }
            
            // Çıktıyı gönder (parçalı gönderim)
            size_t output_len = strlen(output);
            size_t chunk_size = 4000; // Daha küçük parçalar halinde gönder
            
            for (size_t i = 0; i < output_len; i += chunk_size) {
                size_t current_chunk = (i + chunk_size < output_len) ? chunk_size : output_len - i;
                write(write_pipe, output + i, current_chunk);
                usleep(10000); // 10ms bekle, pipe'ın dolmasını önle
            }
            
            // Null terminatör gönder
            char null_term = '\0';
            write(write_pipe, &null_term, 1);
            
            continue;
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
                }
                
                exit(EXIT_FAILURE);
            }
            
            // Ebeveyn süreç
            close(pipefd[1]);
            
            // Çıkış yönlendirme varsa, çıktı dosyaya yazılır
            if (has_output_redir) {
                snprintf(output, sizeof(output), "Çıktı '%s' dosyasına yönlendirildi.\n", output_file);
            } else {
                // Çıktıyı oku
                memset(output, 0, sizeof(output));
                ssize_t total_read = 0;
                ssize_t bytes;
                
                // Çocuk sürecin tamamlanmasını bekle
                int status;
                waitpid(pid, &status, 0);
                
                // Çıktıyı oku (parçalı okuma)
                while ((bytes = read(pipefd[0], output + total_read, sizeof(output) - total_read - 1)) > 0) {
                    total_read += bytes;
                    if (total_read >= (ssize_t)(sizeof(output) - 1)) {
                        break;
                    }
                }
                
                // Çıktı yoksa bilgi mesajı
                if (total_read == 0) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        snprintf(output, sizeof(output), "Komut çalıştırma hatası (kod: %d)\n", WEXITSTATUS(status));
                    } else {
                        strcpy(output, "Komut tamamlandı (çıktı yok)\n");
                    }
                }
            }
            
            close(pipefd[0]);
            
            // Çıktıyı gönder (parçalı gönderim)
            size_t output_len = strlen(output);
            size_t chunk_size = 4000; // Daha küçük parçalar halinde gönder
            
            for (size_t i = 0; i < output_len; i += chunk_size) {
                size_t current_chunk = (i + chunk_size < output_len) ? chunk_size : output_len - i;
                write(write_pipe, output + i, current_chunk);
                usleep(10000); // 10ms bekle, pipe'ın dolmasını önle
            }
            
            // Null terminatör gönder
            char null_term = '\0';
            write(write_pipe, &null_term, 1);
        }
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
        usleep(100000); // 100ms
        
        // Tekrar kontrol et
        result = waitpid(shmp->terminal_processes[idx].process_id, &status, WNOHANG);
        
        if (result == 0) {
            // Hala çalışıyor, SIGKILL gönder
            kill(shmp->terminal_processes[idx].process_id, SIGKILL);
            
            // Sonlanmasını bekle
            waitpid(shmp->terminal_processes[idx].process_id, &status, 0);
        }
    }
    
    // Pipe'ları kapat
    close(shmp->terminal_processes[idx].pipe_to_terminal[1]);
    close(shmp->terminal_processes[idx].pipe_from_terminal[0]);
    
    // Terminal bilgilerini temizle
    shmp->terminal_processes[idx].terminal_id = -1;
    shmp->terminal_processes[idx].process_id = -1;
    shmp->terminal_processes[idx].pipe_to_terminal[0] = -1;
    shmp->terminal_processes[idx].pipe_to_terminal[1] = -1;
    shmp->terminal_processes[idx].pipe_from_terminal[0] = -1;
    shmp->terminal_processes[idx].pipe_from_terminal[1] = -1;
    shmp->terminal_processes[idx].active = false;
    
    return 0;
}

// Terminal sürecini sıfırla
int model_reset_terminal_process(ShmBuf* shmp, int terminal_id) {
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
    
    // Pipe tamponlarını temizle
    flush_pipe(shmp->terminal_processes[idx].pipe_from_terminal[0]);
    
    // Reset komutu gönder
    return model_send_command_to_terminal(shmp, terminal_id, "reset");
}

// Terminal sürecinin durumunu kontrol et
int model_check_terminal_process(ShmBuf* shmp, int terminal_id) {
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
    
    // Sürecin durumunu kontrol et
    int status;
    pid_t result = waitpid(shmp->terminal_processes[idx].process_id, &status, WNOHANG);
    
    if (result == 0) {
        return 1; // Süreç çalışıyor
    } else if (result == shmp->terminal_processes[idx].process_id) {
        // Süreç sonlandı
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status); // Normal sonlanma
        } else if (WIFSIGNALED(status)) {
            return -2; // Sinyal ile sonlandırıldı
        } else {
            return -3; // Diğer sonlanma
        }
    } else {
        return -4; // waitpid hatası
    }
}

// Komut çalıştırma (terminal_id parametresi eklendi)
int model_execute_command(const char* command, char* output, size_t output_size, int terminal_id) {
    if (!command || !output || output_size == 0 || terminal_id < 0) {
        return -1;
    }
    
    // Boş komut kontrolü
    if (strlen(command) == 0) {
        output[0] = '\0';
        return 0;
    }
    
    // Sıfırlama komutu kontrolü
    if (strcmp(command, "reset") == 0 || strcmp(command, "clear") == 0) {
        // Terminali sıfırla
        if (model_reset_terminal_process(NULL, terminal_id) == 0) {
            snprintf(output, output_size, "Terminal %d sıfırlandı.\n", terminal_id + 1);
            return 0;
        }
    }
    
    // İnteraktif komut kontrolü
    if (is_interactive_command(command)) {
        // İnteraktif komut için özel mesaj
        snprintf(output, output_size, 
                "İnteraktif komut tespit edildi: %s\n"
                "Bu komut kullanıcı girdisi bekliyor ve terminal arayüzünde doğrudan çalıştırılamaz.\n"
                "Lütfen dosya yönlendirme kullanın (örn: cat < dosya.txt) veya argüman belirtin (örn: cat dosya.txt).\n", 
                command);
        return 0;
    }
    
    // Terminal sürecine komutu gönder
    if (model_send_command_to_terminal(NULL, terminal_id, command) != 0) {
        snprintf(output, output_size, "Komut gönderilemedi. Terminal %d aktif değil veya hata oluştu.\n", terminal_id + 1);
        return -1;
    }
    
    // Çıktıyı bekle (daha uzun bir süre)
    usleep(500000); // 500ms (artırıldı)
    
    // Çıktıyı oku
    int bytes_read = model_read_output_from_terminal(NULL, terminal_id, output, output_size);
    
    if (bytes_read < 0) {
        snprintf(output, output_size, "Çıktı okunamadı. Terminal %d aktif değil veya hata oluştu.\n", terminal_id + 1);
        return -1;
    } else if (bytes_read == 0) {
        // Çıktı yoksa, biraz daha bekle ve tekrar dene
        usleep(500000); // 500ms daha bekle
        bytes_read = model_read_output_from_terminal(NULL, terminal_id, output, output_size);
        
        if (bytes_read <= 0) {
            // Hala çıktı yoksa, muhtemelen komut bulunamadı
            snprintf(output, output_size, "sh: %s: command not found\n", command);
        }
    }
    
    return 0;
}

// Mesaj gönderme
ShmBuf* model_send_message(ShmBuf* shmp, const char* message) {
    if (!shmp || !message) {
        return NULL;
    }
    
    size_t len = strlen(message);
    if (len == 0 || len >= shmp->buf_size) {
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
    
    // Mesajı kopyala
    memcpy(shmp->msgbuf, message, len + 1); // null karakteri dahil
    shmp->cnt = len;
    
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

// Mesaj okuma
int model_read_messages(ShmBuf* shmp, char* buffer, size_t buffer_size) {
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
    if (shmp->cnt > 0 && shmp->last_read_pos != shmp->cnt) {
        // Mesajı kopyala
        size_t copy_size = shmp->cnt < buffer_size - 1 ? shmp->cnt : buffer_size - 1;
        memcpy(buffer, shmp->msgbuf, copy_size);
        buffer[copy_size] = '\0';
        
        // Son okunan pozisyonu güncelle
        shmp->last_read_pos = shmp->cnt;
    } else {
        // Yeni mesaj yok
        buffer[0] = '\0';
    }
    
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
    
    return strlen(buffer);
}

// Temizleme
void model_cleanup(ShmBuf* shmp) {
    if (!shmp) return;
    
    // Tüm terminal süreçlerini sonlandır
    for (int i = 0; i < shmp->terminal_count; i++) {
        if (shmp->terminal_processes[i].active) {
            model_terminate_terminal_process(shmp, shmp->terminal_processes[i].terminal_id);
        }
    }
    
    #ifdef __APPLE__
    // macOS için temizleme
    if (shmp->sem_ptr) {
        sem_close(shmp->sem_ptr);
        sem_unlink("/mysem");
    }
    free(shmp);
    #else
    // Linux için temizleme
    sem_destroy(&shmp->sem);
    
    if (shmp->fd >= 0) {
        close(shmp->fd);
        shm_unlink(SHARED_FILE_PATH);
    }
    
    munmap(shmp, sizeof(ShmBuf) + shmp->buf_size);
    #endif
    
    // Global değişkeni temizle
    g_shm_buffer = NULL;
}
