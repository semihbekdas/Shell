# Multi-User Communicating Shells

## ENG

A terminal-like application that allows multiple shell instances to run in parallel and communicate through a shared message buffer. Built using the MVC (Model-View-Controller) architecture with a graphical user interface provided by the GTK4 library.

### Features

- Create and manage multiple terminal tabs
- Run standard shell commands (`ls`, `cat`, `grep`, etc.)
- Inter-terminal messaging system
- Command redirection and pipeline support
- Communication through shared memory
- User-friendly graphical interface
- Support for command history
- Color-coded terminal output and messages

### Requirements

- Linux operating system (Ubuntu 20.04 or higher recommended) or macOS
- GTK4 library
- GCC compiler
- Make utility
- POSIX-compliant system

### Installation

#### Installing Required Packages

##### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-4-dev
```

##### Fedora
```bash
sudo dnf install gcc make pkgconfig gtk4-devel
```

##### macOS (with Homebrew)
```bash
brew install gtk4
brew install pkg-config
```

#### Building the Project

1. Download or clone the project
2. Navigate to the project directory
3. Build the project with the following command:

```bash
make
```

This command will create an executable file named `multi_shell_app`.

### Usage

#### Starting the Application

After building the project, you can start the application with:

```bash
./multi_shell_app
```

or

```bash
make run
```

#### Basic Usage

- **Adding a New Terminal**: Click the "+" button at the top of the window to add a new terminal tab.
- **Closing a Terminal**: Click the close button next to the tab title to close a terminal. Note: At least one terminal must remain open.
- **Running Commands**: Type standard shell commands in the terminal input area and press Enter to execute them.
- **Sending Messages**: Use the `@msg` prefix to send messages to other terminals. For example:
  ```
  @msg Hello, this is a test message!
  ```
- **Clearing the Terminal**: Use the `clear` command to clear the terminal content.
- **Exiting a Terminal**: Use the `exit` command to close a terminal.

#### Supported Command Features

- **Standard Shell Commands**: All standard shell commands like `ls`, `cat`, `grep`, `echo`, etc. are supported.
- **Directory Navigation**: Use the `cd` command to navigate between directories.
- **Input/Output Redirection**: Use the `>`, `>>`, `<` operators for file redirection operations.
  ```
  ls -la > file.txt
  cat < input.txt
  echo "additional content" >> file.txt
  ```
- **Pipeline**: Use the `|` operator to connect commands.
  ```
  ls -la | grep ".txt" | sort
  ```

#### Command History

- Use the up and down arrow keys to navigate through previously executed commands.
- Each terminal maintains its own command history.

#### Command Examples

Here are some example commands you can use in the application:

```bash
# Basic file operations
ls -la
mkdir new_folder
cd new_folder
touch test.txt
echo "Hello World" > test.txt
cat test.txt

# Using pipelines
ls -la | grep ".txt" | sort
ps aux | grep firefox
find . -name "*.c" | wc -l

# Using redirection
ls -la > list.txt
cat < list.txt
echo "Additional content" >> list.txt

