#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

void all_esp_reset(void);
void stackWD(void);
uint64_t extendedMillis();
void wifi_status_print(void);

// to be tuned for ESP32 and ESP8266
#if defined(ESP32)
#define MIN_HEAP_SIZE 12000
#elif defined(ESP8266)
#define MIN_HEAP_SIZE 5000
#endif


#endif // UTILS_H