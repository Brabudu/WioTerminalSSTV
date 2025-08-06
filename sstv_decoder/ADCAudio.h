#ifndef ADC_AUDIO_H
#define ADC_AUDIO_H

#include <stdio.h>
//#include <adc.h>
#include "Adafruit_ZeroDMA.h"

/* 
Adaptation by Franciscu Capuzzi "Brabudu" 2025
Based on 
https://github.com/ShawnHymel/ei-keyword-spotting/blob/master/embedded-demos/arduino/wio-terminal/wio-terminal.ino
* Author: Shawn Hymel
*/

class ADCAudio
{

    public:
    ADCAudio();
    void begin();
    void end();
    void input_samples(uint16_t*& samples);

    private:
    int adc_dma;
};

#endif

