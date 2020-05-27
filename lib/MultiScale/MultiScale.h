#ifndef MULTI_SCALE_M_H
#define MULTI_SCALE_M_H
#include "Arduino.h"
#include "HX711.h"
#define MAX_SCALE_NUM 4
class MultiScale
{
public:
void init(HX711 *scale1,HX711 *scale2,HX711 *scale3,HX711 *scale4);
float readKg();

void waitReady();
float calibrateTo(float kg);
void tare();

void saveConfig();

private:
float m_offsetTotal;
float m_scaleTotal;
HX711 *m_scales[4];
};

#endif