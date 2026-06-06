#include "../config/Configuration.h"


 
  #if defined(ETH_H) || defined(ETH_class) // detect ETH libs
  #define USED_NETWORK ETH
  #else
  #define USED_NETWORK WiFi
  #endif
 
 
 /* internal stuff */
 #define MIN_BATTERY_CONTROL_LOOP_TIME min_battery_control_loop_time
 #define NUMBER_OF_MARSTEK_BATTERIES_MAX 10 
 #define NUMBER_OF_MARSTEK_BATTERIES number_of_marstek_batteries 
 #define FEED_IN_OFFSET 50.0f
 #define MIN_POWER_CONTROL 50.0f
 #define MAX_CHARGE_POWER_MARSTEK_VENUS_E -2500
 #define MAX_DISCHARGE_POWER_MARSTEK_VENUS_E 2500

static unsigned long int min_battery_control_loop_time=4000; // 4seconds
static int number_of_marstek_batteries=0;
static void queryMarstekPower(int idx);
static void queryMarstekSoC(int idx);
static void marstek_venus_e_controll(int new_grid_power);
static void sendMarstekPowerToDevice(int idx, int power);


static int16_t MarstekPower[NUMBER_OF_MARSTEK_BATTERIES_MAX];
static uint16_t MarstekSoC[NUMBER_OF_MARSTEK_BATTERIES_MAX];
static IPAddress MarstekVenusIP[NUMBER_OF_MARSTEK_BATTERIES_MAX];
static ModbusIP modbus_venus_e;
static int16_t target[NUMBER_OF_MARSTEK_BATTERIES_MAX]; // target power for each battery, negative value => charge, positive value => discharge, this is calculated in the control loop and then sent to the batteries, this is used to avoid sending too many commands to the batteries in case of small changes in the load or charge command, because the control loop runs every MIN_BATTERY_CONTROL_LOOP_TIME milliseconds, so that the batteries have some time to recover from "heavy" communication /sigh/


float getMarstekSoC(void)
{
  float val=0;
  for (int n=0;n<NUMBER_OF_MARSTEK_BATTERIES;n++) {
   val=val+(float)MarstekSoC[n];
  }
  if (NUMBER_OF_MARSTEK_BATTERIES>0) val=val/((float)NUMBER_OF_MARSTEK_BATTERIES);
  return val;
}


float getMarstekPower(void)
{
  float val=0;
  for (int n=0;n<NUMBER_OF_MARSTEK_BATTERIES;n++) {
   val=val+(float)MarstekPower[n];
  }
  return val;
}


/* external function, to be called during setup, accepts a list of hostnames and a control time
   hostnames must be separated by semicolons
 */
void setupMarstek(char* hostnames, int control_time) {
  // returns if the given text is empty, directly abort
  if (hostnames == nullptr || *hostnames == '\0') return;

  int count = 0;

  // strtok detects first name in the text, using ';' as separator. It modifies the input text by inserting null terminators, so 'hostnames' will be changed after this call.
  char* singleHostname = strtok(hostnames, ";");
  
  //exit loop when last name is found or limit is reached
  while (singleHostname != nullptr && count < NUMBER_OF_MARSTEK_BATTERIES_MAX) {
    
    // first, try to given string as IP adresses (e.g. "192.168.178.30")
    if (MarstekVenusIP[count].fromString(singleHostname)) {
        DEBUG_SERIAL.println(MarstekVenusIP[count]);
    }
    // second, try to resolve as hostname (e.g. "marstek-1.local")
    else if (USED_NETWORK.hostByName(singleHostname, MarstekVenusIP[count])) {
        DEBUG_SERIAL.println(MarstekVenusIP[count]);
    }
    // third, fallback: set IP to 0.0.0.0 to mark as invalid, this entry will be ignored in the control loop
    else {
        MarstekVenusIP[count] = IPAddress(0, 0, 0, 0); 
    }
    
    // init default values for this battery, these values will be used in the control loop until the first successful query of the battery, so that the battery can be controlled from the beginning, even if communication is not working for some reason.
    MarstekPower[count] = 0; // 0W is a good default value, because the control loop will not try to charge or discharge the battery at this power, so that the battery can be used even if communication is not working for some reason.
    MarstekSoC[count] = 11; // 11% SoC is a good default value, because the control loop will not try to discharge the battery at this SoC, but it will be charged if there is a charge wish, so that the battery can be used even if communication is not working for some reason.
    
    count++; // jump to next battery for next loop run, count is also used to save the number of batteries for later use in the control loop
    
    // detect next name in the text, using ';' as separator
    singleHostname = strtok(nullptr, ";");
  }
  
  // save number of batteries for later use in control loop
  number_of_marstek_batteries = count;
  
  if (control_time > 1000) {
      min_battery_control_loop_time = control_time;
  }
  modbus_venus_e.client();
}
 
 /* privat function for charge/discharge control loop */