# Messaging
@msg Hello from Terminal 1 to everyone!
```

#### Interactive Commands

Some interactive commands (e.g., `cat` without arguments, `nano`, `vim`, etc.) are not directly supported. When using such commands, you need to specify arguments or use file redirection:

```
cat file.txt    # Supported
cat < file.txt  # Supported
cat             # Not supported (interactive mode)
```

### Project Structure

The project is designed using the MVC (Model-View-Controller) architecture:

- **Model (model.c/h)**: Manages data and background logic. Command execution, process management, and shared memory operations are performed here.
  - Handles shared memory management using POSIX shared memory (`shm_open`, `mmap`) and semaphores (`sem_t`)
  - Implements command execution using `fork()`, `execvp()`, and `pipe()` system calls
  - Manages input/output redirection using `dup2()` system call
  - Provides platform compatibility between macOS and Linux

- **View (view.c/h)**: Manages the user interface. Provides terminal emulation and message panel using GTK4.
  - Implements terminal emulation using `GtkTextView` and `GtkEntry` widgets
  - Manages tabs using `GtkNotebook` widget
  - Provides a message panel for displaying shared messages
  - Uses color coding for terminal output and messages

- **Controller (controller.c/h)**: Coordinates communication between Model and View. Processes user inputs and calls appropriate model functions.
  - Analyzes user input to distinguish between standard shell commands and message commands (`@msg`)
  - Routes commands to appropriate Model functions and relays results to View
  - Periodically checks the shared memory buffer for new messages and updates the View
  - Maintains message history to prevent displaying duplicate messages

- **Main (main.c)**: Starts the application and performs basic configuration.

### Troubleshooting

- **Compilation Errors**: Make sure the GTK4 library is correctly installed.
- **Runtime Errors**: If shared memory objects are not cleaned up properly, you can manually delete the `mymsgbuf` file in the `/dev/shm` directory:
  ```
  rm /dev/shm/mymsgbuf
  ```
- **Semaphore Errors**: If semaphores are not properly cleaned up, you may need to use the following commands:
  ```
  # For Linux
  ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -n1 ipcrm -s
  
  # For macOS
  sem_unlink /mysem
  ```
- **Terminal Output Issues**: If terminal output appears truncated or corrupted, try using the `clear` command to reset the terminal display.
- **Command Execution Problems**: Some commands that require direct terminal access may not work as expected. Use file redirection or specify command arguments when possible.

## TR

Birden fazla kabuk (shell) örneğinin paralel olarak çalışabildiği ve paylaşılan bir mesaj tamponu üzerinden iletişim kurabildiği bir terminal benzeri uygulama. MVC (Model-View-Controller) mimarisi kullanılarak tasarlanmış olup, GTK4 kütüphanesi ile grafiksel kullanıcı arayüzü sağlanmaktadır.

### Özellikler

- Birden fazla terminal sekmesi oluşturma ve yönetme
- Standart kabuk komutlarını çalıştırma (`ls`, `cat`, `grep` vb.)
- Terminaller arası mesajlaşma sistemi
- Komut yönlendirme ve boru hattı (pipe) desteği
- Paylaşılan bellek üzerinden iletişim
- Kullanıcı dostu grafiksel arayüz
- Komut geçmişi desteği
- Renk kodlamalı terminal çıktısı ve mesajlar

### Gereksinimler

- Linux işletim sistemi (Ubuntu 20.04 veya üzeri önerilir) veya macOS
- GTK4 kütüphanesi
- GCC derleyici
- Make aracı
- POSIX uyumlu sistem

### Kurulum

#### Gerekli Paketlerin Yüklenmesi

##### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-4-dev
```

##### Fedora
```bash
sudo dnf install gcc make pkgconfig gtk4-devel
```

##### macOS (Homebrew ile)
```bash
brew install gtk4
brew install pkg-config
```

#### Projeyi Derleme

1. Projeyi indirin veya klonlayın
2. Proje dizinine gidin
3. Aşağıdaki komutu çalıştırarak projeyi derleyin:

```bash
make
```

Bu komut, `multi_shell_app` adlı çalıştırılabilir dosyayı oluşturacaktır.

### Kullanım

#### Uygulamayı Başlatma

Projeyi derledikten sonra, aşağıdaki komutu kullanarak uygulamayı başlatabilirsiniz:

```bash
./multi_shell_app
```

veya

```bash
make run
```

#### Temel Kullanım

- **Yeni Terminal Ekleme**: Pencere üst kısmındaki "+" düğmesine tıklayarak yeni bir terminal sekmesi ekleyebilirsiniz.
- **Terminal Kapatma**: Sekme başlığının yanındaki kapatma düğmesine tıklayarak terminali kapatabilirsiniz. Not: En az bir terminal her zaman açık kalmalıdır.
- **Komut Çalıştırma**: Terminal giriş alanına standart kabuk komutlarını yazıp Enter tuşuna basarak çalıştırabilirsiniz.
- **Mesaj Gönderme**: Diğer terminallere mesaj göndermek için `@msg` önekini kullanabilirsiniz. Örneğin:
  ```
  @msg Merhaba, bu bir test mesajıdır!
  ```
- **Terminal Temizleme**: Terminal içeriğini temizlemek için `clear` komutunu kullanabilirsiniz.
- **Terminali Kapatma**: Bir terminali kapatmak için `exit` komutunu kullanabilirsiniz.

#### Desteklenen Komut Özellikleri

- **Standart Kabuk Komutları**: `ls`, `cat`, `grep`, `echo` vb. tüm standart kabuk komutları desteklenmektedir.
- **Dizin Değiştirme**: `cd` komutu ile dizinler arası geçiş yapabilirsiniz.
- **Giriş/Çıkış Yönlendirme**: `>`, `>>`, `<` operatörleri ile dosya yönlendirme işlemleri yapabilirsiniz.
  ```
  ls -la > dosya.txt
  cat < girdi.txt
  echo "ek içerik" >> dosya.txt
  ```
