# 📡 ESP32 Container Monitor dengan Telegram & Google Sheets

Proyek ini merupakan sistem monitoring suhu dan energi menggunakan ESP32 yang terhubung ke sensor DS18B20 dan PZEM004Tv30. Sistem dilengkapi dengan input tombol fisik dan integrasi Telegram untuk notifikasi serta pengiriman data ke Google Sheets melalui Apps Script.

---

## 🚀 Fitur

- Monitoring suhu dengan sensor DS18B20
- Monitoring voltase, arus, daya, energi, dan frekuensi dari PZEM004Tv30
- Kirim data ke Google Sheets setiap 30 detik
- Kontrol setpoint suhu melalui tombol fisik dan Telegram
- Reset data energi dan Google Sheets via Telegram atau tombol
- Notifikasi suhu melebihi atau kembali ke batas normal ±2°C
- Penyimpanan data energi dan setpoint ke EEPROM internal

---

## 🛠️ Perangkat Keras

- ESP32 board
- Sensor suhu DS18B20
- PZEM004Tv30
- 4 buah tombol (Up, Down, Set, Reset)
- Akses WiFi
- Akun Telegram & Google Account

---

## 🔧 Konfigurasi

### WiFi dan Telegram
Edit pada bagian awal sketch:
```cpp
const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";

#define BOTtoken "TOKEN_BOT_TELEGRAM"
#define CHAT_ID "CHAT_ID_TELEGRAM"
