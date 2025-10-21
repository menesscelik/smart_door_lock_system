#include "utils.h"

void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("✅ Zaman sunucusu ayarlandı");
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time Error";
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}