static void marstek_venus_e_controll(int new_grid_power) 
{

    for (int n = 0; n < NUMBER_OF_MARSTEK_BATTERIES; n++) {
      queryMarstekPower(n);
      delay(30);
    }
    for (int n = 0; n < NUMBER_OF_MARSTEK_BATTERIES; n++) {
      queryMarstekSoC(n);
      delay(30);
    }

    /* give batteries some time to recover from "heavy" communication /sigh/ */
    /* to be checked, to be reduced to 0 */
    for (int n = 0; n < 100; n++) {
      modbus_venus_e.task();
      delay(10);
      yield();
    }


    float housePower = (float)new_grid_power + FEED_IN_OFFSET;
    int totalCurrentlyDelivered = 0;
    float sumsoc = 0.0f;
    
    
    for (int n=0;n<NUMBER_OF_MARSTEK_BATTERIES;n++) {
    	target[n]=0;
    	sumsoc=sumsoc+(float)MarstekSoC[n];
    	totalCurrentlyDelivered+=MarstekPower[n]; //negative MarstekPower => charge
    }
    int newTotalTarget = totalCurrentlyDelivered + (int)housePower;
  
    if (abs(newTotalTarget) < MIN_POWER_CONTROL) {
      newTotalTarget = 0;  // deactivate batteries if load or charge command is in sum < MIN_POWER_CONTROL e.g.50Watt
    }
  

   
    if (newTotalTarget > 0) {  // discharge batteries.
    
      float reference = newTotalTarget / (100.0f*(float)NUMBER_OF_MARSTEK_BATTERIES); // fallback if sumsoc is invalid 
      // compute weighted discharge power. batteries with larger SoC do more
      // external balancing between multiple devices while discharging
      if (sumsoc > 0) reference = newTotalTarget / sumsoc;
      for (int n = 0; n < NUMBER_OF_MARSTEK_BATTERIES; n++) {        
        target[n] = (int)(reference * MarstekSoC[n]);
      }
    }

    if (newTotalTarget < 0) {  // charge batteries
      for (int n = 0; n < NUMBER_OF_MARSTEK_BATTERIES; n++) {
        target[n] = newTotalTarget / NUMBER_OF_MARSTEK_BATTERIES;
      }
    }

    for (int n = 0; n < NUMBER_OF_MARSTEK_BATTERIES; n++) {  // negative target values => charge

      target[n] = constrain(target[n], MAX_CHARGE_POWER_MARSTEK_VENUS_E, MAX_DISCHARGE_POWER_MARSTEK_VENUS_E);
      sendMarstekPowerToDevice(n, target[n]);
    }
 }
 
 
 /* privat function for local power control */