- **Boru Hattı (Pipe)**: `|` operatörü ile komutları birbirine bağlayabilirsiniz.
  ```
  ls -la | grep ".txt" | sort
  ```

#### Komut Geçmişi

- Daha önce çalıştırılan komutlar arasında gezinmek için yukarı ve aşağı ok tuşlarını kullanabilirsiniz.
- Her terminal kendi komut geçmişini tutar.

#### Komut Örnekleri

Aşağıda, uygulamada kullanabileceğiniz bazı komut örnekleri verilmiştir:

```bash
# Temel dosya işlemleri
ls -la
mkdir yeni_klasor
cd yeni_klasor
touch test.txt
echo "Merhaba Dünya" > test.txt
cat test.txt

# Boru hattı kullanımı
ls -la | grep ".txt" | sort
ps aux | grep firefox
find . -name "*.c" | wc -l

# Yönlendirme kullanımı
ls -la > liste.txt
cat < liste.txt
echo "Ek içerik" >> liste.txt

# Mesajlaşma
@msg Terminal 1'den herkese merhaba!
```

#### İnteraktif Komutlar

Bazı interaktif komutlar (örn. argümansız `cat`, `nano`, `vim` vb.) doğrudan desteklenmemektedir. Bu tür komutları kullanırken argüman belirtmeniz veya dosya yönlendirme kullanmanız gerekmektedir:

```
cat dosya.txt    # Desteklenir
cat < dosya.txt  # Desteklenir
cat              # Desteklenmez (interaktif mod)
```

### Proje Yapısı

Proje, MVC (Model-View-Controller) mimarisi kullanılarak tasarlanmıştır:

- **Model (model.c/h)**: Veri ve arka plan mantığını yönetir. Komut çalıştırma, süreç yönetimi ve paylaşılan bellek işlemleri burada gerçekleştirilir.
  - POSIX paylaşılan bellek (`shm_open`, `mmap`) ve semaforlar (`sem_t`) kullanarak paylaşılan bellek yönetimini sağlar
  - `fork()`, `execvp()` ve `pipe()` sistem çağrıları kullanarak komut çalıştırmayı gerçekleştirir
  - `dup2()` sistem çağrısı kullanarak giriş/çıkış yönlendirmesini yönetir
  - macOS ve Linux arasında platform uyumluluğu sağlar

- **View (view.c/h)**: Kullanıcı arayüzünü yönetir. GTK4 kullanarak terminal emülasyonu ve mesaj paneli sağlar.
  - `GtkTextView` ve `GtkEntry` widget'ları kullanarak terminal emülasyonu sağlar
  - `GtkNotebook` widget'ı kullanarak sekmeleri yönetir
  - Paylaşılan mesajları görüntülemek için bir mesaj paneli sunar
  - Terminal çıktıları ve mesajlar için renk kodlaması kullanır

- **Controller (controller.c/h)**: Model ve View arasındaki iletişimi koordine eder. Kullanıcı girişlerini işler ve uygun model fonksiyonlarını çağırır.
  - Kullanıcı girişlerini analiz ederek standart kabuk komutları ve mesaj komutları (`@msg`) arasında ayrım yapar
  - Komutları uygun Model fonksiyonlarına yönlendirir ve sonuçları View'a iletir
  - Paylaşılan bellek tamponunu periyodik olarak kontrol ederek yeni mesajları tespit eder ve View'ı günceller
  - Tekrarlanan mesajların görüntülenmesini önlemek için mesaj geçmişi tutar

- **Main (main.c)**: Uygulamayı başlatır ve temel yapılandırmayı gerçekleştirir.

### Sorun Giderme

- **Derleme Hataları**: GTK4 kütüphanesinin doğru şekilde yüklendiğinden emin olun.
- **Çalışma Zamanı Hataları**: Paylaşılan bellek nesnelerinin temizlenmemesi durumunda, `/dev/shm` dizinindeki `mymsgbuf` dosyasını manuel olarak silebilirsiniz:
  ```
  rm /dev/shm/mymsgbuf
  ```
- **Terminal Çıktı Sorunları**: Terminal çıktısı kesik veya bozuk görünüyorsa, terminal görüntüsünü sıfırlamak için `clear` komutunu kullanmayı deneyin.
- **Komut Çalıştırma Sorunları**: Doğrudan terminal erişimi gerektiren bazı komutlar beklendiği gibi çalışmayabilir. Mümkün olduğunda dosya yönlendirme kullanın veya komut argümanlarını belirtin.
