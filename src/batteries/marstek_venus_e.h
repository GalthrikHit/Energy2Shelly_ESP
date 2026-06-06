#ifndef MARSTEK_VENUS_E_H
#define MARSTEK_VENUS_E_H

/* external lib functions */
void setupMarstek(char* hostnames, int control_time); // called once in setup, control_time in ms
void Marstek_battery(int new_grid_power); // called in every loop, Watt
float getMarstekSoC(void); // average of local cached SoC data in %
float getMarstekPower(void); // sum of local cached power values in Watt, negative MarstekPower => charge

 

#endif
