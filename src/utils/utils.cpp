#include "utils.h"
#include "config/Configuration.h"

void all_esp_reset(void)
{
#if defined(ESP32)
  WiFi.disconnect(false, false);
#elif defined(ESP8266)
  WiFi.disconnect(false);
#endif
  delay(1000);
  ESP.restart();
}

void stackWD(void)
{
  uint32_t freeHeap = ESP.getFreeHeap();
#if defined(ESP32)
  uint32_t maxBlock = ESP.getMaxAllocHeap();
#elif defined(ESP8266)
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
#endif
  DEBUG_SERIAL.print(F("Free heap: "));
  DEBUG_SERIAL.print(freeHeap);
  DEBUG_SERIAL.print(F(" bytes, Max free block: "));
  DEBUG_SERIAL.print(maxBlock);
  DEBUG_SERIAL.println(F(" bytes"));

  if (maxBlock < MIN_HEAP_SIZE)
  {
    DEBUG_SERIAL.println(F("Low memory detected, restarting..."));
    all_esp_reset();
  }
}

uint64_t extendedMillis()
{
  static unsigned long lastMillis = 0;
  static uint64_t overflows = 0;

  unsigned long currentMillis = millis();

  // Überlauf erkennen
  if (currentMillis < lastMillis)
  {
    overflows++;
  }
  lastMillis = currentMillis;

  // combine overflows and currentMillis to get the extended time in milliseconds
  return (overflows << 32) + currentMillis;
}

void wifi_status_print(void)
{
  DEBUG_SERIAL.print(F("Wifi, RSSI: "));
  DEBUG_SERIAL.print(WiFi.RSSI());
  DEBUG_SERIAL.print(F(" dBm, Access Point MAC: "));
  uint8_t *bssid = WiFi.BSSID();
  if (bssid)
  {
    DEBUG_SERIAL.printf("%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
  }
  DEBUG_SERIAL.println();
  uint64_t longMillis = extendedMillis();

  uint64_t ontime_minutes = longMillis / 60000ll;
  uint64_t ontime_hours = ontime_minutes / 60ll;

  // separate days, hours, and minutes for better readability
  uint32_t days = (uint32_t)(ontime_hours / 24ll);
  uint32_t houres = (uint32_t)(ontime_hours % 24ll);
  uint32_t minutes = (uint32_t)(ontime_minutes % 60ll);

  DEBUG_SERIAL.print(F("Ontime: "));
  DEBUG_SERIAL.print(days);
  DEBUG_SERIAL.print(F(" days, "));
  DEBUG_SERIAL.printf("%02d:%02d", houres, minutes);
  DEBUG_SERIAL.println(F(" h"));
}
