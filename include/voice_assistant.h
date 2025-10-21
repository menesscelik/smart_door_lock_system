// voice_assistant.h
#ifndef VOICE_ASSISTANT_H
#define VOICE_ASSISTANT_H

#include "wifi_manager.h"
//#include "config.h"

void handleVoiceAssistant();
bool checkWakeWord();
String getNameByVoice();
String getCommandByVoice();

#endif