static void sendMarstekPowerToDevice(int idx, int power) 
{
    if (!modbus_venus_e.isConnected(MarstekVenusIP[idx])) {
      modbus_venus_e.connect(MarstekVenusIP[idx], 502);
    } else {
      // enable external control, magic number
      modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42000, 0x55AA, nullptr, 255); // for some unknown reason value resets to default after X minutes
      delay(30);
      if ((power < 0) && (MarstekSoC[idx] >= 100)) { // charging whish, but SoC allready at 100%=> stop
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42020, 0, nullptr, 255);
        delay(30);
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42021, 0, nullptr, 255);
        delay(30);
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42010, 0x0000, nullptr, 255);  // stop, standby
        return;
      }
      if ((power > 0) && (MarstekSoC[idx] < 12)) { // discharging whish, but SoC allready at lower limit => stop
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42020, 0, nullptr, 255);
        delay(30);
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42021, 0, nullptr, 255);
        delay(30);
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42010, 0x0000, nullptr, 255);  // stop, standby
        return;
      }

      if (power > 0) {  // discharge as requested
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42021, abs(power), nullptr, 255);
        delay(30);
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42010, 0x0002, nullptr, 255); // discharge command
        return;
      }
      if (power < 0) {  // charge

        if ((MarstekSoC[idx]>=95)&&(abs(power)>100)) { // force slow charging with 100Watt! let bms some time for doing its job
          modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42020, 100, nullptr, 255);  
        }
        else { // charge as requested
         modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42020, abs(power), nullptr, 255);
        }
        
        delay(30);
        modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42010, 0x0001, nullptr, 255); // charge command
        return;
      }

      modbus_venus_e.writeHreg(MarstekVenusIP[idx], 42010, 0x0000, nullptr, 255);  // stop, standby
    }
  }

/* privat function for querying inverter power (AC) */
static void queryMarstekPower(int idx)
 {
    if (!modbus_venus_e.isConnected(MarstekVenusIP[idx])) {
      modbus_venus_e.connect(MarstekVenusIP[idx], 502);
    } else {
      uint16_t transaction = modbus_venus_e.readHreg(MarstekVenusIP[idx], 30006, (uint16_t *)&MarstekPower[idx], 1, nullptr, 255);
      delay(10);
      modbus_venus_e.task();
      int t = 0;
      while (modbus_venus_e.isTransaction(transaction)) {
        modbus_venus_e.task();
        delay(10);
        t++;
        if (t > 50) {
          DEBUG_SERIAL.println("Timeout Marstek Modbus");
          modbus_venus_e.disconnect(MarstekVenusIP[idx]);
          break;
        }
      }
      DEBUG_SERIAL.printf("Marstek power: %d,%d\n\r", idx, MarstekPower[idx]);
    }
  }

/* privat function for querying SoC */
static void queryMarstekSoC(int idx)
   {
    if (!modbus_venus_e.isConnected(MarstekVenusIP[idx])) {
      modbus_venus_e.connect(MarstekVenusIP[idx], 502);
    } else {
      uint16_t transaction = modbus_venus_e.readHreg(MarstekVenusIP[idx], 32104, (uint16_t *)&MarstekSoC[idx], 1, nullptr, 255);
      delay(10);
      modbus_venus_e.task();
      int t = 0;
      while (modbus_venus_e.isTransaction(transaction)) {
        modbus_venus_e.task();
        delay(10);
        t++;
        if (t > 50) {
          DEBUG_SERIAL.println("Timeout Marstek Modbus");
          modbus_venus_e.disconnect(MarstekVenusIP[idx]);
          break;
        }
      }
      DEBUG_SERIAL.printf("Marstek SoC: %d,%d\n\r", idx, MarstekSoC[idx]);
    }
  }


 /* main function, called from external*/
 void Marstek_battery(int new_grid_power)  // in Watt. negative=> excess Watt feed to grid.
 {
  static unsigned long last_control=0;
  static int last_power=0;
  //only do control every >=4s and if grid power has changed
  //e.g when smart meter delivers every 10s a new value, we would falsely act to fast.
  if (((millis() - last_control) > MIN_BATTERY_CONTROL_LOOP_TIME)&&(last_power!=new_grid_power)) {
      DEBUG_SERIAL.printf("Control-Time %ld\r\n", millis() - last_control);
      marstek_venus_e_controll(new_grid_power);
      last_control = millis();
      last_power=new_grid_power;
  }
  modbus_venus_e.task();
 }
