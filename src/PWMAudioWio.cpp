#include "PWMAudioWio.h"

// Thanks to 
// https://forum.seeedstudio.com/t/wio-terminal-and-samd-51-dac/262623
// https://github.com/Dennis-van-Gils/SAMD51_InterruptTimer

#include "SAMD51_InterruptTimer.h"
#include "Arduino.h"

PWMAudioWio ::PWMAudioWio() {
}

void PWMAudioWio::end() 
{
    TC.stopTimer();
}

void PWMAudioWio::begin(const uint8_t audio_pin, const uint32_t audio_sample_rate, const uint32_t cpuFrequencyHz) 
{
  	buffer=true;
    offset=0;
    TC.startTimer(1/audio_sample_rate, this->update);
	  analogWriteResolution(12); 
}

void PWMAudioWio::update()
{
	if (buffer) {
		analogWrite(11,(uint32_t)*(buffer1+offset++));
		if (offset>buffer1_len) {
			offset=0;
			buffer=false;		
		}
	} else {
		analogWrite(11,(uint32_t)*(buffer2+offset++));
		if (offset>buffer2_len) {
			offset=0;
			buffer=true;		
		}	
	}	
}

void PWMAudioWio::output_samples(const uint16_t samples[], const uint16_t len) {
	static bool last_buff;

	while (buffer==last_buff);
	
  	if (buffer) {
		buffer1=samples;
		buffer1_len=len;
	} else {
		buffer2=samples;
		buffer2_len=len;
	}

	last_buff=buffer;
}
