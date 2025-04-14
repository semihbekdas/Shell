# Multi-User Communicating Shells - Project Report / Proje Raporu

## ENG

### Introduction

This report discusses the design choices, implementation details, and challenges encountered in the "Multi-User Communicating Shells" project. The application provides a terminal-like environment where multiple shell instances can run in parallel and communicate through a shared message buffer. The project is designed using the MVC (Model-View-Controller) architecture with a graphical user interface provided by the GTK4 library.

### Design Choices

#### MVC Architecture

The project was designed using the Model-View-Controller (MVC) architectural pattern, which provides the following advantages:

1. **Modularity**: The application logic (Model), user interface (View), and control flow (Controller) are separated. This allows each component to be developed and tested independently.

2. **Maintainability**: Dividing the code into sections with different responsibilities simplifies maintenance and debugging processes.

3. **Extensibility**: New features can be added without disrupting the existing structure. For example, adding a new command type or messaging feature only requires changes to the relevant components.

4. **Reusability**: Each component can be reused independently of other components. For instance, the Model component could be used with a different interface.

#### Component Details

##### Model Component

The Model component manages the application's data and business logic. Its primary responsibilities include:

1. **Shared Memory Management**: Inter-terminal communication is achieved using POSIX shared memory (`shm_open`, `mmap`) and semaphores (`sem_t`). This approach provides efficient data sharing between different processes.

2. **Command Execution**: Shell commands are executed using `fork()`, `execvp()`, and `pipe()` system calls. This enables support for standard Unix/Linux commands.

3. **Input/Output Redirection**: Standard input/output streams are redirected using the `dup2()` system call. This enables support for redirection operators (`>`, `>>`, `<`) and the pipeline operator (`|`).

4. **Platform Compatibility**: Conditional compilation (`#ifdef __APPLE__`) is used to handle differences between macOS and Linux. This allows the application to run on different operating systems.

5. **Message Buffer Management**: A fixed-size message buffer (4096 bytes) is used on shared memory. When the buffer fills up, it wraps around and overwrites old messages to make room for new ones. This approach keeps memory usage limited and predictable.

##### View Component

The View component manages the user interface. It is built using the GTK4 library. Key features include:

1. **Terminal Emulation**: Each terminal tab includes command input and output display areas. `GtkTextView` and `GtkEntry` widgets are used to provide a terminal-like experience.

2. **Tab Management**: A `GtkNotebook` widget is used to manage multiple terminal tabs. Users can add new tabs and close existing ones.

3. **Message Panel**: A separate panel displays shared messages, visualizing inter-terminal communication.

4. **Color Coding**: Terminal outputs and messages are displayed with color coding to indicate their source. This improves user experience and makes information easier to distinguish.

5. **Command History**: Each terminal maintains its own command history, allowing users to navigate through previously executed commands using arrow keys.

##### Controller Component

The Controller component coordinates communication between the Model and View. Its primary responsibilities include:

1. **User Input Processing**: It analyzes user inputs to distinguish between standard shell commands and message commands (`@msg`).

2. **Command Routing**: It routes commands to appropriate Model functions and relays results to the View.

3. **Periodic Updates**: It periodically checks the shared memory buffer for new messages and updates the View. This is implemented using GTK's `g_timeout_add()` function.

4. **Message History**: It maintains a buffer of recent messages to prevent displaying duplicate messages.

#### Technical Choices

1. **GTK4 Usage**: GTK4, a modern and platform-independent GUI library, was chosen. This ensures consistent appearance and behavior across different Linux distributions.

2. **POSIX Shared Memory**: POSIX shared memory mechanism was selected for inter-terminal communication. This provides efficient data sharing between processes, with synchronization problems solved using semaphores.

3. **Flexible Array Usage**: A flexible array (`char msgbuf[]`) is used in the shared memory structure to optimize memory usage.

4. **Semaphore Synchronization**: Access to shared memory is synchronized using semaphores (`sem_t`). This preserves data integrity and prevents race conditions.

5. **Pipeline and Redirection Support**: Pipeline and redirection operators are implemented to support standard Unix/Linux shell features.

6. **Circular Buffer Approach**: A circular buffer approach is adopted for the message buffer. When the buffer fills up, it wraps around and overwrites old messages to make room for new ones.

### Performance Optimizations

Several optimizations were implemented to enhance the application's performance and user experience:

#### 1. Buffer Size Optimization

**Issue**: Small buffer sizes caused performance problems when processing large command outputs.

