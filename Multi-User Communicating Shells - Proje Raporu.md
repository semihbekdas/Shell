# Multi-User Communicating Shells - Proje Raporu

## Giriş

Bu rapor, "Multi-User Communicating Shells with Shared Messaging" projesinin tasarım seçimleri, uygulama detayları ve karşılaşılan zorlukları ele almaktadır. Proje, birden fazla kabuk (shell) örneğinin paralel olarak çalışabildiği ve paylaşılan bir mesaj tamponu üzerinden iletişim kurabildiği bir terminal benzeri uygulama sunmaktadır. MVC (Model-View-Controller) mimarisi kullanılarak tasarlanmış olup, GTK4 kütüphanesi ile grafiksel kullanıcı arayüzü sağlanmaktadır.

## Tasarım Seçimleri

### MVC Mimarisi

Proje, Model-View-Controller (MVC) mimari deseni kullanılarak tasarlanmıştır. Bu seçim, aşağıdaki avantajları sağlamıştır:

1. **Modülerlik**: Uygulama mantığı (Model), kullanıcı arayüzü (View) ve kontrol akışı (Controller) birbirinden ayrılmıştır. Bu sayede her bir bileşen bağımsız olarak geliştirilebilir ve test edilebilir.

2. **Bakım Kolaylığı**: Kodun farklı sorumlulukları olan bölümlere ayrılması, bakım ve hata ayıklama süreçlerini kolaylaştırmıştır.

3. **Genişletilebilirlik**: Yeni özelliklerin eklenmesi, mevcut yapıyı bozmadan gerçekleştirilebilir. Örneğin, yeni bir komut türü veya mesajlaşma özelliği eklemek için sadece ilgili bileşenlerde değişiklik yapmak yeterlidir.

4. **Yeniden Kullanılabilirlik**: Her bir bileşen, diğer bileşenlerden bağımsız olarak yeniden kullanılabilir. Örneğin, Model bileşeni farklı bir arayüz ile kullanılabilir.

### Bileşen Detayları

#### Model Bileşeni

Model bileşeni, uygulamanın veri ve iş mantığını yönetir. Temel sorumlulukları şunlardır:

1. **Paylaşılan Bellek Yönetimi**: POSIX paylaşılan bellek (`shm_open`, `mmap`) ve semaforlar (`sem_t`) kullanılarak terminaller arası iletişim sağlanmıştır. Bu yaklaşım, farklı süreçler arasında verimli veri paylaşımı sağlar.

2. **Komut Çalıştırma**: `fork()`, `execvp()` ve `pipe()` sistem çağrıları kullanılarak kabuk komutlarının çalıştırılması gerçekleştirilmiştir. Bu, standart Unix/Linux komutlarının desteklenmesini sağlar.

3. **Giriş/Çıkış Yönlendirme**: `dup2()` sistem çağrısı kullanılarak standart giriş/çıkış akışlarının yönlendirilmesi sağlanmıştır. Bu, `>`, `>>`, `<` gibi yönlendirme operatörlerinin ve `|` boru hattı operatörünün desteklenmesini sağlar.

4. **Platform Uyumluluğu**: macOS ve Linux arasındaki farklılıkları ele almak için koşullu derleme (`#ifdef __APPLE__`) kullanılmıştır. Bu, uygulamanın farklı işletim sistemlerinde çalışabilmesini sağlar.

5. **Mesaj Tamponu Yönetimi**: Paylaşılan bellek üzerinde sabit boyutlu bir mesaj tamponu (4096 bayt) kullanılmıştır. Tampon dolduğunda, yeni mesajlar için yer açmak amacıyla tampon başa döndürülür ve eski mesajların üzerine yazılır. Bu yaklaşım, bellek kullanımını sınırlı ve öngörülebilir tutar.

#### View Bileşeni

View bileşeni, kullanıcı arayüzünü yönetir. GTK4 kütüphanesi kullanılarak oluşturulmuştur. Temel özellikleri şunlardır:

1. **Terminal Emülasyonu**: Her bir terminal sekmesi, komut girişi ve çıktı görüntüleme alanlarını içerir. `GtkTextView` ve `GtkEntry` widget'ları kullanılarak terminal benzeri bir deneyim sağlanmıştır.

2. **Sekme Yönetimi**: `GtkNotebook` widget'ı kullanılarak birden fazla terminal sekmesinin yönetilmesi sağlanmıştır. Kullanıcılar yeni sekmeler ekleyebilir ve mevcut sekmeleri kapatabilir.

3. **Mesaj Paneli**: Paylaşılan mesajların görüntülendiği ayrı bir panel bulunmaktadır. Bu panel, terminaller arası iletişimi görselleştirir.

4. **Renk Kodlaması**: Terminal çıktıları ve mesajlar, kaynağını belirtmek için renk kodlaması ile görüntülenir. Bu, kullanıcı deneyimini iyileştirir ve bilgilerin daha kolay ayırt edilmesini sağlar.

#### Controller Bileşeni

Controller bileşeni, Model ve View arasındaki iletişimi koordine eder. Temel sorumlulukları şunlardır:

1. **Kullanıcı Girişi İşleme**: Kullanıcı girişlerini analiz ederek standart kabuk komutları ve mesaj komutları (`@msg`) arasında ayrım yapar.

2. **Komut Yönlendirme**: Komutları uygun Model fonksiyonlarına yönlendirir ve sonuçları View'a iletir.

3. **Periyodik Güncelleme**: Paylaşılan bellek tamponunu periyodik olarak kontrol ederek yeni mesajları tespit eder ve View'ı günceller. Bu, GTK'nın `g_timeout_add()` fonksiyonu kullanılarak gerçekleştirilmiştir.

4. **Mesaj Geçmişi**: Son mesajları bir tampon içinde saklayarak tekrarlanan mesajların görüntülenmesini önler.

### Teknik Seçimler

1. **GTK4 Kullanımı**: Modern ve platform bağımsız bir GUI kütüphanesi olan GTK4 tercih edilmiştir. Bu, uygulamanın farklı Linux dağıtımlarında tutarlı bir görünüm ve davranış sergilemesini sağlar.

2. **POSIX Paylaşılan Bellek**: Terminaller arası iletişim için POSIX paylaşılan bellek mekanizması seçilmiştir. Bu, süreçler arası verimli veri paylaşımı sağlar ve semaforlar ile senkronizasyon problemleri çözülmüştür.

3. **Esnek Dizi Kullanımı**: Paylaşılan bellek yapısında esnek dizi (`char msgbuf[]`) kullanılarak bellek kullanımı optimize edilmiştir.

4. **Semafor Senkronizasyonu**: Paylaşılan belleğe erişim, semaforlar (`sem_t`) kullanılarak senkronize edilmiştir. Bu, veri bütünlüğünü korur ve yarış koşullarını önler.

5. **Boru Hattı ve Yönlendirme Desteği**: Standart Unix/Linux kabuk özelliklerini desteklemek için boru hattı ve yönlendirme operatörleri uygulanmıştır.

6. **Dairesel Tampon Yaklaşımı**: Mesaj tamponu için dairesel tampon yaklaşımı benimsenmiştir. Tampon dolduğunda, yeni mesajlar için yer açmak amacıyla tampon başa döndürülür ve eski mesajların üzerine yazılır.

## Karşılaşılan Zorluklar ve Çözümler

### 1. Süreçler Arası İletişim

**Zorluk**: Farklı terminal süreçleri arasında verimli ve güvenilir iletişim sağlamak.

**Çözüm**: POSIX paylaşılan bellek ve semaforlar kullanılarak süreçler arası iletişim sağlanmıştır. Semaforlar, paylaşılan belleğe eşzamanlı erişimi kontrol ederek veri bütünlüğünü korur.

```c
sem_wait(&shmp->sem); // Kritik bölgeye giriş
// Paylaşılan bellek işlemleri
sem_post(&shmp->sem); // Kritik bölgeden çıkış
```

### 2. Platform Uyumluluğu

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

