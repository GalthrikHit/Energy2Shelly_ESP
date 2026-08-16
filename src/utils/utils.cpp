#include "utils.h"
#include "config/Configuration.h"

// Survives deep sleep and software resets
#if defined(ESP32)
RTC_DATA_ATTR Energ2Shelly_ResetReason rtc_reset_reason;
#elif defined(ESP8266)
Energ2Shelly_ResetReason rtc_reset_reason;
#endif



// return true if the reset reason is a power-on reset, false otherwise
bool isPowerOnReset() {
#if defined(ESP32)
  esp_reset_reason_t reason = esp_reset_reason();
  return (reason == ESP_RST_POWERON || reason == ESP_RST_EXT);

#elif defined(ESP8266)
  rst_info *resetInfo = ESP.getResetInfoPtr();
  return (resetInfo->reason == REASON_DEFAULT_RST || resetInfo->reason == REASON_EXT_SYS_RST);

#else
  return false; // Unsupported platform
#endif
}



void clear_rtc_power_on(void)
{
  if (isPowerOnReset())
  {
    rtc_reset_reason = Energ2Shelly_ResetReason::POWER_ON;
#if defined(ESP8266)
    ESP.rtcUserMemoryWrite(0,  (uint32_t*)&rtc_reset_reason, sizeof(rtc_reset_reason));
#endif
  }
  else
  {
#if defined(ESP8266)
    ESP.rtcUserMemoryRead(0,  (uint32_t*)&rtc_reset_reason, sizeof(rtc_reset_reason));
#endif
  }
}

void all_esp_reset(Energ2Shelly_ResetReason reason)
{
  rtc_reset_reason = reason;
#if defined(ESP32)
  WiFi.disconnect(false, false);
#elif defined(ESP8266)
  ESP.rtcUserMemoryWrite(0,  (uint32_t*)&rtc_reset_reason, sizeof(rtc_reset_reason));
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
    all_esp_reset(Energ2Shelly_ResetReason::LOW_MEMORY);
  }
}

uint64_t extendedMillis()
{
  static unsigned long lastMillis = 0;
  static uint64_t overflows = 0;

  unsigned long currentMillis = millis();

  // detect overflow of millis() and increment the overflow counter
  if (currentMillis < lastMillis)
  {
    overflows++;
  }
  lastMillis = currentMillis;

  // combine overflows and currentMillis to get the extended time in milliseconds
  return (overflows << 32) + currentMillis;
}

void status_print(void)
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

  DEBUG_SERIAL.print(F("Reset reason: "));
  switch (rtc_reset_reason)
  {
  case Energ2Shelly_ResetReason::POWER_ON:
    DEBUG_SERIAL.print(F("Power-on reset"));
    break;
  case Energ2Shelly_ResetReason::LOW_MEMORY:
    DEBUG_SERIAL.print(F("Low memory reset"));
    break;
  case Energ2Shelly_ResetReason::WIFI_DISCONNECT:
    DEBUG_SERIAL.print(F("WiFi disconnect reset"));
    break;
  case Energ2Shelly_ResetReason::MANUAL_RESET:
    DEBUG_SERIAL.print(F("Manual reset"));
    break;
  case Energ2Shelly_ResetReason::OTHER:
    DEBUG_SERIAL.print(F("Other reset"));
    break;
  default:
    DEBUG_SERIAL.print(F("Unknown reset reason"));
    break; 
  }
  DEBUG_SERIAL.print(F(", reconnect attempts: "));
  DEBUG_SERIAL.println(wifi_reconnect_attempts);


}