**Solution**: 
- Output buffer size was increased from 8KB to 16KB
- Data transmission chunk size was increased from 4KB to 16KB
- These changes significantly improved performance when running commands that produce large outputs (like `cat`, `find`)

```c
// Previous implementation
char output[8192]; // 8KB buffer
// ...
size_t chunk_size = 4000; // 4KB chunk size

// Improved implementation
char output[16384]; // 16KB buffer
// ...
size_t chunk_size = 16384; // 16KB chunk size
```

#### 2. Elimination of Unnecessary Wait Times

**Issue**: Unnecessary wait times when sending command output and managing terminal processes reduced command execution performance.

**Solution**:
- The 10ms wait (usleep(10000)) between each chunk when sending command output was removed
- Terminal process termination wait time was reduced from 100ms to 50ms
- Wait times during terminal startup and command execution in the Controller component were reduced from 100ms to 30ms

```c
// Previous implementation
usleep(10000); // 10ms wait between each chunk
// ...
usleep(100000); // 100ms wait for terminal termination

// Improved implementation
// Wait removed - to improve performance
// ...
usleep(50000); // 50ms wait for terminal termination
```

#### 3. Improved Pipe Cleaning Mechanism

**Issue**: After viewing large files (e.g., `cat model.c`) and then using the `clear` command followed by other commands, problems occurred due to pipes not being properly cleaned.

**Solution**:
- The `flush_pipe()` function was enhanced with a larger buffer (8KB) and more attempts (100 tries) for more aggressive cleaning
- Special handling was added for the `clear` command, completely cleaning pipes when this command is executed

```c
// Improved pipe cleaning
void flush_pipe(int pipe_fd) {
    // ...
    char buffer[8192]; // Larger buffer (8KB)
    int flush_attempts = 0;
    
    // More data cleaning and more attempts
    while (flush_attempts < 100) { // 100 attempts
        // ...
    }
    // ...
}
```

#### 4. Command Validation and Error Handling

**Issue**: Invalid commands or empty inputs could cause unstable system behavior.

**Solution**:
- Added the `is_valid_command()` function to detect and reject empty commands or those consisting only of whitespace characters
- Added better error messages and user feedback for invalid commands
- Added better recovery mechanisms for error conditions

```c
// Check command validity
int is_valid_command(const char* command) {
    if (!command || strlen(command) == 0) return 0;
    
    // Check for commands consisting only of whitespace characters
    int only_whitespace = 1;
    for (const char* p = command; *p; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            only_whitespace = 0;
            break;
        }
    }
    
    return !only_whitespace;
}
```

### Implementation Challenges and Solutions

#### 1. Inter-Process Communication

**Challenge**: Providing efficient and reliable communication between different terminal processes.

**Solution**: Inter-process communication is achieved using POSIX shared memory and semaphores. Semaphores control concurrent access to shared memory, preserving data integrity.

```c
sem_wait(&shmp->sem); // Enter critical section
// Shared memory operations
sem_post(&shmp->sem); // Exit critical section
```

#### 2. Platform Compatibility

**Challenge**: Differences in shared memory and semaphore implementations between macOS and Linux.

**Solution**: Conditional compilation (`#ifdef __APPLE__`) is used to provide appropriate code paths for different operating systems. The named semaphore approach is used for macOS, while the anonymous semaphore approach is used for Linux.

```c
#ifdef __APPLE__
// Code for macOS
sem_unlink("/mysem");
sem_t *sem = sem_open("/mysem", O_CREAT, 0600, 1);
#else
// Code for Linux
sem_init(&shmp->sem, 1, 1);
#endif
```

#### 3. Command Execution and Output Capture

**Challenge**: Executing shell commands and redirecting their output to the GUI.

**Solution**: Commands are executed using `fork()`, `execvp()`, `pipe()`, and `dup2()` system calls. Non-blocking I/O and `select()` are used to capture output from long-running commands in a timely manner.

```c
// Create pipe
int pipefd[2];
pipe(pipefd);

if (fork() == 0) {
    // Child process
    close(pipefd[0]); // Close reading end
    dup2(pipefd[1], STDOUT_FILENO); // Redirect standard output to pipe
    execvp(args[0], args); // Execute command
    exit(EXIT_FAILURE);
} else {
    // Parent process
    close(pipefd[1]); // Close writing end
    // Set up non-blocking I/O
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    // Read output
    // ...
}
```

