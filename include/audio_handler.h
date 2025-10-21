// audio_handler.h
#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

//#include "AudioOutputI2S.h"
//#include <Arduino.h>
//#include "driver/i2s.h"
#include "config.h"
//#include <WiFi.h>
//#include <HTTPClient.h>
//#include "AudioFileSourceHTTPStream.h"

// Voice Assistant Variables
uint8_t chunk_buffer[CHUNK_SIZE + WAV_HEADER_SIZE];  // Chunk + WAV header için buffer
AudioFileSourceHTTPStream *file;
AudioOutputI2S *out;
AudioGeneratorWAV *wav;

void i2s_record_init();
void i2s_play_init();
void create_wav_header(uint8_t* h, size_t pcm_size, int sr);
String send_audio_to_server(uint8_t* data, size_t len);
void play_wav_from_url(const String& url);

#endif
