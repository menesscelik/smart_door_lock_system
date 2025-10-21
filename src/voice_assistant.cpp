#include "voice_assistant.h"
#include "audio_handler.h"

void handleVoiceAssistant() {
  // Önce wake word kontrolü yap
  if (!checkWakeWord()) {
    Serial.println("Wake word algılanamadı, sistem başlatılamıyor.");
    return;
  }
  
  Serial.println("Wake word doğrulandı, sistem başlatılıyor...");
  delay(500);
  
  if (!check_server_connection()) {
    Serial.println("Sunucu bağlantısı kurulamadı! İşlem iptal ediliyor.");
    return;
  }
  
  i2s_driver_uninstall(I2S_NUM_0);
  delay(100);
  
  i2s_record_init();
  Serial.println("Ses algılama başlatıldı...");
  
  String session_id = String(random(0xFFFFFFFF), HEX);
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi bağlantısı kesildi! Yeniden bağlanılıyor...");
    wifi_connect();
  }
  
  WiFiClient client;
  client.setTimeout(120000);
  
  HTTPClient http;
  http.begin(client, UPLOAD_URL);
  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("X-Session-ID", session_id);
  http.setTimeout(120000);
  
  size_t total_data_size = SAMPLE_RATE * RECORD_TIME_SEC * 2;
  create_wav_header(chunk_buffer, total_data_size, SAMPLE_RATE);
  
  size_t total_bytes = 0;
  unsigned long start_time = millis();
  bool first_chunk = true;
  String url = "";
  int chunk_count = 0;
  int http_errors = 0;
  
  while ((millis() - start_time) < RECORD_TIME_SEC * 1000) {
    size_t bytes_read = 0;
    
    esp_err_t result = i2s_read(I2S_NUM_0, 
                               first_chunk ? chunk_buffer + WAV_HEADER_SIZE : chunk_buffer, 
                               CHUNK_SIZE, 
                               &bytes_read, 
                               portMAX_DELAY);
    
    if (result == ESP_OK && bytes_read > 0) {
      http.addHeader("X-First-Chunk", first_chunk ? "true" : "false");
      http.addHeader("X-Last-Chunk", (millis() - start_time) >= (RECORD_TIME_SEC * 1000 - 100) ? "true" : "false");
      
      int httpCode;
      if (first_chunk) {
        httpCode = http.POST(chunk_buffer, bytes_read + WAV_HEADER_SIZE);
        first_chunk = false;
      } else {
        httpCode = http.POST(chunk_buffer, bytes_read);
      }
      
      if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        if (response.length() > 10) {
          url = response;
          Serial.println("Sunucu yanıtı alındı: " + url);
        }
        chunk_count++;
        total_bytes += bytes_read;
        
        if ((millis() - start_time) % 2000 < 100) {
          Serial.printf("Ses algılama devam ediyor: %d saniye, %u byte\n", 
                       (millis() - start_time) / 1000,
                       total_bytes);
        }
      } else {
        http_errors++;
        Serial.printf("Bağlantı hatası (parça %d): %d\n", 
                     chunk_count, 
                     httpCode);
        
        if (http_errors > 5) {
          Serial.println("Çok fazla hata oluştu, işlem durduruluyor!");
          break;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("WiFi bağlantısı kesildi! Yeniden bağlanılıyor...");
          wifi_connect();
          http.end();
          http.begin(client, UPLOAD_URL);
          http.addHeader("Content-Type", "audio/wav");
          http.addHeader("X-Session-ID", session_id);
          http.setTimeout(120000);
        }
      }
    }
  }
  
  i2s_stop(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_0);
  
  Serial.printf("Ses algılama tamamlandı. Toplam %u byte (%d parça)\n", 
                total_bytes + WAV_HEADER_SIZE,
                chunk_count);
  
  if (http_errors > 0) {
    Serial.printf("Toplam %d bağlantı hatası oluştu\n", http_errors);
  }
  
  http.end();
  
  if (url.length() > 0) {
    Serial.println("Ses yanıtı çalınıyor: " + url);
    play_wav_from_url(url);
  } else {
    Serial.println("Sunucu yanıt vermedi");
  }
}