#### 4. Pipeline and Redirection Operators

**Challenge**: Supporting pipeline (`|`) and file redirection (`>`, `>>`, `<`) operators that connect multiple commands.

**Solution**: A parser is implemented to detect operators in the command string and make appropriate system calls. A separate child process is created for each command, and pipes are established between these processes.

```c
// Split commands
char* commands[64] = {0};
int cmd_count = 0;
char* token = strtok(cmd_copy, "|");
while (token && cmd_count < 64) {
    commands[cmd_count++] = token;
    token = strtok(NULL, "|");
}

// Create pipe for each command
int pipes[64][2];
for (int i = 0; i < cmd_count - 1; i++) {
    pipe(pipes[i]);
}

// Create child process for each command
for (int i = 0; i < cmd_count; i++) {
    // ...
    if (i > 0) {
        // Connect output of previous command to input of this command
        dup2(pipes[i-1][0], STDIN_FILENO);
    }
    if (i < cmd_count - 1) {
        // Connect output of this command to input of next command
        dup2(pipes[i][1], STDOUT_FILENO);
    }
    // ...
}
```

#### 5. Interactive Commands

**Challenge**: Running interactive commands like `cat`, `nano`, `vim` in a GUI environment.

**Solution**: A function is implemented to detect interactive commands, and an information message is displayed to the user for these commands. Users are directed to use file redirection or specify arguments when using these commands.

```c
int is_interactive_command(const char* command) {
    // ...
    if (strcmp(args[0], "cat") == 0 && arg_count == 1) {
        return 1; // cat command without arguments is interactive
    }
    if (strcmp(args[0], "nano") == 0) return 1;
    if (strcmp(args[0], "vim") == 0) return 1;
    // ...
    return 0;
}
```

#### 6. GTK4 Integration

**Challenge**: Creating a terminal-like interface using GTK4's new API and integrating GTK events with system calls.

**Solution**: Terminal emulation is provided using GTK4's modern widgets and signal system. Periodic timers (`g_timeout_add()`) are used for shared memory checking and GUI updates.

```c
// Message update timer
controller->message_update_timer_id = g_timeout_add(1000, controller_message_update_timer, controller);

// Timer callback function
gboolean controller_message_update_timer(gpointer user_data) {
    Controller *controller = (Controller *)user_data;
    controller_update_messages(controller);
    return G_SOURCE_CONTINUE; // Continue timer
}
```

#### 7. Message Buffer Management

**Challenge**: Preventing memory overflow issues when a large number of messages are sent.

**Solution**: A fixed-size message buffer is used, and when the buffer fills up, a circular buffer approach is adopted to overwrite old messages. This keeps memory usage under control while ensuring new messages can always be delivered.

```c
// If buffer is full, wrap around
if (shmp->cnt + msg_len + 1 > shmp->buf_size) {
    // Buffer full, wrap around
    shmp->cnt = 0;
    shmp->last_read_pos = 0;
}

// Copy message to shared memory
memcpy(&shmp->msgbuf[shmp->cnt], message, msg_len);
shmp->cnt += msg_len;
shmp->msgbuf[shmp->cnt] = '\0';  // Add null terminator
shmp->cnt++;
```

#### 8. Large File Outputs and Terminal Stability

**Challenge**: Maintaining terminal stability after viewing large files (e.g., `cat model.c`).

**Solution**: An enhanced pipe cleaning mechanism and chunked reading/writing strategy were implemented. Additionally, special handling was added for the `clear` command to completely reset the terminal state.

```c
// Directly relay output - without using buffer
char temp_buffer[8192]; // Temporary buffer
ssize_t bytes;
int total_sent = 0;
int has_output = 0;

while ((bytes = read(pipefd[0], temp_buffer, sizeof(temp_buffer) - 1)) > 0) {
    has_output = 1;
    temp_buffer[bytes] = '\0';
    write(write_pipe, temp_buffer, bytes);
    total_sent += bytes;
    
    // Safety check for very large outputs
    if (total_sent > 10 * 1024 * 1024) { // More than 10MB of data
        char overflow_msg[] = "\n... (output too large, remaining part truncated) ...\n";
        write(write_pipe, overflow_msg, strlen(overflow_msg));
        break;
    }
}
```

### Performance Evaluation

The application has been evaluated based on the following performance criteria:

1. **Response Time**: Response time to user commands is in the order of milliseconds in most cases. However, response time may increase for commands that require intensive processing (e.g., `find /`).

