// filename: sstv_decoder.ino
// Copyright (c) Jonathan P Dawson 2024
// description:
// Adaptation for Seeed Wio Terminal by IS0JSV Franciscu Capuzzi "Brabudu" 2025
//
// License: MIT
//


// KEYS:

// Capture mode:
// ------------
// A : Reset capture
// B : Rotate screen
// C : Enter slideshow mode

// Slideshow mode:
// ---------------
// Right: change slide
// Left: change slide
// B (long press): delete slide
// C : Return to capture mode

#include "sstv_decoder.h"
#include "ADCAudioWio.h"
#include "TFT_eSPI.h"

#include <bmp_lib.h>
#include <Seeed_FS.h>  //Seeed Arduino FS needed
#include "SD/Seeed_SD.h"

#define ENABLE_SLANT_CORRECTION true
//#define ENABLE_SLANT_CORRECTION false

#define LOST_SIGNAL_TIMEOUT_SECONDS 5

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

TFT_eSPI display = TFT_eSPI();

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define BAR_HEIGHT 10

File myImg;

uint8_t display_rotation = 3;
bool sd = true;

void display_image(const char* filename);
void writeHLine(uint16_t, uint16_t, uint16_t, uint16_t*);

class c_bmp_writer_stdio : public c_bmp_writer {

  File myFile;

  bool file_open(const char* filename) {
    if (sd) myFile = SD.open(filename, FILE_WRITE);
    return (myFile);
  }

  void file_close() {
    if (myFile) myFile.close();
  }

  void file_write(const void* data, uint32_t element_size, uint32_t num_elements) {
    if (myFile) myFile.write((char*)data, element_size * num_elements);
  }

  void file_seek(uint32_t offset) {
    if (myFile) myFile.seek(offset);
  }
};

class c_bmp_reader_stdio : public c_bmp_reader {

  File myFile;

  bool file_open(const char* filename) {
    if (sd) myFile = SD.open(filename, FILE_READ);
    return (myFile);
  }

  void file_close() {
    if (myFile) myFile.close();
  }

  uint32_t file_read(void* data, uint32_t element_size, uint32_t num_elements) {
    if (myFile) return myFile.read(data, element_size * num_elements);
  }

  void file_seek(uint32_t offset) {
    if (myFile) myFile.seek(offset);
  }
};



//c_sstv_decoder provides a reusable SSTV decoder
//We need to override some hardware specific functions to make it work with
//ADC audio input and TFT image display

class c_sstv_decoder_io : public c_sstv_decoder {

  ADCAudio adc_audio;
  uint16_t row_number = 0;
  const uint16_t display_width = DISPLAY_WIDTH;
  const uint16_t display_height = DISPLAY_HEIGHT - BAR_HEIGHT;  //allow space for status bar
  uint16_t tft_row_number = 0;

  //override the get_audio_sample function to read ADC audio
  int16_t get_audio_sample() {
    static int16_t* samples;
    static uint16_t sample_number = 1024;

    //if we reach the end of a block request a new one
    if (sample_number == 1024) {
      //fetch a new block of 1024 samples
      samples = adc_audio.input_samples();
      sample_number = 0;
    }

    //output the next sample in the block
    return samples[sample_number++];
  }

  //override the image_write_line function to output images to a TFT display
  void image_write_line(uint16_t line_rgb565[], uint16_t y, uint16_t width, uint16_t height, const char* mode_string) {

    //write unscaled image to bmp file
    output_file.change_width(width);
    output_file.change_height(y + 1);

    //write unscaled image to bmp file
    if (++bmp_row_number < height) output_file.write_row_rgb565(line_rgb565);

    //scale image to fit TFT size
    uint16_t scaled_row[display_width];
    uint16_t pixel_number = 0;
    for (uint16_t x = 0; x < width; x++) {
      uint16_t scaled_x = static_cast<uint32_t>(x) * display_width / width;
      while (pixel_number < scaled_x) {
        //display expects byteswapped data
        scaled_row[pixel_number] = line_rgb565[x];
        pixel_number++;
      }
    }

    //Serial.println(row_number);
    uint32_t scaled_y = static_cast<uint32_t>(y) * display_height / height;
    while (row_number < scaled_y) {
      writeHLine(0, row_number, display_width, scaled_row);
      row_number++;
    }

    //update progress
    display.fillRect(0, display_height, 10, display_width, TFT_BLACK);
    char buffer[21];
    snprintf(buffer, 21, "%10s: %ux%03u", mode_string, width, y + 1);
    display.drawString(buffer, 0, display_height + 2);
    //Serial.println(buffer);
  }


