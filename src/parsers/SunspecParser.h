#ifndef SUNSPECPARSER_H
#define SUNSPECPARSER_H
#include <Arduino.h>

void setupSUNSPEC_power_register(char *s); 
void setupSUNSPEC_apparant_power_register(char *s);
void setupSUNSPEC_voltage_register(char *s);
void setupSUNSPEC_current_register(char *s);
void setupSUNSPEC_frequency_register_base_and_count(char *s);
void setupSUNSPEC_power_factor_register(char *s);
void setupSUNSPEC_real_energy_exported_register(char *s);
void setupSUNSPEC_real_energy_imported_register(char *s);
void setupSUNSPEC_from_configpage(void);
#endif