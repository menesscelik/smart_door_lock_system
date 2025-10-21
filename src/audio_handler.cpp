#include "audio_handler.h"


void i2s_record_init() {
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_config_t cfg = {
    .mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate       = SAMPLE_RATE,
    .bits_per_sample   = SAMPLE_BITS,
    .channel_format    = CHANNEL_FORMAT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .dma_buf_count     = 4,
    .dma_buf_len       = 1024
  };
  i2s_pin_config_t pins = {
    .bck_io_num    = I2S0_BCK,
    .ws_io_num     = I2S0_WS,
    .data_out_num  = -1,
    .data_in_num   = I2S0_SD
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void i2s_play_init() {
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = SAMPLE_BITS,
    .channel_format = CHANNEL_FORMAT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = true
  };
  i2s_pin_config_t pins = {
    .bck_io_num = DAC_BCK,
    .ws_io_num = DAC_WS,
    .data_out_num = DAC_DIN,
    .data_in_num = -1
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, SAMPLE_BITS, I2S_CHANNEL_MONO);
}

void create_wav_header(uint8_t* h, size_t pcm_size, int sr) {
  int byte_rate   = sr * 2;
  int block_align = 2;
  memcpy(h, "RIFF", 4);
  uint32_t cs = pcm_size + 36;
  memcpy(h + 4, &cs, 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  uint32_t sub1 = 16;
  memcpy(h + 16, &sub1, 4);
  h[20] = 1; h[21] = 0;
  h[22] = 1; h[23] = 0;
  memcpy(h + 24, &sr, 4);
  memcpy(h + 28, &byte_rate, 4);
  h[32] = block_align; h[33] = 0;
  h[34] = 16; h[35] = 0;
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &pcm_size, 4);
}

String send_audio_to_server(uint8_t* data, size_t len) {
  // ➊ WiFiClient'ın timeout'unu 120s yap
  WiFiClient client;
  client.setTimeout(120000);

  HTTPClient http;
  http.begin(client, UPLOAD_URL);
  // ➋ HTTPClient'in de kendi timeout'unu 120s yap (sürümünüz destekliyorsa)
  http.setTimeout(120000);

  http.addHeader("Content-Type", "audio/wav");
  int code = http.sendRequest("POST", data, len);

  String resp = "";
  if (code == HTTP_CODE_OK) {
    resp = http.getString();
    Serial.printf("📨 Sunucudan gelen URL (%d bytes): %s\n", resp.length(), resp.c_str());
  } else {
    Serial.printf("🚫 HTTP hatası: %d %s\n",
                  code,
                  http.errorToString(code).c_str());
  }

  http.end();
  return resp;
}

void play_wav_from_url(const String &url) {
  Serial.println("▶️ Playback başlıyor…");

  // ➊ no manual i2s_play_init();
  file = new AudioFileSourceHTTPStream(url.c_str());
  out  = new AudioOutputI2S();  
  out->SetPinout(DAC_BCK, DAC_WS, DAC_DIN);  
  out->SetGain(2.0);                // try a higher gain  
  wav = new AudioGeneratorWAV();

  if (!wav->begin(file, out)) {
    Serial.println("❌ wav.begin() başarısız!");
    return;
  }
  // optional: block until done
  while (wav->isRunning()) {
    wav->loop();
  }
}