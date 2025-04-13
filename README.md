# Multi-User Communicating Shells

Bu proje, birden fazla kabuk (shell) örneğinin paralel olarak çalışabildiği ve paylaşılan bir mesaj tamponu üzerinden iletişim kurabildiği bir terminal benzeri uygulama sunar. MVC (Model-View-Controller) mimarisi kullanılarak tasarlanmış olup, GTK4 kütüphanesi ile grafiksel kullanıcı arayüzü sağlanmaktadır.

## Özellikler

- Birden fazla terminal sekmesi oluşturma ve yönetme
- Standart kabuk komutlarını çalıştırma (`ls`, `cat`, `grep` vb.)
- Terminaller arası mesajlaşma
- Komut yönlendirme ve boru hattı (pipe) desteği
- Paylaşılan bellek üzerinden iletişim
- Kullanıcı dostu grafiksel arayüz

## Gereksinimler

- Linux işletim sistemi (Ubuntu 20.04 veya üzeri önerilir)
- GTK4 kütüphanesi
- GCC derleyici
- Make aracı
- POSIX uyumlu sistem

## Kurulum

### Gerekli Paketlerin Yüklenmesi

```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-4-dev
```

### Projeyi Derleme

1. Projeyi indirin veya klonlayın
2. Proje dizinine gidin
3. Aşağıdaki komutu çalıştırarak projeyi derleyin:

```bash
make
```

Bu komut, `multi_shell_app` adlı çalıştırılabilir dosyayı oluşturacaktır.

## Kullanım

### Uygulamayı Başlatma

Projeyi derledikten sonra, aşağıdaki komutu kullanarak uygulamayı başlatabilirsiniz:

```bash
./multi_shell_app
```

veya

```bash
make run
```

### Temel Kullanım

- **Yeni Terminal Ekleme**: Pencere üst kısmındaki "+" düğmesine tıklayarak yeni bir terminal sekmesi ekleyebilirsiniz.
- **Terminal Kapatma**: Sekme başlığının yanındaki kapatma düğmesine tıklayarak terminali kapatabilirsiniz. Not: En az bir terminal her zaman açık kalmalıdır.
- **Komut Çalıştırma**: Terminal giriş alanına standart kabuk komutlarını yazıp Enter tuşuna basarak çalıştırabilirsiniz.
- **Mesaj Gönderme**: Diğer terminallere mesaj göndermek için `@msg` önekini kullanabilirsiniz. Örneğin:
  ```
  @msg Merhaba, bu bir test mesajıdır!
  ```
- **Terminal Temizleme**: Terminal içeriğini temizlemek için `clear` komutunu kullanabilirsiniz.
- **Terminali Kapatma**: Bir terminali kapatmak için `exit` komutunu kullanabilirsiniz.

### Desteklenen Komut Özellikleri

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

### İnteraktif Komutlar

Bazı interaktif komutlar (örn. argümansız `cat`, `nano`, `vim` vb.) doğrudan desteklenmemektedir. Bu tür komutları kullanırken argüman belirtmeniz veya dosya yönlendirme kullanmanız gerekmektedir:

```
cat dosya.txt    # Desteklenir
cat < dosya.txt  # Desteklenir
cat              # Desteklenmez (interaktif mod)
```

## Proje Yapısı

Proje, MVC (Model-View-Controller) mimarisi kullanılarak tasarlanmıştır:

- **Model (model.c/h)**: Veri ve arka plan mantığını yönetir. Komut çalıştırma, süreç yönetimi ve paylaşılan bellek işlemleri burada gerçekleştirilir.
- **View (view.c/h)**: Kullanıcı arayüzünü yönetir. GTK4 kullanarak terminal emülasyonu ve mesaj paneli sağlar.
- **Controller (controller.c/h)**: Model ve View arasındaki iletişimi koordine eder. Kullanıcı girişlerini işler ve uygun model fonksiyonlarını çağırır.
- **Main (main.c)**: Uygulamayı başlatır ve temel yapılandırmayı gerçekleştirir.

## Sorun Giderme

- **Derleme Hataları**: GTK4 kütüphanesinin doğru şekilde yüklendiğinden emin olun.
- **Çalışma Zamanı Hataları**: Paylaşılan bellek nesnelerinin temizlenmemesi durumunda, `/dev/shm` dizinindeki `mymsgbuf` dosyasını manuel olarak silebilirsiniz:
  ```
  rm /dev/shm/mymsgbuf
  ```
- **Semafor Hataları**: Semaforların düzgün temizlenmemesi durumunda, sistemi yeniden başlatmak gerekebilir.

