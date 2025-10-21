// wifi_manager.cpp
#include "wifi_manager.h"


void wifi_connect() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi bağlandı: " + WiFi.localIP().toString());
}

bool check_server_connection() {
  WiFiClient client;
  HTTPClient http;
  
  Serial.println("🔍 Sunucu bağlantısı kontrol ediliyor...");
  Serial.println("URL: " SERVER_URL);
  
  if (!http.begin(client, SERVER_URL)) {
    Serial.println("❌ HTTP bağlantısı başlatılamadı!");
    return false;
  }
  
  int httpCode = http.GET();
  http.end();
  
  if (httpCode > 0) {
    Serial.printf("✅ Sunucu yanıt verdi (HTTP %d)\n", httpCode);
    return true;
  } else {
    Serial.printf("❌ Sunucuya erişilemiyor: %s\n", http.errorToString(httpCode).c_str());
    return false;
  }
}