### 3. Komut Çalıştırma ve Çıktı Yakalama

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
    // select() ile timeout'lu okuma
    // ...
}
```

### 4. Boru Hattı ve Yönlendirme Operatörleri

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

### 5. İnteraktif Komutlar

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

### 6. GTK4 Entegrasyonu

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

### 7. Mesaj Tamponu Yönetimi

**Zorluk**: Çok sayıda mesaj gönderildiğinde bellek taşması sorunlarını önlemek.

**Çözüm**: Sabit boyutlu bir mesaj tamponu (4096 bayt) kullanılmış ve tampon dolduğunda dairesel tampon mantığıyla eski mesajların üzerine yazma yaklaşımı benimsenmiştir. Bu, bellek kullanımını kontrol altında tutarken, yeni mesajların her zaman iletilebilmesini sağlar.

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

Bu yaklaşım, bellek kullanımını sabit tutar ve sistem kaynaklarının tükenmesini önler. Ancak, eski mesajların kaybedilmesi gibi bir dezavantajı vardır. Gelecekteki sürümlerde, dinamik bellek tahsisi veya dosya tabanlı bir mesaj geçmişi sistemi ile bu sınırlama aşılabilir.

## Performans Değerlendirmesi

Uygulama, aşağıdaki performans kriterleri açısından değerlendirilmiştir:

1. **Tepki Süresi**: Kullanıcı komutlarına tepki süresi, çoğu durumda milisaniyeler mertebesindedir. Ancak, yoğun işlem gerektiren komutlar (örn. `find /`) için tepki süresi artabilir.

2. **Bellek Kullanımı**: Uygulama, makul bir bellek ayak izine sahiptir. Her terminal sekmesi için ek bellek kullanımı gerekir, ancak bu artış doğrusal ve öngörülebilirdir. Mesaj tamponu için sabit boyutlu bellek kullanımı (4096 bayt), bellek tüketimini sınırlı ve tahmin edilebilir kılar.

3. **CPU Kullanımı**: Boşta durumda CPU kullanımı minimumdur. Periyodik mesaj kontrolleri ve GUI güncellemeleri, önemli bir CPU yükü oluşturmaz.

4. **Ölçeklenebilirlik**: Uygulama, 10'a kadar terminal sekmesini sorunsuz bir şekilde destekleyebilir. Daha fazla sekme için bellek limitleri ve GUI performansı göz önünde bulundurulmalıdır.

5. **Mesaj Kapasitesi**: Sabit boyutlu mesaj tamponu (4096 bayt), belirli bir anda saklanabilecek mesaj sayısını sınırlar. Ancak, dairesel tampon yaklaşımı sayesinde, tampon dolduğunda yeni mesajlar için yer açılır ve sistem sürekli çalışmaya devam eder.

## Sonuç

"Multi-User Communicating Shells with Shared Messaging" projesi, sistem programlama, GUI geliştirme ve eşzamanlılık yönetimi konularında kapsamlı bir uygulama sunmaktadır. MVC mimarisi kullanılarak modüler ve bakımı kolay bir kod tabanı oluşturulmuştur. POSIX paylaşılan bellek ve semaforlar kullanılarak terminaller arası güvenilir iletişim sağlanmıştır. GTK4 kütüphanesi kullanılarak modern ve kullanıcı dostu bir arayüz oluşturulmuştur.

Mesaj tamponu yönetimi için benimsenen dairesel tampon yaklaşımı, bellek kullanımını sınırlı ve öngörülebilir tutarken, sistemin sürekli çalışmasını sağlar. Bu yaklaşım, eski mesajların kaybedilmesi pahasına, yeni mesajların her zaman iletilebilmesini garanti eder.

Proje, temel kabuk işlevselliğini ve terminaller arası mesajlaşmayı başarıyla birleştirerek, sistem programlama ve GUI geliştirme becerilerini sergilemektedir. Karşılaşılan zorluklar, uygun teknik çözümlerle aşılmış ve gelecekteki iyileştirmeler için sağlam bir temel oluşturulmuştur.
