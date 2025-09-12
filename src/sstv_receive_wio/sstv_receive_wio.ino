// filename: sstv_decoder.ino
// Copyright (c) Jonathan P Dawson 2024
// description:
// Adaptation for Seeed Wio Terminal by IS0JSV Franciscu Capuzzi "Brabudu" 2025
//
// License: MIT
//
// Libraries needed: 
//
// * SSTV pico library https://github.com/dawsonjon/PicoSSTV/tree/encoder/sstv_library
// 
// Remove ADCAudio.cpp and ADCAudio.h from sstv_library

#include "sstv_decoder.h"
#include "ADCAudioWio.h"
#include "TFT_eSPI.h"


#define ENABLE_SLANT_CORRECTION true
//#define ENABLE_SLANT_CORRECTION false

#define LOST_SIGNAL_TIMEOUT_SECONDS 40

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

TFT_eSPI display=TFT_eSPI();


//c_sstv_decoder provides a reusable SSTV decoder
//We need to override some hardware specific functions to make it work with
//ADC audio input and TFT image display
class c_sstv_decoder_io : public c_sstv_decoder
{
 
  ADCAudio adc_audio;  
  uint16_t row_number = 0;
  const uint16_t display_width = 320;
  const uint16_t display_height = 240 - 10; //allow space for status bar

  //override the get_audio_sample function to read ADC audio
  int16_t get_audio_sample()
  {
    static int16_t *samples;
    static uint16_t sample_number = 1024;

    //if we reach the end of a block request a new one
    if(sample_number == 1024)
    {
      //fetch a new block of 1024 samples
      samples = adc_audio.input_samples();
      sample_number = 0;
    }

    //output the next sample in the block
    return samples[sample_number++];
  }

  //override the image_write_line function to output images to a TFT display
  void image_write_line(uint16_t line_rgb565[], uint16_t y, uint16_t width, uint16_t height, const char* mode_string)
  {

    //Serial.println(y);

    //scale image to fit TFT size
    uint16_t scaled_row[display_width];
    uint16_t pixel_number = 0;
    for(uint16_t x=0; x<width; x++)
    {
        uint16_t scaled_x = static_cast<uint32_t>(x) * display_width / width;
        while(pixel_number < scaled_x)
        {
          //display expects byteswapped data
          scaled_row[pixel_number] = line_rgb565[x];
          pixel_number++;
        }
    }

    //Serial.println(row_number);
    uint32_t scaled_y = static_cast<uint32_t>(y) * display_height / height;
    while(row_number < scaled_y)
    {
      writeHLine(0, row_number, display_width, scaled_row);
      row_number++;
    }

    //update progress
    display.fillRect(0, display_height, 10, display_width, TFT_BLACK);
    char buffer[21];
    snprintf(buffer, 21, "%10s: %ux%03u", mode_string, width, y+1);
    display.drawString(buffer, 0, display_height+2);
    //Serial.println(buffer);

  }

  void writeHLine(int16_t x, int16_t y, int16_t w, uint16_t* color) {
    for (int i=0;i<w;i++) {
      display.drawPixel(i+x,y, color[i]);
      
    }
  } 

  void scope(uint16_t mag, int16_t freq) {
        
    // Frequency
    static uint8_t row=0;
    static uint16_t count=0;
    static uint16_t w[150];
    static float mean_freq=0;

    uint16_t val=0;
    
    uint8_t f=(freq-1000)/10; //from 1500-2300 to 50-130
    
    mean_freq=(mean_freq*15+f)/16;
    f=mean_freq;

    if (f>0 && f<150) {
      w[f]=(w[f]<<1)|3; //Pseudo exponential increment
    }
    if (count>200 ) {
      w[20]=0xF800;  //1200 hz red line
      w[50]=0xF800;  //1500 hz red line
      w[130]=0xF800; //2300 hz red line
      writeHLine(150,231+row++,150,w);
      
      for (int i=0;i<150;i++) {
        w[i]=w[i]>>2;  //Exponential decay
        val+=w[i]/150; //Accumulator for signal strenght
      }

      if (row>8) row=0;   
      count=0;

      val=(val-300)/500; //Scale to 0-8
      // Draw signal bar
      display.fillRect(145, 231, 3, 8-val, TFT_BLACK);
      display.fillRect(145, 239-val, 3, val, TFT_GREEN);
    }
    count++;

  }

  public:

  void start(){adc_audio.begin(28, 15000); row_number = 0;}
  void stop(){adc_audio.end();}
  c_sstv_decoder_io(float fs) : c_sstv_decoder{fs}{
    
  }

};

void setup() {
  Serial.begin(115200); 
  configure_display();
  
}

void loop() {
  c_sstv_decoder_io sstv_decoder(15000);
  while(1)
  {
    sstv_decoder.start();
    sstv_decoder.decode_image(LOST_SIGNAL_TIMEOUT_SECONDS, ENABLE_SLANT_CORRECTION);
    sstv_decoder.stop();
  }
}

void configure_display()
{
    display.begin();
    display.setRotation(3);
    digitalWrite(LCD_BACKLIGHT, HIGH); // turn on the backlight
    display.drawString("SSTV decoder ready.",2,2);
}

