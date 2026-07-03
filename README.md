# CAN Bus Tabanlı Merkezi Yat Otomasyon Sistemi

Bu proje, bir yat içerisindeki yedi farklı odada bulunan aydınlatma ve röle sistemlerinin merkezi bir dokunmatik ekran üzerinden yönetilmesi amacıyla geliştirilmiş dağıtık bir gömülü sistem altyapısıdır.

Sistem; merkezi kontrol kartı, oda yönetim kartları, LED kontrol kartları ve röle kontrol kartlarından oluşur. Kartlar arasındaki haberleşme CAN Bus üzerinden gerçekleştirilir.

## Sistem Mimarisi

```text
Merkezi Kontrol Kartı
        |
        | CAN Bus
        |
        +-- Oda Master Kartı 1
        |      +-- LED Kontrol Kartı
        |      +-- Röle Kontrol Kartı
        |
        +-- Oda Master Kartı 2
        |      +-- LED Kontrol Kartı
        |      +-- Röle Kontrol Kartı
        |
        +-- ...
        |
        +-- Oda Master Kartı 7
               +-- LED Kontrol Kartı
               +-- Röle Kontrol Kartı
```

Merkezi kart, kullanıcı arayüzünü ve odalar arası yönetimi üstlenir. Her oda master kartı ise kendi odasında bulunan LED ve röle düğümlerinin kayıt, kimliklendirme, bağlantı takibi ve mesaj yönlendirme işlemlerini gerçekleştirir.

## Temel Bileşenler

### Merkezi Kontrol Kartı

Merkezi kontrol kartı, sistemin kullanıcıya görünen ana yönetim birimidir.

Başlıca görevleri:

- Yedi farklı oda arasında seçim yapılmasını sağlamak
- Oda master kartlarını otomatik olarak algılamak
- Oda kartlarına benzersiz oda kimlikleri atamak
- Aktif, bekleyen ve bağlantısı kaybolan odaları takip etmek
- Her odadaki LED ve röle kartlarının durum bilgilerini almak
- Aydınlatma açma, kapama ve parlaklık komutlarını oluşturmak
- Röle çıkışları için açma ve kapama komutları oluşturmak
- Oda ve modül bilgilerini kalıcı hafızada saklamak
- Dokunmatik ekran üzerinden merkezi yönetim arayüzü sunmak

Merkezi kartın arayüzünde aydınlatma, priz ve sistem yönetimi için ayrı kontrol sayfaları bulunur.

### Oda Master Kartları

Her oda için bir oda master kartı kullanılır. Bu kart, merkezi kontrol kartı ile odada bulunan uç kontrol kartları arasında köprü görevi görür.

Başlıca görevleri:

- Merkezi karta kayıt isteği göndermek
- Merkezi kart tarafından atanan oda kimliğini almak
- Oda kimliğini kalıcı hafızada saklamak
- Odadaki LED ve röle kartlarını otomatik olarak keşfetmek
- Uç kartlara dinamik CAN kimlikleri atamak
- Merkezi karttan gelen komutları hedef uç karta yönlendirmek
- Bağlı kartların heartbeat mesajlarını takip etmek
- Aktif ve bağlantısı kaybolan kartları belirlemek
- Merkezi karta oda durum bilgisi göndermek

Bu yapı sayesinde oda içerisindeki kartların CAN kimliklerinin kaynak kodda sabitlenmesi gerekmez.

### LED Kontrol Kartları

LED kontrol kartları, oda içerisindeki aydınlatma çıkışlarını yönetir.

Kartın desteklediği temel işlemler:

- Altı bağımsız LED kanalını kontrol etmek
- LED kanallarını açmak ve kapatmak
- LED parlaklık seviyesini değiştirmek
- Yazılımsal PWM ile parlaklık kontrolü sağlamak
- Oda master kartından dinamik CAN kimliği almak
- Kendisine yönlendirilen CAN komutlarını işlemek
- Bağlantı durumunu heartbeat mesajlarıyla bildirmek

LED parlaklık kontrolü, kullanıcı arayüzündeki yüzde değeri ile uç karttaki PWM seviyesi arasında dönüştürülür.

### Röle Kontrol Kartları

Röle kontrol kartları, oda içerisindeki elektriksel yüklerin anahtarlanmasını sağlar.

Kartın desteklediği temel işlemler:

- Altı bağımsız röle çıkışını kontrol etmek
- Tek bir röleyi açmak veya kapatmak
- Röle durumunu tersine çevirmek
- Tüm röleleri aynı anda açmak veya kapatmak
- Oda master kartından dinamik CAN kimliği almak
- Gelen CAN komutlarını fiziksel GPIO çıkışlarına uygulamak
- Bağlantı durumunu heartbeat mesajlarıyla bildirmek

## Çalışma Prensibi

### 1. Oda Kartlarının Kaydı

Oda master kartları sistem açıldığında merkezi karta kayıt isteği gönderir.

Merkezi kart:

1. Kayıt isteğini gönderen kartı tanımlar.
2. Kart daha önce kayıtlıysa mevcut oda bilgisini geri yükler.
3. Yeni bir kartsa kullanılabilir oda numaralarından birini atar.
4. Oda kimliğini ilgili master karta gönderir.
5. Onay mesajı alındığında odayı aktif olarak işaretler.

Sistem en fazla yedi oda master kartını yönetmek üzere tasarlanmıştır.

### 2. Uç Kartların Otomatik Tanınması

Oda master kartı, kendi CAN ağına bağlanan LED ve röle kartlarını algılar.

Yeni bir kart bağlandığında:

