#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include <time.h>
#include <ArduinoJson.h>
#include <Keypad.h>
#include <ESP32Servo.h>

#define WIFI_SSID    "Menes"
#define WIFI_PASS    "deneme123"
#define SERVER_IP    "172.20.10.3"
#define SERVER_PORT  "5000"
#define SERVER_URL   "http://" SERVER_IP ":" SERVER_PORT
#define UPLOAD_URL   SERVER_URL "/upload"
#define LOG_URL      SERVER_URL "/log_access"

#define SAMPLE_RATE     16000
#define SAMPLE_BITS     I2S_BITS_PER_SAMPLE_16BIT
#define CHANNEL_FORMAT  I2S_CHANNEL_FMT_ONLY_LEFT
#define RECORD_TIME_SEC 10
#define CHUNK_SIZE      4096
#define WAV_HEADER_SIZE 44

#define I2S0_BCK 14
#define I2S0_WS  13
#define I2S0_SD  15

#define DAC_BCK 38
#define DAC_WS  39
#define DAC_DIN 37

#define RECORD_BUTTON 4

#define WAKEWORD_TIME_SEC 3
#define WAKEWORD_PHRASE "uyan"

#define SERVO_PIN 18
#define SERVO_OPEN 90
#define SERVO_CLOSED 0

const String correctPassword = "1234";

#endif