  void scope(uint16_t mag, int16_t freq) {

    // Frequency
    static uint8_t row = 0;
    static uint16_t count = 0;
    static uint16_t w[150];
    static float mean_freq = 0;

    uint16_t val = 0;

    uint8_t f = (freq - 1000) / 10;  //from 1500-2300 to 50-130

    mean_freq = (mean_freq * 15 + f) / 16;
    f = mean_freq;

    if (f > 0 && f < 150) {
      w[f] = (w[f] << 1) | 3;  //Pseudo exponential increment
    }
    if (count > 200) {
      w[20] = 0xF800;   //1200 hz red line
      w[50] = 0xF800;   //1500 hz red line
      w[130] = 0xF800;  //2300 hz red line
      writeHLine(150, 231 + row++, 150, w);

      for (int i = 0; i < 150; i++) {
        w[i] = w[i] >> 2;   //Exponential decay
        val += w[i] / 150;  //Accumulator for signal strenght
      }

      if (row > 8) row = 0;
      count = 0;

      val = (val - 300) / 500;  //Scale to 0-8
      // Draw signal bar
      display.fillRect(145, 231, 3, 8 - val, TFT_BLACK);
      display.fillRect(145, 239 - val, 3, val, TFT_GREEN);
    }
    count++;
  }

  void writeHLine(int16_t x, int16_t y, int16_t w, uint16_t* color) {
    for (int i = 0; i < w; i++) {
      display.drawPixel(i + x, y, color[i]);
    }
  }


  c_bmp_writer_stdio output_file;
  uint16_t bmp_row_number = 0;

public:

  void open(const char* bmp_file_name) {
    tft_row_number = 0;
    bmp_row_number = 0;
    Serial.print("opening bmp file: ");
    Serial.println(bmp_file_name);
    output_file.open(bmp_file_name, 10, 10);  //image size gets updated later
  }

  void close() {
    tft_row_number = 0;
    bmp_row_number = 0;
    Serial.println("closing bmp file");
    output_file.update_header();
    output_file.close();
  }

  void start() {
    adc_audio.begin(28, 15000);
    row_number = 0;
  }
  void stop() {
    adc_audio.end();
  }
  c_sstv_decoder_io(float fs)
    : c_sstv_decoder{ fs } {
  }
};

class c_slideshow {

private:

  bool redraw = false;
  File root;
  uint16_t num_bitmaps = 0;
  uint16_t bitmap_index = 0;
  String filename;
  uint32_t last_update_time = 0;

public:
  void launch_slideshow() {

    root = SD.open("/");
    num_bitmaps = count_bitmaps(root);
    bitmap_index = 0;
    last_update_time = 0;
    update_slideshow(true);
  }

  void update_slideshow(bool redraw) {
    if (num_bitmaps == 0) return;

    static const uint16_t timeouts[] = { 0, 1, 2, 5, 10, 30, 60, 60 * 2, 60 * 5 };
    uint16_t timeout_milliseconds = 1000 * timeouts[0];

    /*
    if (((millis() - last_update_time) > timeout_milliseconds) && (timeout_milliseconds != 0)) {
      last_update_time = millis();
      if (bitmap_index == num_bitmaps - 1) bitmap_index = 0;
      else bitmap_index++;
      redraw = true;
    }
*/
    if (digitalRead(WIO_KEY_B) == LOW) {
      delay(500);
      if (digitalRead(WIO_KEY_B) == HIGH) return;
      get_bitmap_index(root, bitmap_index);
      filename = root.name();
      filename=filename.substring(3);
      SD.remove(filename);
      bitmap_index = 0;  //TODO std::min((int)bitmap_index, num_bitmaps-2);
      root = SD.open("/");
      num_bitmaps--;
      if (num_bitmaps == 0) return;
      redraw = true;
    }

    if (digitalRead(WIO_5S_RIGHT) == LOW) {
      delay(100);
      if (bitmap_index == num_bitmaps - 1) bitmap_index = 0;
      else bitmap_index++;
      redraw = true;
    }

    if (digitalRead(WIO_5S_LEFT) == LOW) {
      delay(100);
      if (bitmap_index == 0) bitmap_index = num_bitmaps - 1;
      else bitmap_index--;
      redraw = true;
    }

    if (redraw) {
      File f=get_bitmap_index(root, bitmap_index);
      filename = f.name();
      filename=filename.substring(3);
      display.begin(TFT_BLACK);
      draw_banner(filename.c_str());
      display_image(filename.c_str());
      uint16_t width = strlen(filename.c_str()) * 6 + 10;
      
      //draw_button_bar("Menu", "Delete", "Last", "Next");
    }
  }