2. **Memory Usage**: The application has a reasonable memory footprint. Additional memory is required for each terminal tab, but this increase is linear and predictable. The fixed-size memory usage for the message buffer (4096 bytes) keeps memory consumption limited and predictable.

3. **CPU Usage**: CPU usage is minimal when idle. Periodic message checks and GUI updates do not create significant CPU load.

4. **Scalability**: The application can smoothly support up to 10 terminal tabs. For more tabs, memory limits and GUI performance should be considered.

5. **Message Capacity**: The fixed-size message buffer (4096 bytes) limits the number of messages that can be stored at any given time. However, thanks to the circular buffer approach, when the buffer fills up, space is made for new messages, and the system continues to operate continuously.

### Conclusion

The "Multi-User Communicating Shells" project provides a comprehensive application in system programming, GUI development, and concurrency management. A modular and maintainable codebase has been created using the MVC architecture. Reliable inter-terminal communication has been achieved using POSIX shared memory and semaphores. A modern and user-friendly interface has been created using the GTK4 library.

The circular buffer approach adopted for message buffer management keeps memory usage limited and predictable while ensuring the system operates continuously. This approach guarantees that new messages can always be delivered, at the cost of losing old messages.

The project successfully combines basic shell functionality and inter-terminal messaging, showcasing system programming and GUI development skills. Challenges have been overcome with appropriate technical solutions, and a solid foundation has been established for future improvements.

## TR

### Giriş

Bu rapor, "Multi-User Communicating Shells" projesinin tasarım seçimleri, uygulama detayları ve karşılaşılan zorlukları ele almaktadır. Proje, birden fazla kabuk (shell) örneğinin paralel olarak çalışabildiği ve paylaşılan bir mesaj tamponu üzerinden iletişim kurabildiği bir terminal benzeri uygulama sunmaktadır. MVC (Model-View-Controller) mimarisi kullanılarak tasarlanmış olup, GTK4 kütüphanesi ile grafiksel kullanıcı arayüzü sağlanmaktadır.

### Tasarım Seçimleri

#### MVC Mimarisi

Proje, Model-View-Controller (MVC) mimari deseni kullanılarak tasarlanmıştır. Bu seçim, aşağıdaki avantajları sağlamıştır:

1. **Modülerlik**: Uygulama mantığı (Model), kullanıcı arayüzü (View) ve kontrol akışı (Controller) birbirinden ayrılmıştır. Bu sayede her bir bileşen bağımsız olarak geliştirilebilir ve test edilebilir.

2. **Bakım Kolaylığı**: Kodun farklı sorumlulukları olan bölümlere ayrılması, bakım ve hata ayıklama süreçlerini kolaylaştırmıştır.

3. **Genişletilebilirlik**: Yeni özelliklerin eklenmesi, mevcut yapıyı bozmadan gerçekleştirilebilir. Örneğin, yeni bir komut türü veya mesajlaşma özelliği eklemek için sadece ilgili bileşenlerde değişiklik yapmak yeterlidir.

4. **Yeniden Kullanılabilirlik**: Her bir bileşen, diğer bileşenlerden bağımsız olarak yeniden kullanılabilir. Örneğin, Model bileşeni farklı bir arayüz ile kullanılabilir.

#### Bileşen Detayları

##### Model Bileşeni

Model bileşeni, uygulamanın veri ve iş mantığını yönetir. Temel sorumlulukları şunlardır:

1. **Paylaşılan Bellek Yönetimi**: POSIX paylaşılan bellek (`shm_open`, `mmap`) ve semaforlar (`sem_t`) kullanılarak terminaller arası iletişim sağlanmıştır. Bu yaklaşım, farklı süreçler arasında verimli veri paylaşımı sağlar.

2. **Komut Çalıştırma**: `fork()`, `execvp()` ve `pipe()` sistem çağrıları kullanılarak kabuk komutlarının çalıştırılması gerçekleştirilmiştir. Bu, standart Unix/Linux komutlarının desteklenmesini sağlar.

3. **Giriş/Çıkış Yönlendirme**: `dup2()` sistem çağrısı kullanılarak standart giriş/çıkış akışlarının yönlendirilmesi sağlanmıştır. Bu, `>`, `>>`, `<` gibi yönlendirme operatörlerinin ve `|` boru hattı operatörünün desteklenmesini sağlar.

