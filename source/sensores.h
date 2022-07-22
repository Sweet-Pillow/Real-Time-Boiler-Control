#ifndef SENSORES_H
#define SENSORES_H

void sensor_put(double temp, double nivel, double temp_ambi, double temp_entr, double fluxo_said);
double sensor_get(char s[5]);
void sensor_alarmeT(double limite);

#endif



