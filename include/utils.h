// utils.h
#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 10800;  // UTC+3 for Turkey
const int   daylightOffset_sec = 0;

void initTime();
String getCurrentTime();

#endif