4. **Platform Uyumluluğu**: macOS ve Linux arasındaki farklılıkları ele almak için koşullu derleme (`#ifdef __APPLE__`) kullanılmıştır. Bu, uygulamanın farklı işletim sistemlerinde çalışabilmesini sağlar.

5. **Mesaj Tamponu Yönetimi**: Paylaşılan bellek üzerinde sabit boyutlu bir mesaj tamponu (4096 bayt) kullanılmıştır. Tampon dolduğunda, yeni mesajlar için yer açmak amacıyla tampon başa döndürülür ve eski mesajların üzerine yazılır. Bu yaklaşım, bellek kullanımını sınırlı ve öngörülebilir tutar.

##### View Bileşeni

View bileşeni, kullanıcı arayüzünü yönetir. GTK4 kütüphanesi kullanılarak oluşturulmuştur. Temel özellikleri şunlardır:

1. **Terminal Emülasyonu**: Her bir terminal sekmesi, komut girişi ve çıktı görüntüleme alanlarını içerir. `GtkTextView` ve `GtkEntry` widget'ları kullanılarak terminal benzeri bir deneyim sağlanmıştır.

2. **Sekme Yönetimi**: `GtkNotebook` widget'ı kullanılarak birden fazla terminal sekmesinin yönetilmesi sağlanmıştır. Kullanıcılar yeni sekmeler ekleyebilir ve mevcut sekmeleri kapatabilir.

3. **Mesaj Paneli**: Paylaşılan mesajların görüntülendiği ayrı bir panel bulunmaktadır. Bu panel, terminaller arası iletişimi görselleştirir.

4. **Renk Kodlaması**: Terminal çıktıları ve mesajlar, kaynağını belirtmek için renk kodlaması ile görüntülenir. Bu, kullanıcı deneyimini iyileştirir ve bilgilerin daha kolay ayırt edilmesini sağlar.

5. **Komut Geçmişi**: Her terminal kendi komut geçmişini tutar, kullanıcılar ok tuşlarını kullanarak daha önce çalıştırılan komutlar arasında gezinebilir.

##### Controller Bileşeni

Controller bileşeni, Model ve View arasındaki iletişimi koordine eder. Temel sorumlulukları şunlardır:

1. **Kullanıcı Girişi İşleme**: Kullanıcı girişlerini analiz ederek standart kabuk komutları ve mesaj komutları (`@msg`) arasında ayrım yapar.

2. **Komut Yönlendirme**: Komutları uygun Model fonksiyonlarına yönlendirir ve sonuçları View'a iletir.

3. **Periyodik Güncelleme**: Paylaşılan bellek tamponunu periyodik olarak kontrol ederek yeni mesajları tespit eder ve View'ı günceller. Bu, GTK'nın `g_timeout_add()` fonksiyonu kullanılarak gerçekleştirilmiştir.

4. **Mesaj Geçmişi**: Son mesajları bir tampon içinde saklayarak tekrarlanan mesajların görüntülenmesini önler.

#### Teknik Seçimler

1. **GTK4 Kullanımı**: Modern ve platform bağımsız bir GUI kütüphanesi olan GTK4 tercih edilmiştir. Bu, uygulamanın farklı Linux dağıtımlarında tutarlı bir görünüm ve davranış sergilemesini sağlar.

2. **POSIX Paylaşılan Bellek**: Terminaller arası iletişim için POSIX paylaşılan bellek mekanizması seçilmiştir. Bu, süreçler arası verimli veri paylaşımı sağlar ve semaforlar ile senkronizasyon problemleri çözülmüştür.

3. **Esnek Dizi Kullanımı**: Paylaşılan bellek yapısında esnek dizi (`char msgbuf[]`) kullanılarak bellek kullanımı optimize edilmiştir.

4. **Semafor Senkronizasyonu**: Paylaşılan belleğe erişim, semaforlar (`sem_t`) kullanılarak senkronize edilmiştir. Bu, veri bütünlüğünü korur ve yarış koşullarını önler.

5. **Boru Hattı ve Yönlendirme Desteği**: Standart Unix/Linux kabuk özelliklerini desteklemek için boru hattı ve yönlendirme operatörleri uygulanmıştır.

6. **Dairesel Tampon Yaklaşımı**: Mesaj tamponu için dairesel tampon yaklaşımı benimsenmiştir. Tampon dolduğunda, yeni mesajlar için yer açmak amacıyla tampon başa döndürülür ve eski mesajların üzerine yazılır.

### Performans İyileştirmeleri

