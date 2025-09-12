#ifndef PWM_AUDIO_H
#define PWM_AUDIO_H

#include <stdio.h>



class PWMAudioWio
{

    public:
    PWMAudioWio();
    void end();
    void begin(uint8_t audio_pin, const uint32_t audio_sample_rate, const uint32_t cpuFrequencyHz);
    void output_samples(const uint16_t samples[], const uint16_t len);

	  static void update();    

	  static volatile const uint16_t *buffer1;
	  static volatile uint16_t buffer1_len;

	  static volatile const uint16_t *buffer2;
	  static volatile uint16_t buffer2_len;

	static volatile bool buffer;
	static volatile uint16_t offset;

};


#endif
