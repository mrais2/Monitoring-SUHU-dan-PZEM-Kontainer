#include <WiFi.h>
#include <HTTPClient.h>
#include <PZEM004Tv30.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <ezButton.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ====== WiFi credentials ======
const char* ssid = "4GMODEM";
const char* password = "admin12345";

// ====== Google Sheets via Apps Script ======
String GOOGLE_URL = "https://script.google.com/macros/s/AKfycbzMxUB4AQoa1QlWgWwQufkqYGFZGcGHl_0Z8waf35SDDOYBF_6mvaaV-rxLYZ6tV8o_mA/exec";

// ====== Telegram Config ======
// #define BOTtoken "7995568426:AAGuMs0EViR9_Fb0A8jRc6JVvzGUh7PpF6k" // Ganti dengan token bot
// #define CHAT_ID "1493469379"  // Ganti dengan chat ID kamu

#define BOTtoken "7256585030:AAFty87llI7NvPNlXQXnP1bNPCO2ib1esdk" // Ganti dengan token bot
#define CHAT_ID "6889402327"  // Ganti dengan chat ID kamu

WiFiClientSecure secureClient;
UniversalTelegramBot bot(BOTtoken, secureClient);

// ====== EEPROM Config ======
#define EEPROM_SIZE 64
#define ENERGY_ADDR 0
#define SETPOINT_ADDR 10

// ====== PZEM Setup ======
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// ====== DS18B20 Setup ======
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ====== Tombol Keypad Setup ======
#define KEY_NUM 4
#define PIN_KEY_UP     26
#define PIN_KEY_DOWN   27
#define PIN_KEY_SET    14
#define PIN_KEY_RESET  33
ezButton keypad_1x4[] = {
  ezButton(PIN_KEY_UP),
  ezButton(PIN_KEY_DOWN),
  ezButton(PIN_KEY_SET),
  ezButton(PIN_KEY_RESET)
};

// ====== Global Variables ======
float energy_total = 0.0;
float setpointTemp = -16.0;
bool overSetpointSent = false;
unsigned long lastSensorUpdate = 0;
unsigned long lastMillisEnergy = 0;
unsigned long lastTelegramUpdate = 0;
const unsigned long intervalUpdate = 30000; // 30 detik
const unsigned long intervalTelegram = 15000; // 15 detik

bool suhuNormalSent = false;
unsigned long lastAlarmMillis = 0;
const unsigned long intervalAlarm = 60000;  // 1 menit

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  for (byte i = 0; i < KEY_NUM; i++) keypad_1x4[i].setDebounceTime(100);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  secureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  bot.sendMessage(CHAT_ID, "🤖 Welcome Monitoring Container!", "");

  // Kirim tombol awal ke Telegram
  String keyboardJson = "[[\"/status\"], [\"/setpoint\"], [\"/reset\"]]";
  bot.sendMessageWithReplyKeyboard(CHAT_ID, "📋 Silahkan Pilih perintah tombol :", "", keyboardJson, true);

  sensors.begin();
  EEPROM.begin(EEPROM_SIZE);

  energy_total = EEPROM.readFloat(ENERGY_ADDR);
  setpointTemp = EEPROM.readFloat(SETPOINT_ADDR);
  if (isnan(setpointTemp)) setpointTemp = -16.0;

  lastMillisEnergy = millis();
}

int getKeyPressed() {
  for (byte i = 0; i < KEY_NUM; i++) keypad_1x4[i].loop();
  for (byte i = 0; i < KEY_NUM; i++) {
    if (keypad_1x4[i].isPressed()) return (i + 1);
  }
  return 0;
}

void kirimTelegram(String pesan) {
  bot.sendMessage(CHAT_ID, pesan, "Markdown");
}

void handleKeyAction(int key) {
  switch (key) {
  case 1:  // Tombol naik
    setpointTemp++;
    Serial.println("🔼 Set-point naik: " + String(setpointTemp));
    kirimTelegram("🔼 Set-point dinaikkan via keypad: *" + String(setpointTemp, 1) + "°C*");
    break;

  case 2:  // Tombol turun
    setpointTemp--;
    Serial.println("🔽 Set-point turun: " + String(setpointTemp));
    kirimTelegram("🔽 Set-point diturunkan via keypad: *" + String(setpointTemp, 1) + "°C*");
    break;


    case 3:
      EEPROM.writeFloat(SETPOINT_ADDR, setpointTemp);
      EEPROM.commit();
      Serial.println("✅ Set-point disimpan: " + String(setpointTemp));
      kirimTelegram("✅ Set-point suhu disimpan dari keypad: *" + String(setpointTemp, 1) + "°C*");
      break;

    case 4:
      energy_total = 0.0;
      EEPROM.writeFloat(ENERGY_ADDR, energy_total);
      EEPROM.commit();
      Serial.println("♻️ Energi telah direset ke 0.0 Wh");
      HTTPClient http;
      http.begin(GOOGLE_URL + "?reset=1");
      int code = http.GET();
      if (code > 0) Serial.println("📤 Reset GSheet: " + http.getString());
      else Serial.println("❌ Reset GSheet gagal, kode: " + String(code));
      http.end();
      kirimTelegram("♻️ *Data energi & Google Sheet telah direset!*\nEnergi: *0.0 Wh*");
      break;
  }
}

void handleNewTelegramMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;

if (text.startsWith("/setpoint")) {
  int spaceIndex = text.indexOf(' ');
  if (spaceIndex > 0 && spaceIndex < text.length() - 1) {
    String valueStr = text.substring(spaceIndex + 1);
    float val = valueStr.toFloat();
    setpointTemp = val;
    EEPROM.writeFloat(SETPOINT_ADDR, setpointTemp);
    EEPROM.commit();
    kirimTelegram("✅ Set-point suhu disimpan: *" + String(setpointTemp, 1) + "°C*");
    } else {
    kirimTelegram("📥 Format salah.\nKirim seperti ini: `/setpoint -18.5` (pakai spasi setelah perintah)");
    }
    } else if (text == "/status") {
      sensors.requestTemperatures();
      String statusMsg = "📊 *Status Sensor:*\n";
      statusMsg += "🌡️ Suhu: " + String(sensors.getTempCByIndex(0), 1) + "°C\n";
      statusMsg += "🔌 Volt: " + String(pzem.voltage(), 2) + " V\n";
      statusMsg += "⚡ Arus: " + String(pzem.current(), 2) + " A\n";
      statusMsg += "⚙️ Daya: " + String(pzem.power(), 2) + " W\n";
      statusMsg += "⏲️ Energi: " + String(energy_total, 3) + " Wh\n";
      statusMsg += "🎯 Setpoint: " + String(setpointTemp, 1) + "°C";
      kirimTelegram(statusMsg);
    } else if (text == "/reset") {
      energy_total = 0.0;
      EEPROM.writeFloat(ENERGY_ADDR, energy_total);
      EEPROM.commit();
      kirimTelegram("♻️ *Energi berhasil direset ke 0.0 Wh*");
    } else {
      kirimTelegram("📘 Perintah tidak dikenal.\nGunakan:\n/status - Lihat data sensor\n/setpoint - Pilih suhu target\n/reset - Reset energi");
    }
  }
}

void updateSensorAndSend() {
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float frequency = pzem.frequency();
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(frequency) || tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("⚠️ Sensor error!");
    return;
  }

  float now = millis();
  float energy_now = power * ((now - lastMillisEnergy) / 3600000.0);
  energy_total += energy_now;
  lastMillisEnergy = now;

  static unsigned long lastEEPROM = 0;
      if (now - lastEEPROM > 60000) {
       EEPROM.writeFloat(ENERGY_ADDR, energy_total);
      EEPROM.commit();
      lastEEPROM = now;
      }
      static bool suhuNormalSent = false;

      if (tempC > (setpointTemp + 2) || tempC < (setpointTemp - 2)) {
        if (millis() - lastAlarmMillis >= intervalAlarm) {
        kirimTelegram("🚨 *Peringatan!* Suhu keluar batas toleransi ±2°C:\n🌡️ Suhu: " + String(tempC, 1) + "°C\n🎯 Setpoint: " + String(setpointTemp, 1) + "°C");
        lastAlarmMillis = millis();
      }
        suhuNormalSent = false;  // Reset agar bisa kirim notif suhu normal nanti
      }

      if (!suhuNormalSent && (tempC >= (setpointTemp - 1) && tempC <= (setpointTemp + 1))) {
      kirimTelegram("✅ *Suhu telah kembali normal*\n🌡️ Suhu: " + String(tempC, 1) + "°C\n🎯 Setpoint: " + String(setpointTemp, 1) + "°C");
      suhuNormalSent = true;
      }


  HTTPClient http;
  String urlFinal = GOOGLE_URL + "?voltage=" + String(voltage, 2) +
                    "&current=" + String(current, 2) +
                    "&power=" + String(power, 2) +
                    "&energy=" + String(energy_total, 3) +
                    "&frequency=" + String(frequency, 1) +
                    "&suhu=" + String(tempC, 1);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(urlFinal);
  int httpCode = http.GET();
  if (httpCode > 0) Serial.println("✅ Google Sheet: " + http.getString());
  else Serial.println("❌ GSheet HTTP error: " + String(httpCode));
  http.end();

  Serial.println("📌 Set-point saat ini: " + String(setpointTemp) + "°C");
}

void loop() {
  int key = getKeyPressed();
  if (key) handleKeyAction(key);

  if (millis() - lastSensorUpdate >= intervalUpdate) {
    lastSensorUpdate = millis();
    updateSensorAndSend();
  }

  if (millis() - lastTelegramUpdate >= intervalTelegram) {
    lastTelegramUpdate = millis();
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewTelegramMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  }
}