Proje geliştirme sürecinde, kullanıcı deneyimini ve sistem performansını artırmak için çeşitli iyileştirmeler yapılmıştır:

#### 1. Tampon Boyutlarının Optimizasyonu

**Sorun**: Küçük tampon boyutları, büyük komut çıktılarının işlenmesinde performans sorunlarına neden oluyordu.

**Çözüm**: 
- Çıktı tampon boyutu 8KB'dan 16KB'a çıkarıldı
- Veri gönderme parça boyutu 4KB'dan 16KB'a artırıldı
- Bu değişiklikler, özellikle büyük çıktı üreten komutların (`cat`, `find` gibi) çalıştırılmasında önemli performans artışı sağladı

```c
// Önceki implementasyon
char output[8192]; // 8KB tampon
// ...
size_t chunk_size = 4000; // 4KB parça boyutu

// İyileştirilmiş implementasyon
char output[16384]; // 16KB tampon
// ...
size_t chunk_size = 16384; // 16KB parça boyutu
```

#### 2. Gereksiz Bekleme Sürelerinin Kaldırılması

**Sorun**: Komut çıktısını gönderirken ve terminal süreçlerini yönetirken gereksiz bekleme süreleri, komut çalıştırma performansını düşürüyordu.

**Çözüm**:
- Komut çıktısını gönderirken her parça arasındaki 10ms bekleme (usleep(10000)) kaldırıldı
- Terminal sürecini sonlandırırken bekleme süresi 100ms'den 50ms'ye düşürüldü
- Controller bileşeninde terminal başlatma ve komut çalıştırma sırasındaki 100ms beklemeler 30ms'ye düşürüldü

```c
// Önceki implementasyon
usleep(10000); // Her parça arasında 10ms bekleme
// ...
usleep(100000); // Terminal sonlandırma için 100ms bekleme

// İyileştirilmiş implementasyon
// Bekleme kaldırıldı - performansı artırmak için
// ...
usleep(50000); // Terminal sonlandırma için 50ms bekleme
```

#### 3. Pipe Temizleme Mekanizmasının İyileştirilmesi

**Sorun**: Büyük dosyaları görüntüledikten sonra (örneğin `cat model.c`) ve ardından `clear` komutunu kullanıp başka komutlar çalıştırdığında, pipe'ların düzgün temizlenmemesi nedeniyle sorunlar yaşanıyordu.

**Çözüm**:
- `flush_pipe()` fonksiyonu geliştirildi, daha büyük tampon (8KB) ve daha fazla deneme (100 deneme) ile daha agresif temizleme sağlandı
- `clear` komutu için özel işlem eklendi, bu komut çalıştırıldığında pipe'lar tamamen temizleniyor

```c
// İyileştirilmiş pipe temizleme
void flush_pipe(int pipe_fd) {
    // ...
    char buffer[8192]; // Daha büyük tampon (8KB)
    int flush_attempts = 0;
    
    // Daha fazla veri temizleme ve daha fazla deneme
    while (flush_attempts < 100) { // 100 deneme
        // ...
    }
    // ...
}
```

#### 4. Komut Doğrulama ve Hata Yönetimi

**Sorun**: Geçersiz komutlar veya boş girişler, sistemin kararsız davranmasına neden olabiliyordu.

**Çözüm**:
- Boş veya sadece boşluk karakterlerinden oluşan komutları tespit eden ve reddeden `is_valid_command()` fonksiyonu eklendi
- Geçersiz komutlar için daha iyi hata mesajları ve kullanıcı geri bildirimi eklendi
- Hata durumlarında daha iyi kurtarma mekanizmaları eklendi

```c
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
```

### Karşılaşılan Zorluklar ve Çözümler

#### 1. Süreçler Arası İletişim

**Zorluk**: Farklı terminal süreçleri arasında verimli ve güvenilir iletişim sağlamak.

**Çözüm**: POSIX paylaşılan bellek ve semaforlar kullanılarak süreçler arası iletişim sağlanmıştır. Semaforlar, paylaşılan belleğe eşzamanlı erişimi kontrol ederek veri bütünlüğünü korur.

```c
sem_wait(&shmp->sem); // Kritik bölgeye giriş
// Paylaşılan bellek işlemleri
sem_post(&shmp->sem); // Kritik bölgeden çıkış
```

#### 2. Platform Uyumluluğu

**Zorluk**: macOS ve Linux arasındaki paylaşılan bellek ve semafor uygulamalarındaki farklılıklar.

