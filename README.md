# ESP32 Master Control Unit


## 🎯 Genel Bakış

Bu proje, akıllı yat otomasyonu için merkezi bir kontrol ünitesidir. CAN bus protokolü üzerinden slave kartlarla (LED, Röle) haberleşir ve kullanıcı dostu bir LVGL arayüzü ile kontrol sağlar.

## ✨ Özellikler

### Kontrol Sayfaları
- **Toilet Control** - 6 kanal PWM kontrolü
- **Lighting Control** - 6 kanal LED parlaklık kontrolü (0-100%)
- **Sockets Control** - 6 kanal Röle açma/kapama

### CAN Bus Sistemi
- **Dinamik ID Atama** - Slave kartlar otomatik ID alır
- **Node Tipi Tanıma** - LED, Röle, Sensör kartlarını ayırt eder
- **Heartbeat Mekanizması** - Bağlantı durumu sürekli kontrol edilir
- **NVS Hafıza** - Kayıtlı kartlar sistem yeniden başlatıldığında hatırlanır
- **Master ID** - Her ESP32'nin benzersiz ID'si MAC adresinden türetilir

### Kullanıcı Arayüzü
- **Modern LVGL Tasarım** - Dokunmatik LCD desteği
- **Gece/Gündüz Mod** - İki farklı renk teması
- **Şifre Koruması** - Ayarlar sayfası 6 haneli PIN ile korunur
- **Özelleştirme** - Buton isimlerini değiştirebilme
- **Geçmiş Kaydı** - İsim değişikliklerinin kaydı tutulur


## 🔌 CAN Bus Pinleri

```c
#define CAN_TX_PIN  GPIO_NUM_X
#define CAN_RX_PIN  GPIO_NUM_X
```

## 📡 CAN Protokolü

### Komut Yapısı

**LED Kartı:**
- `0x10` - Toggle
- `0x11` - Set (On/Off)
- `0x12` - Set Brightness (0-1000)

**Röle Kartı:**
- `0x20` - Toggle
- `0x21` - Set (On/Off)

### ID Aralıkları
- `0x100-0x28F` - Dinamik ID'ler (LED, Röle, Sensör)
- `0x290-0x29F` - Protokol mesajları
- `0x390-0x3EF` - Heartbeat mesajları

### Çalışma Mantığı

1. **İlk Açılış:**
   - Slave kart CAN'e bağlanır
   - ID request mesajı gönderir (Unique ID + Tip bilgisi)
   - Master uygun ID atar
   - Slave atanan ID'yi onaylar

2. **Normal Çalışma:**
   - Slave kartlar her 2 saniyede heartbeat gönderir
   - Master, 5 saniye heartbeat gelmezse kartı "LOST" işaretler
   - Kayıp kartlar 60 saniye sonra sistemden silinir

3. **Yeniden Bağlanma:**
   - Daha önce kayıtlı kart ID'sini hatırlar
   - Master, aynı ID'yi tekrar verir

## 🎨 GUI Kullanımı

### İlk Kurulum
1.  Sistem açılışta şifre oluşturma ekranı gelir
2. 6 haneli PIN belirleyin
3. Ana menüye yönlendirilirsiniz

### Kontrol Sayfaları
- **Sidebar:** 6 kanal arasında seçim
- **Power Button:** Açma/Kapama
- **Slider:** Parlaklık kontrolü (sadece LED)
- **Close All:** Tüm kanalları kapat
- **Gösterge Işıkları:** Yan menüde aktif kanallar yanıyor

### Ayarlar
- **Button Naming:** Her sayfada buton isimlerini özelleştirin
- **History:** İsim değişikliklerini görüntüleyin
- **Edit Password:** Mevcut şifreyi değiştirin

## 💾 NVS Kullanımı

Sistem aşağıdaki verileri flash'ta saklar:
- Kayıtlı CAN node'ları (UID, CAN ID, Tip)
- Kullanıcı şifresi
- Buton isimleri
- LED parlaklık değerleri
- İsim değişiklik geçmişi


## 📝 Notlar

- **Timeout Süreleri:** Heartbeat timeout 5 saniye, node temizleme 60 saniye
- **Master ID:** MAC adresinin son 2 byte'ından oluşur
- **Parlaklık:** 0-1000 arası (0-100% olarak gösterilir)
- **NVS Namespace'ler:** 
  - `can_nodes` - Node kayıtları
  - `app_settings` - Şifre
  - `button_names` - Buton isimleri
  - `brightness` - Parlaklık değerleri

---
**Geliştirici:** YusufZiya12  
**Platform:** ESP-IDF + LVGL
