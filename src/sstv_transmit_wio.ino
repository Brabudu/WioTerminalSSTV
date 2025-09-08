//  _  ___  _   _____ _     _                 
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___ 
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \.
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/    
//
// Copyright (c) Jonathan P Dawson 2025
// filename: sstv_encoder.ino
// description:
//
// SSTV Encoder using pi-pico.
//
// Reads bmp image from SD card and transmits via SSTV
// Supports Martin and Scottie modes
//
// License: MIT

#include <sstv_encoder.h>
#include "PWMAudioWio.h"



void setup() {
  Serial.begin(115200);

  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2025");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
 

}

//Derive a class from sstv encoder and override hardware specific functions
const uint16_t audio_buffer_length = 4096u;
class c_sstv_encoder_pwm : public c_sstv_encoder
{

  private :
  uint16_t audio_buffer[2][audio_buffer_length];
  uint16_t audio_buffer_index = 0;
  uint8_t ping_pong = 0;
  PWMAudioWio audio_output;
  uint16_t sample_min, sample_max;

  void output_sample(int16_t sample)
  {
    uint16_t scaled_sample = ((sample+32767)>>5);// + 1024;
   
    audio_buffer[ping_pong][audio_buffer_index++] = scaled_sample;
    if(audio_buffer_index == audio_buffer_length)
    {
      audio_output.output_samples(audio_buffer[ping_pong], audio_buffer_length);
      ping_pong ^= 1;
      audio_buffer_index = 0;
      //Serial.println(sample_min);
      //Serial.println(sample_max);
      sample_max = scaled_sample;
      sample_min = scaled_sample;
    }
    else
    {
      sample_max = max(sample_max, scaled_sample);
      sample_min = min(sample_min, scaled_sample);
    }
  }
  
  uint8_t get_image_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour)
  {
         //b
    return 0;
  }
  
  
  uint16_t row_number = 0;
  uint16_t row[320];
  uint16_t image_width, image_height;
  
  public:
  void open()
  { 
    row_number=0;
    audio_output.begin(0, 15000, 0);
  }
  void close()
  { 
    
    audio_output.end();
  }
  c_sstv_encoder_pwm(double fs_Hz) : c_sstv_encoder(fs_Hz){}

};

void loop() {
  
  c_sstv_encoder_pwm sstv_encoder(15000);
  sstv_encoder.open();
  sstv_encoder.generate_sstv(tx_martin_m2);
  sstv_encoder.close();
}