**Çözüm**: Koşullu derleme (`#ifdef __APPLE__`) kullanılarak farklı işletim sistemleri için uygun kod yolları sağlanmıştır. macOS için named semaphore yaklaşımı, Linux için anonim semafor yaklaşımı kullanılmıştır.

```c
#ifdef __APPLE__
// macOS için kod
sem_unlink("/mysem");
sem_t *sem = sem_open("/mysem", O_CREAT, 0600, 1);
#else
// Linux için kod
sem_init(&shmp->sem, 1, 1);
#endif
```

#### 3. Komut Çalıştırma ve Çıktı Yakalama

**Zorluk**: Kabuk komutlarını çalıştırmak ve çıktılarını GUI'ye yönlendirmek.

**Çözüm**: `fork()`, `execvp()`, `pipe()` ve `dup2()` sistem çağrıları kullanılarak komutlar çalıştırılmış ve çıktıları yakalanmıştır. Non-blocking I/O ve `select()` kullanılarak uzun süren komutların çıktıları zamanında alınmıştır.

```c
// Boru oluştur
int pipefd[2];
pipe(pipefd);

if (fork() == 0) {
    // Çocuk süreç
    close(pipefd[0]); // Okuma ucunu kapat
    dup2(pipefd[1], STDOUT_FILENO); // Standart çıkışı boruya yönlendir
    execvp(args[0], args); // Komutu çalıştır
    exit(EXIT_FAILURE);
} else {
    // Ebeveyn süreç
    close(pipefd[1]); // Yazma ucunu kapat
    // Non-blocking I/O için ayarla
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    // Çıktıyı oku
    // ...
}
```

#### 4. Boru Hattı ve Yönlendirme Operatörleri

**Zorluk**: Birden fazla komutu birbirine bağlayan boru hattı (`|`) ve dosya yönlendirme (`>`, `>>`, `<`) operatörlerini desteklemek.

**Çözüm**: Komut dizesini ayrıştırarak operatörleri tespit eden ve uygun sistem çağrılarını yapan bir parser uygulanmıştır. Her bir komut için ayrı bir çocuk süreç oluşturulmuş ve bu süreçler arasında borular kurulmuştur.

```c
// Komutları ayır
char* commands[64] = {0};
int cmd_count = 0;
char* token = strtok(cmd_copy, "|");
while (token && cmd_count < 64) {
    commands[cmd_count++] = token;
    token = strtok(NULL, "|");
}

// Her komut için boru oluştur
int pipes[64][2];
for (int i = 0; i < cmd_count - 1; i++) {
    pipe(pipes[i]);
}

// Her komut için çocuk süreç oluştur
for (int i = 0; i < cmd_count; i++) {
    // ...
    if (i > 0) {
        // Önceki komutun çıkışını bu komutun girişine bağla
        dup2(pipes[i-1][0], STDIN_FILENO);
    }
    if (i < cmd_count - 1) {
        // Bu komutun çıkışını sonraki komutun girişine bağla
        dup2(pipes[i][1], STDOUT_FILENO);
    }
    // ...
}
```

#### 5. İnteraktif Komutlar

**Zorluk**: `cat`, `nano`, `vim` gibi interaktif komutların GUI ortamında çalıştırılması.

**Çözüm**: İnteraktif komutları tespit eden bir fonksiyon uygulanmış ve bu komutlar için kullanıcıya bilgi mesajı gösterilmiştir. Kullanıcılar, bu komutları dosya yönlendirme veya argüman belirterek kullanmaya yönlendirilmiştir.

```c
int is_interactive_command(const char* command) {
    // ...
    if (strcmp(args[0], "cat") == 0 && arg_count == 1) {
        return 1; // Argümansız cat komutu interaktiftir
    }
    if (strcmp(args[0], "nano") == 0) return 1;
    if (strcmp(args[0], "vim") == 0) return 1;
    // ...
    return 0;
}
```

#### 6. GTK4 Entegrasyonu

**Zorluk**: GTK4'ün yeni API'sini kullanarak terminal benzeri bir arayüz oluşturmak ve GTK olayları ile sistem çağrılarını entegre etmek.

**Çözüm**: GTK4'ün modern widget'ları ve sinyal sistemi kullanılarak terminal emülasyonu sağlanmıştır. Periyodik zamanlayıcılar (`g_timeout_add()`) kullanılarak paylaşılan bellek kontrolü ve GUI güncellemeleri gerçekleştirilmiştir.

