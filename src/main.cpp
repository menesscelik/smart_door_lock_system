#include <Arduino.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include "config.h"
#include "wifi_manager.h"
#include "voice_assistant.h"
#include "user_auth.h"
#include "utils.h"
#include "audio_handler.h"
Servo doorServo;

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {4, 5, 6, 7}; 
byte colPins[COLS] = {8, 9, 10, 11}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(115200);
  
  // Kayıt butonu için pin ayarı
  pinMode(RECORD_BUTTON, INPUT_PULLUP);
  
  wifi_connect();
  
  // Initialize components
  initTime();
  
  // Servo başlat
  doorServo.attach(SERVO_PIN);
  doorServo.write(SERVO_CLOSED);
  
  Serial.println("\n=== Sistem Hazır ===");
}

void loop() {
  Serial.println("\n=== Ana Menü ===");
  Serial.println("[A] Sesli Asistan");
  Serial.println("[B] Giriş İşlemleri");
  Serial.println("Seçiminizi yapın (A/B):");
  char choice = 0;
  while (true) {
    char key = keypad.getKey();
    if (key == 'A' || key == 'B') {
      choice = key;
      break;
    }
    delay(50);
  }
  switch (choice) {
    case 'A':
      handleVoiceAssistant();
      break;
    case 'B':
      while (true) {
        Serial.println("\n=== Giriş İşlemleri (sesli komut ile) ===");
        Serial.println("Lütfen yapmak istediğiniz işlemi sesli olarak söyleyin: 'yeni kullanıcı kaydı' veya 'ana menüye dön'");
        String command = getCommandByVoice();
        command.toLowerCase();
        command.trim();
        Serial.print("Algılanan komut: "); Serial.println(command);
        if (command.indexOf("yeni kullanıcı") != -1) {
          // Yeni kullanıcı kaydı akışı
          Serial.println("\nLütfen isminizi sesli olarak söyleyin ve kaydı başlatmak için butona basın...");
          String name = getNameByVoice();
          Serial.print("Algılanan isim: "); Serial.println(name);
          Serial.println("Şimdi 4 haneli şifrenizi girin (bitirmek için #):");
          int passwordAttempts = 0;  // Şifre deneme sayacı
          
          while (passwordAttempts < 3) {  // 3 deneme hakkı kontrolü
            String password = "";
            while (true) {
              char key = keypad.getKey();
              if (key) {
                if (key == '#') break;
                if (key >= '0' && key <= '9' && password.length() < 4) {
                  password += key;
                  Serial.print("*");
                }
              }
              delay(50);
            }
            Serial.println();
            
            if (password.length() != 4) {
              Serial.println("Hatalı giriş! 4 haneli şifre zorunlu.");
              continue;
            }
            
            // Sunucuya gönder
            WiFiClient client;
            HTTPClient http;
            http.begin(client, String("http://") + SERVER_IP + ":" + SERVER_PORT + "/register_user");
            http.addHeader("Content-Type", "application/json");
            String json = String("{\"name\":\"") + name + "\",\"password\":\"" + password + "\"}";
            int httpCode = http.POST(json);
            if (httpCode == HTTP_CODE_OK) {
              String response = http.getString();
              Serial.println("\nKayıt başarılı: " + response);
            } else {
              Serial.println("\nKayıt başarısız! HTTP kodu: " + String(httpCode));
            }
            http.end();
            break; // Kayıt sonrası ana menüye dön
          }
          break; // Kayıt sonrası ana menüye dön
        } else if (command.indexOf("ana menü") != -1) {
          break; // Ana menüye dön
        } else if (command.indexOf("giriş yap") != -1) {
          Serial.println("Lütfen isminizi sesli olarak söyleyin ve kaydı başlatmak için butona basın...");
          String name = getNameByVoice();
          name.trim();
          if (name.length() == 0) {
            Serial.println("İsim algılanamadı. Ana menüye dönülüyor.");
            break;
          }
          // Sunucuda bu isim var mı kontrol et
          WiFiClient client;
          HTTPClient http;
          http.begin(client, String("http://") + SERVER_IP + ":" + SERVER_PORT + "/check_user");
          http.addHeader("Content-Type", "application/json");
          String checkJson = String("{\"name\":\"") + name + "\"}";
          int checkCode = http.POST(checkJson);
          if (checkCode == HTTP_CODE_OK) {
            String checkResp = http.getString();
            if (checkResp == "OK") {
              Serial.println("4 haneli şifrenizi girin (bitirmek için #):");
              int passwordAttempts = 0;  // Şifre deneme sayacı
              
              while (passwordAttempts < 3) {  // 3 deneme hakkı kontrolü
                String password = "";
                while (true) {
                  char key = keypad.getKey();
                  if (key) {
                    if (key == '#') break;
                    if (key >= '0' && key <= '9' && password.length() < 4) {
                      password += key;
                      Serial.print("*");
                    }
                  }
                  delay(50);
                }
                Serial.println();
                
                if (password.length() != 4) {
                  Serial.println("Hatalı giriş! 4 haneli şifre zorunlu.");
                  continue;
                }
                
                // Şifreyi ve ismi sunucuya gönder
                WiFiClient loginClient;
                HTTPClient loginHttp;
                loginHttp.begin(loginClient, String("http://") + SERVER_IP + ":" + SERVER_PORT + "/verify_user");
                loginHttp.addHeader("Content-Type", "application/json");
                String loginJson = String("{\"name\":\"") + name + "\",\"password\":\"" + password + "\"}";
                int loginCode = loginHttp.POST(loginJson);
                
                // HTTP yanıtını al
                String response = loginHttp.getString();
                DynamicJsonDocument doc(256);
                DeserializationError error = deserializeJson(doc, response);
                
                // Başarılı giriş veya şifre hatası durumlarını kontrol et
                if (loginCode == HTTP_CODE_OK && !error && doc["status"] == "success") {
                  Serial.println("\nGiriş başarılı! Kapı açılıyor...");
                  doorServo.write(180); // 180 derece döndür
                  delay(2000);
                  doorServo.write(SERVO_CLOSED);
                  Serial.println("Kapı kapandı.");
                  // Welcome mesajı
                  String welcomeText = "Hoş geldiniz " + name;
                  WiFiClient ttsClient;
                  HTTPClient ttsHttp;
                  ttsHttp.begin(ttsClient, String("http://") + SERVER_IP + ":" + SERVER_PORT + "/upload");
                  ttsHttp.addHeader("Content-Type", "application/json");
                  String ttsJson = String("{\"text\":\"") + welcomeText + "\",\"lang\":\"tr\"}";
                  int ttsCode = ttsHttp.POST(ttsJson);
                  if (ttsCode == HTTP_CODE_OK) {
                    String ttsUrl = ttsHttp.getString();
                    if (ttsUrl.startsWith("http")) {
                      play_wav_from_url(ttsUrl);
                    }
                  }
                  ttsHttp.end();
                  loginHttp.end();
                  break; // Başarılı girişte döngüden çık
                } else {
                  // Şifre yanlış veya HTTP hatası durumu
                  passwordAttempts++;
                  
                  // Yanlış şifre sesli uyarısı
                  WiFiClient wrongPassClient;
                  HTTPClient wrongPassHttp;
                  wrongPassHttp.begin(wrongPassClient, String("http://") + SERVER_IP + ":" + SERVER_PORT + "/upload");
                  wrongPassHttp.addHeader("Content-Type", "application/json");
                  
                  String wrongPassText;
                  if (passwordAttempts >= 3) {
                    wrongPassText = "Hakkınız kalmadı. Ana menüye dönülüyor.";
                    String wrongPassJson = String("{\"text\":\"") + wrongPassText + "\",\"lang\":\"tr\"}";
                    int wrongPassCode = wrongPassHttp.POST(wrongPassJson);
                    if (wrongPassCode == HTTP_CODE_OK) {
                      String wrongPassUrl = wrongPassHttp.getString();
                      if (wrongPassUrl.startsWith("http")) {
                        play_wav_from_url(wrongPassUrl);
                      }
                    }
                    wrongPassHttp.end();
                    Serial.println("\nGiriş hakkınız kalmadı! Ana menüye dönülüyor.");
                    loginHttp.end();
                    return; // Ana menüye dön
                  } else {
                    wrongPassText = "Şifre yanlış. Kalan hakkınız: " + String(3 - passwordAttempts);
                    String wrongPassJson = String("{\"text\":\"") + wrongPassText + "\",\"lang\":\"tr\"}";
                    int wrongPassCode = wrongPassHttp.POST(wrongPassJson);
                    if (wrongPassCode == HTTP_CODE_OK) {
                      String wrongPassUrl = wrongPassHttp.getString();
                      if (wrongPassUrl.startsWith("http")) {
                        play_wav_from_url(wrongPassUrl);
                      }
                    }
                    wrongPassHttp.end();
                    Serial.println("\nGiriş başarısız! Şifre yanlış. Kalan hak: " + String(3 - passwordAttempts));
                    Serial.println("4 haneli şifrenizi tekrar girin (bitirmek için #):");
                  }
                }
                loginHttp.end();
              }
            } else {
              Serial.println("Böyle bir kullanıcı bulunamadı. Ana menüye dönülüyor.");
            }
          } else {
            Serial.println("Sunucuya erişilemedi. Ana menüye dönülüyor.");
          }
          http.end();
          break;
        } else if (command.indexOf("en son kim girmiş") != -1) {
  // Sunucudan en son giriş yapanı al
        WiFiClient client;
        HTTPClient http;
        http.begin(client, String("http://") + SERVER_IP + ":" + SERVER_PORT + "/last_login");
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
          String response = http.getString();
    
          DynamicJsonDocument doc(256);
          DeserializationError err = deserializeJson(doc, response);

        if (!err && doc.containsKey("name")) {
          String name = doc["name"].as<String>();
          String url = doc["url"].as<String>();

          // Konsola sade çıktı
          Serial.println("Son giriş yapan kişi: " + name);

          // Sesli olarak oynat
            if (url.startsWith("http")) {
            play_wav_from_url(url);
      }
        } else {
          Serial.println("Sunucudan geçerli veri alınamadı.");
          Serial.println("Yanıt içeriği: " + response);
    }
        } else {
    Serial.println("Sunucudan bilgi alınamadı. HTTP kodu: " + String(httpCode));
  }
  http.end();
  break;
}

      }
      break;
    default:
      Serial.println("Geçersiz seçim!");
      break;
  }
}