  uint16_t count_bitmaps(File& root) {
    uint16_t count = 0;
    File f;
    root.rewindDirectory();
    while (f=root.openNextFile()) {
      String filename = f.name();
      if (!f.isDirectory() && filename.endsWith(".bmp")) count++;
    }
    return count;
  }
  File get_bitmap_index(File& root, uint16_t index) {
    File f;
    uint16_t count = 0;
    root.rewindDirectory();
    while (f=root.openNextFile()) {
      String filename = f.name();
      if (!f.isDirectory() && filename.endsWith(".bmp")) {
        if (count == index) return f;
        count++;
      }
    }
  }
  void display_image(const char* filename) {
    c_bmp_reader_stdio bitmap;
    uint16_t width, height;
    bitmap.open(filename, width, height);

    const uint16_t display_width = DISPLAY_WIDTH, display_height = DISPLAY_HEIGHT - BAR_HEIGHT;
    uint16_t tft_row_number = 0;

    for (uint16_t y = 0; y < height; y++) {
      uint16_t line_rgb565[width];
      bitmap.read_row_rgb565(line_rgb565);

      //scale image to fit TFT size
      uint16_t scaled_row[display_width];
      uint16_t pixel_number = 0;
      for (uint16_t x = 0; x < width; x++) {
        uint16_t scaled_x = static_cast<uint32_t>(x) * display_width / width;
        while (pixel_number < scaled_x) {
          //display expects byteswapped data
          scaled_row[pixel_number] = ((line_rgb565[x] & 0xff) << 8) | ((line_rgb565[x] & 0xff00) >> 8);
          pixel_number++;
        }
      }
      if (height>display_height) {
        uint32_t scaled_y = static_cast<uint32_t>(y) * display_height / height;
        while (tft_row_number < scaled_y) {
          writeHLine(0, tft_row_number, display_width, scaled_row);
          tft_row_number++;
        }
      } else {
        writeHLine(0, tft_row_number++, display_width, scaled_row);
      }
    }

    bitmap.close();
  }

  void draw_banner(const char* filename) {
     display.drawString(filename, 10, DISPLAY_HEIGHT - BAR_HEIGHT + 2);
  }
  void writeHLine(int16_t x, int16_t y, int16_t w, uint16_t* color) {
    display.pushImage(x, y, w, 1, color);
   /* for (int i = 0; i < w; i++) {
      display.drawPixel(i + x, y, color[i]);
    }*/
  }
};

void setup() {
  Serial.begin(115200);
  configure_display();

  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    sd = false;
    Serial.println("initialization failed!");
  }
}

void loop() {
  c_sstv_decoder_io sstv_decoder(15000);

  sstv_decoder.open("temp");
  char filename[100];
  get_new_filename(filename, 100);
  sstv_decoder.start();

  c_slideshow slideshow;

  bool autosave = true;
  bool capture = true;

  bool image_in_progress = false;
  while (!sstv_decoder.decode_image_non_blocking(LOST_SIGNAL_TIMEOUT_SECONDS, ENABLE_SLANT_CORRECTION, image_in_progress)) {
    if (digitalRead(WIO_KEY_A) == LOW) {
      while (digitalRead(WIO_KEY_A) == LOW);
      display.begin(TFT_BLACK);
      autosave = false;
      break;
    } else if (digitalRead(WIO_KEY_B) == LOW) {
      while (digitalRead(WIO_KEY_B) == LOW);

      if (display_rotation == 1) display_rotation = 3;
      else display_rotation = 1;

      display.setRotation(display_rotation);
      display.begin(TFT_BLACK);
      autosave = false;
      break;
    } else if (digitalRead(WIO_KEY_C) == LOW) {
      while (digitalRead(WIO_KEY_C) == LOW);
      autosave = false;
      capture = false;
      break;
    }
  }
  sstv_decoder.close();
  if (autosave) {
    SD.rename("temp", filename);
    Serial.print("copy to: ");
    Serial.println(filename);
    get_new_filename(filename, 100);
  }

  sstv_decoder.stop();
  autosave = true;
  if (capture == false) {
    slideshow.launch_slideshow();
    while (1) {

      slideshow.update_slideshow(false);
      delay(100);
      //Pressing button C return to capture
      if (digitalRead(WIO_KEY_C) == LOW) {
        display.begin(TFT_BLACK);
        while (digitalRead(WIO_KEY_C) == LOW);
        capture=true;
      
        break;
      }
    }
    sstv_decoder.open("temp");
    //capture=true;
  }
}

void get_new_filename(char* buffer, uint16_t buffer_size) {
  static uint16_t serial_number = 0;
  do {
    snprintf(buffer, buffer_size, "sstv_rx_%u.bmp", serial_number);
    serial_number++;
  } while (SD.exists(buffer));
}


void configure_display() {
  display.begin();
  display.setRotation(3);
  digitalWrite(LCD_BACKLIGHT, HIGH);  // turn on the backlight
  display.drawString("SSTV decoder ready.", 2, 2);
}

void writeHLine(int16_t x, int16_t y, int16_t w, uint16_t* color) {
 
  for (int i = 0; i < w; i++) {
    display.drawPixel(i + x, y, color[i]);
  }
}
