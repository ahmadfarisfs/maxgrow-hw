#include "MultiScale.h"
#include <EEPROM.h>
void MultiScale::init(HX711 *scale1,HX711 *scale2,HX711 *scale3,HX711 *scale4){
    m_scales[0] = scale1;
    m_scales[1] = scale2;
    m_scales[2] = scale3;
    m_scales[3] = scale4;
}
float MultiScale::readKg(){
    ulong sum;
    for (int i=0;i<4;i++){
      sum+=  m_scales[i]->read_average();
    }
    float average = sum / float(4.0);
     float kg = m_scaleTotal  * average;
    return kg;
}
void MultiScale::saveConfig(){
    EEPROM.put(2,m_scaleTotal);
}

void MultiScale::tare(){
    for (int i=0;i<4;i++){
        m_scales[i]->tare();
    }
}

float MultiScale::calibrateTo(float kg){
    ulong sum;
    for (int i=0;i<4;i++){
        sum+=m_scales[i]->read_average();
    }
    float average = sum / float(4.0);
    m_scaleTotal = kg / average;
    return m_scaleTotal;
}