1. Uç kart benzersiz cihaz kimliğiyle kayıt isteği gönderir.
2. Oda master kartı kartın türünü belirler.
3. Karta oda, cihaz türü ve sıra bilgisi içeren bir CAN kimliği atar.
4. Uç kart atanan kimliği kalıcı hafızasına kaydeder.
5. Kart heartbeat mesajları göndermeye başlar.

Bu yapı, aynı türden birden fazla kartın aynı CAN ağı üzerinde çalışmasına olanak tanır.

### 3. Merkezi Kontrol

Kullanıcı merkezi ekrandan önce kontrol etmek istediği odayı, ardından ilgili LED veya röle kanalını seçer.

Kontrol akışı aşağıdaki şekilde ilerler:

```text
Kullanıcı Arayüzü
        |
        v
Merkezi Kontrol Kartı
        |
        v
Seçilen Odanın Master Kartı
        |
        v
LED veya Röle Kontrol Kartı
        |
        v
Fiziksel Çıkış
```

Merkezi kart oda hedefli kontrol mesajı oluşturur. Oda master kartı mesajı alır, ilgili LED veya röle düğümünü bulur ve komutu uç karta yönlendirir.

### 4. Bağlantı Takibi

Merkezi kart ve oda master kartları, bağlı cihazların çalışmaya devam ettiğini heartbeat mesajları üzerinden takip eder.

Belirli bir süre boyunca mesaj alınmayan cihazlar bağlantısı kaybolmuş olarak işaretlenir. Cihaz tekrar haberleşmeye başladığında sistem tarafından yeniden aktif duruma geçirilebilir.

## Merkezi Arayüz

Merkezi kontrol kartında LVGL tabanlı dokunmatik kullanıcı arayüzü bulunur.

Arayüz üzerinden:

- Oda seçimi yapılabilir.
- Altı aydınlatma kanalı ayrı ayrı yönetilebilir.
- Aydınlatma parlaklığı ayarlanabilir.
- Altı röle kanalı ayrı ayrı yönetilebilir.
- Tüm aydınlatmalar toplu olarak kapatılabilir.
- Oda ve kart bağlantı durumları görüntülenebilir.
- Kanal isimleri kullanıcıya göre düzenlenebilir.
- Gündüz ve gece görünümü kullanılabilir.
- Ayarlar parola ile korunabilir.

## CAN Bus Haberleşmesi

Sistemdeki bütün kartlar aynı CAN Bus altyapısı üzerinden haberleşir.

Haberleşme protokolü aşağıdaki mesaj gruplarını kapsar:

- Merkezi karta kayıt mesajları
- Oda kimliği atama ve onay mesajları
- Uç kart kimliği isteme ve atama mesajları
- LED kontrol mesajları
- Röle kontrol mesajları
- Parlaklık kontrol mesajları
- Heartbeat mesajları
- Oda ve düğüm durum mesajları
- Yeniden kayıt ve yeniden kimliklendirme mesajları

CAN kimlikleri; oda numarası, cihaz türü ve düğüm sırası bilgilerini içerecek şekilde dinamik olarak oluşturulur.

## Kalıcı Veri Yönetimi

Sistem, yeniden başlatma sonrasında yapılandırma bilgilerinin korunması için kalıcı hafıza kullanır.

Saklanan başlıca bilgiler:

- Oda kartı ile oda numarası eşleşmeleri
- Merkezi kart tarafından tanınan oda bilgileri
- Oda master kartlarının tanıdığı uç kartlar
- Uç kartlara atanmış CAN kimlikleri
- LED parlaklık seviyeleri
- Kullanıcı tarafından değiştirilen kanal isimleri
- Arayüz ayarları
- Yönetici parolası

Bu sayede daha önce tanımlanmış kartlar sistem yeniden başlatıldığında tekrar kullanılabilir.

## Branch Yapısı

Projenin farklı donanım birimlerine ait kaynak kodları ayrı branch'lerde tutulmaktadır.

| Branch | İçerik |
|---|---|
| `main` | Projenin genel açıklaması |
| `MerkeziMasterKart` | Dokunmatik arayüze sahip merkezi kontrol kartı yazılımı |
| `odaESP` | Her odada çalışan oda master kartı yazılımı |
| `Led` | STM32 tabanlı altı kanallı LED kontrol kartı yazılımı |
| `Röle` | STM32 tabanlı altı kanallı röle kontrol kartı yazılımı |

## Mevcut Geliştirme Durumu

Projede aşağıdaki temel altyapılar uygulanmıştır:

- Merkezi kart ile oda master kartlarının kayıt süreci
- Yedi odaya kadar otomatik oda kimliği atama
- Oda içerisindeki LED ve röle kartlarının otomatik tanınması
- Dinamik CAN kimliği yönetimi
- Heartbeat tabanlı bağlantı takibi
- Merkezi aydınlatma ve röle kontrol arayüzleri
- LED kartında açma, kapama ve parlaklık kontrolü
- Röle kartında altı fiziksel çıkışın kontrolü
- Ayarların kalıcı hafızada saklanması

Merkezi kart, oda master kartı ve uç kart katmanlarında kontrol fonksiyonları bulunmaktadır. Bununla birlikte, branch'lerdeki merkezi kontrol paketleri ile uç kartların beklediği mesaj formatlarının tamamen eşleştirilmesi ve bütün sistemin uçtan uca donanım üzerinde doğrulanması gerekmektedir.

## Kullanım Alanları

Bu sistem temel olarak yat ve tekne otomasyonu için geliştirilmiştir. Aynı mimari aşağıdaki sistemlere de uyarlanabilir:

- Karavan otomasyonu
- Akıllı bina kontrolü
- Dağıtık aydınlatma sistemleri
- Merkezi röle ve yük yönetimi
- Endüstriyel CAN Bus ağları
- Modüler araç içi kontrol sistemleri
