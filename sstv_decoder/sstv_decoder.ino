

//Adaptation for wio Terminal by Franciscu Capuzzi "Brabudu" 2025
// from:
//
// Copyright (c) Jonathan P Dawson 2024
// filename: sstv_decoder.ino
// description:
//
// SSTV Decoder using pi-pico.
//

// License: MIT
//


#include <SPI.h>
#include "font_8x5.h"
#include "font_16x12.h"
#include "decode_sstv.h"
#include "ADCAudio.h"



#define STRETCH true
//#define STRETCH false

#define ENABLE_SLANT_CORRECTION true
//#define ENABLE_SLANT_CORRECTION false

#define LOST_SIGNAL_TIMEOUT_SECONDS 40

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

#include"TFT_eSPI.h"

TFT_eSPI display;
ADCAudio adc_audio;
c_sstv_decoder sstv_decoder(15000);

s_sstv_mode *modes;

int16_t dc;
uint8_t line_rgb[320][4];
uint16_t last_pixel_y=0;

void setup() {
  Serial.begin(115200);
  delay(500);
  configure_display();


  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  adc_audio.begin();
  
  modes = sstv_decoder.get_modes();
  

  sstv_decoder.set_auto_slant_correction(ENABLE_SLANT_CORRECTION);
  sstv_decoder.set_timeout_seconds(LOST_SIGNAL_TIMEOUT_SECONDS);

 }


void loop() {
  {
    
     if (digitalRead(WIO_KEY_A) == LOW) {
       last_pixel_y=0;
       dc=0;
       sstv_decoder.reset();
       configure_display();
     }
     if (digitalRead(WIO_KEY_B) == LOW) {
       //TODO
     }
    uint16_t *samples;
    
    adc_audio.input_samples(samples);
    
    for(uint16_t idx=0; idx<1024; idx++)
    {
      dc = dc + (samples[idx] - dc)/2;
      int16_t sample = samples[idx] - dc;
           
      uint16_t pixel_y;
      uint16_t pixel_x;
      uint8_t pixel_colour;
      uint8_t pixel;
      int16_t frequency;
      const bool new_pixel = sstv_decoder.decode_audio(sample, pixel_y, pixel_x, pixel_colour, pixel, frequency);
      
      if(new_pixel)
      {
        
          e_mode mode = sstv_decoder.get_mode();

          if(pixel_y > last_pixel_y)
          {

            //convert from 24 bit to 16 bit colour
            uint32_t line_rgb565[320];
            uint32_t scaled_pixel_y = 0;

            if(mode == pd_50 || mode == pd_90 || mode == pd_120 || mode == pd_180)
            {

              //rescale imaagesto fit on screen
              if(mode == pd_120 || mode == pd_180)
              {
                scaled_pixel_y = (uint32_t)last_pixel_y * 240 / 496; 
              }
              else
              {
                scaled_pixel_y = last_pixel_y;
              }

              for(uint16_t x=0; x<320; ++x)
              {
                int16_t y  = line_rgb[x][0];
                int16_t cr = line_rgb[x][1];
                int16_t cb = line_rgb[x][2];
                cr = cr - 128;
                cb = cb - 128;
                int16_t r = y + 45 * cr / 32;
                int16_t g = y - (11 * cb + 23 * cr) / 32;
                int16_t b = y + 113 * cb / 64;
                r = r<0?0:(r>255?255:r);
                g = g<0?0:(g>255?255:g);
                b = b<0?0:(b>255?255:b);
                line_rgb565[x] = display.color565(r, g, b);
              }
              writeHLine(0, scaled_pixel_y*2, 320, line_rgb565);
              for(uint16_t x=0; x<320; ++x)
              {
                int16_t y  = line_rgb[x][3];
                int16_t cr = line_rgb[x][1];
                int16_t cb = line_rgb[x][2];
                cr = cr - 128;
                cb = cb - 128;
                int16_t r = y + 45 * cr / 32;
                int16_t g = y - (11 * cb + 23 * cr) / 32;
                int16_t b = y + 113 * cb / 64;
                r = r<0?0:(r>255?255:r);
                g = g<0?0:(g>255?255:g);
                b = b<0?0:(b>255?255:b);
                line_rgb565[x] = display.color565(r, g, b);
              }
              writeHLine(0, scaled_pixel_y*2 + 1, 320, line_rgb565);
            }
            else
            {
              for(uint16_t x=0; x<320; ++x)
              {
                line_rgb565[x] = display.color565(line_rgb[x][0], line_rgb[x][1], line_rgb[x][2]);
              }
              writeHLine(0, last_pixel_y, 320, line_rgb565);
              
            }
            for(uint16_t x=0; x<320; ++x) line_rgb[x][0] = line_rgb[x][1] = line_rgb[x][2] = 0;

            //update progress
            display.fillRect(320-(21*6)-2, 240-10, 10, 21*6+2, TFT_BLACK); 
            char buffer[21];
            if(mode==martin_m1)
            {
              snprintf(buffer, 21, "Martin M1: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==martin_m2)
            {
              snprintf(buffer, 21, "Martin M2: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==scottie_s1)
            {
              snprintf(buffer, 21, "Scottie S1: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==scottie_s2)
            {
              snprintf(buffer, 21, "Scottie S2: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==scottie_dx)
            {
              snprintf(buffer, 21, "Scottie DX: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==sc2_120)
            {
              snprintf(buffer, 21, "SC2 120: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_50)
            {
              snprintf(buffer, 21, "PD 50: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_90)
            {
              snprintf(buffer, 21, "PD 90: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_120)
            {
              snprintf(buffer, 21, "PD 120: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_180)
            {
              snprintf(buffer, 21, "PD 180: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            display.drawString(buffer, (int32_t) (320-(21*6)), (int32_t) (240-8));//, font_8x5);// ,  TFT_WHITE, TFT_BLACK);
            Serial.println(buffer);

          }
          last_pixel_y = pixel_y;
          

          if(pixel_x < 320 && pixel_y < 256 && pixel_colour < 4) {
            if(STRETCH && modes[mode].width==160)
            {
              if(pixel_x < 160)
              {
                line_rgb[pixel_x*2][pixel_colour] = pixel;
                line_rgb[pixel_x*2+1][pixel_colour] = pixel;
              }
            }
            else
            {
              line_rgb[pixel_x][pixel_colour] = pixel;
            }

          }
          
      }

    }
  }
  
}

void configure_display()
{
    display.begin();
    display.setRotation(3);
    digitalWrite(LCD_BACKLIGHT, HIGH); // turn on the backlight
    display.drawString("SSTV decoder V1.0",20,0);
}

void writeHLine(int32_t x, int32_t y, int32_t w, uint32_t* color) {
  for (int i=x;i<w+x;i++) display.drawPixel(i,y, color[i]);
}