bool checkWakeWord() {
  Serial.println("\nSes algılama bekleniyor...");
  
  if (!check_server_connection()) {
    Serial.println("Sunucu bağlantısı kurulamadı!");
    return false;
  }
  
  i2s_driver_uninstall(I2S_NUM_0);
  delay(100);
  i2s_record_init();
  
  bool wake_word_detected = false;
  int attempt = 1;
  
  while (!wake_word_detected) {
    Serial.printf("\nDinleme denemesi #%d\n", attempt++);
    
    String session_id = String(random(0xFFFFFFFF), HEX);
    
    WiFiClient client;
    client.setTimeout(120000);
    
    HTTPClient http;
    http.begin(client, UPLOAD_URL);
    http.addHeader("Content-Type", "audio/wav");
    http.addHeader("X-Session-ID", session_id);
    http.addHeader("X-Wake-Check", "true");
    http.setTimeout(120000);
    
    size_t total_data_size = SAMPLE_RATE * WAKEWORD_TIME_SEC * 2;
    create_wav_header(chunk_buffer, total_data_size, SAMPLE_RATE);
    
    size_t total_bytes = 0;
    unsigned long start_time = millis();
    bool first_chunk = true;
    String transcription = "";
    int chunk_count = 0;
    
    Serial.println("Ses algılanıyor...");
    
    while ((millis() - start_time) < WAKEWORD_TIME_SEC * 1000) {
      size_t bytes_read = 0;
      
      esp_err_t result = i2s_read(I2S_NUM_0, 
                                 first_chunk ? chunk_buffer + WAV_HEADER_SIZE : chunk_buffer, 
                                 CHUNK_SIZE, 
                                 &bytes_read, 
                                 portMAX_DELAY);
      
      if (result == ESP_OK && bytes_read > 0) {
        http.addHeader("X-First-Chunk", first_chunk ? "true" : "false");
        http.addHeader("X-Last-Chunk", (millis() - start_time) >= (WAKEWORD_TIME_SEC * 1000 - 100) ? "true" : "false");
        
        int httpCode;
        if (first_chunk) {
          httpCode = http.POST(chunk_buffer, bytes_read + WAV_HEADER_SIZE);
          first_chunk = false;
        } else {
          httpCode = http.POST(chunk_buffer, bytes_read);
        }
        
        if (httpCode == HTTP_CODE_OK) {
          String response = http.getString();
          if (response.length() > 0 && !response.startsWith("http")) {
            transcription = response;
          }
          chunk_count++;
          total_bytes += bytes_read;
        } else {
          Serial.printf("Bağlantı hatası: %d\n", httpCode);
        }
      }
    }
    
    i2s_stop(I2S_NUM_0);
    i2s_start(I2S_NUM_0);
    
    http.end();
    
    Serial.printf("Ses algılama tamamlandı. Toplam %u byte (%d parça)\n", 
                  total_bytes + WAV_HEADER_SIZE,
                  chunk_count);
    
    transcription.toLowerCase();
    transcription.trim();
    
    Serial.printf("Algılanan ses: '%s'\n", transcription.c_str());
    Serial.printf("Beklenen komut: '%s'\n", WAKEWORD_PHRASE);
    
    if (transcription.indexOf(WAKEWORD_PHRASE) != -1) {
      Serial.println("Komut algılandı!");
      wake_word_detected = true;
    } else {
      Serial.println("Komut algılanamadı, tekrar deneniyor...");
      delay(100);
    }
  }
  
  i2s_driver_uninstall(I2S_NUM_0);
  return true;
}