```c
// Mesaj güncelleme zamanlayıcısı
controller->message_update_timer_id = g_timeout_add(1000, controller_message_update_timer, controller);

// Zamanlayıcı callback fonksiyonu
gboolean controller_message_update_timer(gpointer user_data) {
    Controller *controller = (Controller *)user_data;
    controller_update_messages(controller);
    return G_SOURCE_CONTINUE; // Zamanlayıcıyı devam ettir
}
```

#### 7. Mesaj Tamponu Yönetimi

**Zorluk**: Çok sayıda mesaj gönderildiğinde bellek taşması sorunlarını önlemek.

**Çözüm**: Sabit boyutlu bir mesaj tamponu kullanılmış ve tampon dolduğunda dairesel tampon mantığıyla eski mesajların üzerine yazma yaklaşımı benimsenmiştir. Bu, bellek kullanımını kontrol altında tutarken, yeni mesajların her zaman iletilebilmesini sağlar.

```c
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
```

#### 8. Büyük Dosya Çıktıları ve Terminal Kararlılığı

**Zorluk**: Büyük dosyaları görüntüledikten sonra (örneğin `cat model.c`) terminal kararlılığının korunması.

**Çözüm**: Geliştirilmiş pipe temizleme mekanizması ve parçalı okuma/yazma stratejisi uygulanmıştır. Ayrıca, `clear` komutu için özel işlem eklenerek terminal durumunun tamamen sıfırlanması sağlanmıştır.

```c
// Çıktıyı doğrudan ilet - tampon kullanmadan
char temp_buffer[8192]; // Geçici tampon
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
```

### Performans Değerlendirmesi

Uygulama, aşağıdaki performans kriterleri açısından değerlendirilmiştir:

1. **Tepki Süresi**: Kullanıcı komutlarına tepki süresi, çoğu durumda milisaniyeler mertebesindedir. Ancak, yoğun işlem gerektiren komutlar (örn. `find /`) için tepki süresi artabilir.

2. **Bellek Kullanımı**: Uygulama, makul bir bellek ayak izine sahiptir. Her terminal sekmesi için ek bellek kullanımı gerekir, ancak bu artış doğrusal ve öngörülebilirdir. Mesaj tamponu için sabit boyutlu bellek kullanımı (4096 bayt), bellek tüketimini sınırlı ve tahmin edilebilir kılar.

3. **CPU Kullanımı**: Boşta durumda CPU kullanımı minimumdur. Periyodik mesaj kontrolleri ve GUI güncellemeleri, önemli bir CPU yükü oluşturmaz.

4. **Ölçeklenebilirlik**: Uygulama, 10'a kadar terminal sekmesini sorunsuz bir şekilde destekleyebilir. Daha fazla sekme için bellek limitleri ve GUI performansı göz önünde bulundurulmalıdır.

5. **Mesaj Kapasitesi**: Sabit boyutlu mesaj tamponu (4096 bayt), belirli bir anda saklanabilecek mesaj sayısını sınırlar. Ancak, dairesel tampon yaklaşımı sayesinde, tampon dolduğunda yeni mesajlar için yer açılır ve sistem sürekli çalışmaya devam eder.

### Sonuç

"Multi-User Communicating Shells" projesi, sistem programlama, GUI geliştirme ve eşzamanlılık yönetimi konularında kapsamlı bir uygulama sunmaktadır. MVC mimarisi kullanılarak modüler ve bakımı kolay bir kod tabanı oluşturulmuştur. POSIX paylaşılan bellek ve semaforlar kullanılarak terminaller arası güvenilir iletişim sağlanmıştır. GTK4 kütüphanesi kullanılarak modern ve kullanıcı dostu bir arayüz oluşturulmuştur.

Mesaj tamponu yönetimi için benimsenen dairesel tampon yaklaşımı, bellek kullanımını sınırlı ve öngörülebilir tutarken, sistemin sürekli çalışmasını sağlar. Bu yaklaşım, eski mesajların kaybedilmesi pahasına, yeni mesajların her zaman iletilebilmesini garanti eder.

Proje, temel kabuk işlevselliğini ve terminaller arası mesajlaşmayı başarıyla birleştirerek, sistem programlama ve GUI geliştirme becerilerini sergilemektedir. Karşılaşılan zorluklar, uygun teknik çözümlerle aşılmış ve gelecekteki iyileştirmeler için sağlam bir temel oluşturulmuştur.