String getNameByVoice() {
  Serial.println("Ses kaydı başlatılıyor...");
  i2s_driver_uninstall(I2S_NUM_0);
  delay(100);
  i2s_record_init();
  Serial.println("İsim için ses algılanıyor...");
  String session_id = String(random(0xFFFFFFFF), HEX);
  WiFiClient client;
  client.setTimeout(120000);
  HTTPClient http;
  http.begin(client, UPLOAD_URL);
  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("X-Session-ID", session_id);
  http.addHeader("X-Wake-Check", "true");
  http.setTimeout(120000);
  size_t total_data_size = SAMPLE_RATE * 3 * 2;
  create_wav_header(chunk_buffer, total_data_size, SAMPLE_RATE);
  size_t total_bytes = 0;
  unsigned long start_time = millis();
  bool first_chunk = true;
  String transcription = "";
  int chunk_count = 0;
  while ((millis() - start_time) < 3000) {
    size_t bytes_read = 0;
    esp_err_t result = i2s_read(I2S_NUM_0, first_chunk ? chunk_buffer + WAV_HEADER_SIZE : chunk_buffer, CHUNK_SIZE, &bytes_read, portMAX_DELAY);
    if (result == ESP_OK && bytes_read > 0) {
      http.addHeader("X-First-Chunk", first_chunk ? "true" : "false");
      http.addHeader("X-Last-Chunk", (millis() - start_time) >= (3000 - 100) ? "true" : "false");
      int httpCode;
      if (first_chunk) {
        httpCode = http.POST(chunk_buffer, bytes_read + WAV_HEADER_SIZE);
        first_chunk = false;
      } else {
        httpCode = http.POST(chunk_buffer, bytes_read);
      }
      if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        if (response.length() > 0 && !response.startsWith("http")) {
          transcription = response;
        }
        chunk_count++;
        total_bytes += bytes_read;
      }
    }
  }
  i2s_stop(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_0);
  transcription.trim();
  Serial.print("Algılanan isim: "); Serial.println(transcription);
  return transcription;
}

String getCommandByVoice() {
  Serial.println("Komut için ses kaydı başlatılıyor...");
  i2s_driver_uninstall(I2S_NUM_0);
  delay(100);
  i2s_record_init();
  Serial.println("Komut için ses algılanıyor...");
  String session_id = String(random(0xFFFFFFFF), HEX);
  WiFiClient client;
  client.setTimeout(120000);
  HTTPClient http;
  http.begin(client, UPLOAD_URL);
  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("X-Session-ID", session_id);
  http.addHeader("X-Wake-Check", "true");
  http.setTimeout(120000);
  size_t total_data_size = SAMPLE_RATE * 3 * 2;
  create_wav_header(chunk_buffer, total_data_size, SAMPLE_RATE);
  size_t total_bytes = 0;
  unsigned long start_time = millis();
  bool first_chunk = true;
  String transcription = "";
  int chunk_count = 0;
  while ((millis() - start_time) < 3000) {
    size_t bytes_read = 0;
    esp_err_t result = i2s_read(I2S_NUM_0, first_chunk ? chunk_buffer + WAV_HEADER_SIZE : chunk_buffer, CHUNK_SIZE, &bytes_read, portMAX_DELAY);
    if (result == ESP_OK && bytes_read > 0) {
      http.addHeader("X-First-Chunk", first_chunk ? "true" : "false");
      http.addHeader("X-Last-Chunk", (millis() - start_time) >= (3000 - 100) ? "true" : "false");
      int httpCode;
      if (first_chunk) {
        httpCode = http.POST(chunk_buffer, bytes_read + WAV_HEADER_SIZE);
        first_chunk = false;
      } else {
        httpCode = http.POST(chunk_buffer, bytes_read);
      }
      if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        if (response.length() > 0 && !response.startsWith("http")) {
          transcription = response;
        }
        chunk_count++;
        total_bytes += bytes_read;
      }
    }
  }
  i2s_stop(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_0);
  transcription.trim();
  Serial.print("Algılanan komut: "); Serial.println(transcription);
  return transcription;
}