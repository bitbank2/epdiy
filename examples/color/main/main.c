/* Simple firmware for a ESP32 displaying a Color square in a DES epaper screen with CFA on top */
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "esp_sleep.h"
#include <epdiy.h>
#include "epd_highlevel.h"
#include "JPEGDEC.h"
//#include "bart_400x168.h" // 400x346 test image
//#include "epd_color_test.h"
#include "iron.h"

JPEGIMAGE jpg; // static JPEG decoder structure

EpdiyHighlevelState hl;
int temperature = 20;
// Buffers
uint8_t *fb;            // EPD 2bpp buffer

//
// Treat the display as if each pixel is part of a Bayer pattern
//
void fine_pixel(int x, int y, uint16_t u16Color, uint8_t *fb)
{
int i = (x+y) % 3; // which color filter is in use?
uint8_t u8, u8C;

        if (x > 1447 || y > 1071) return;
        if (i == 0) {
           u8C = (u16Color >> 7) & 0xf; // green
        } else if (i == 1) {
           u8C = (u16Color >> 1) & 0xf; // blue
        } else {
           u8C = (u16Color >> 12);      // red
        }
	fb += (y * (1448/2)) + (x/2); // point to correct byte
        u8 = fb[0];
        if (x & 1) { // right pixel in MSN
           u8 &= 0xf; u8 |= (u8C << 4);
        } else {
	   u8 &= 0xf0; u8 |= u8C;
        }
	fb[0] = u8;
} /* fine_pixel() */

//
// Treat the display as if each pixel is a 3x3 block of pixels
// The color is passed as RGB565 and displayed as RGB444
//
void chunky_pixel(int x, int y, uint16_t u16Color, uint8_t *fb)
{
int tx, ty;
uint8_t u8, u8C, *d;
int iFilter;
uint8_t u8RGB[4];

   if (x > 482 || y > 357) return;
	// Convert RGB565 into 4-bits of G/B/R
	u8RGB[0] = (u16Color >> 7) & 0xf; // green
	u8RGB[1] = (u16Color >> 1) & 0xf; // blue
	u8RGB[2] = (u16Color >> 12);      // red
	x = x*3; y = y*3; // 3x3 pixel block
	for (ty=y; ty<y+3; ty++) {
	        d = &fb[(ty * (1448/2)) + (x/2)];
		iFilter = (x+ty) % 3; 
		for (tx=x; tx<x+3; tx++) {
			u8 = d[0];
			u8C = u8RGB[iFilter];
		        if (tx & 1) { // right pixel in MSN
           			u8 &= 0xf; u8 |= (u8C << 4);
				*d++ = u8;
        		} else {
           			u8 &= 0xf0; u8 |= u8C;
				*d = u8;
        		}
			iFilter++;
			if (iFilter == 3) iFilter = 0;
		} // for tx
	} // for ty
} /* chunky_pixel() */

int JPEGDraw(JPEGDRAW *pDraw)
{
   uint16_t *s = pDraw->pPixels;
      for (int cy=0; cy < pDraw->iHeight; cy++) {
         for (int cx=0; cx<pDraw->iWidth; cx++) {
            chunky_pixel(pDraw->x+cx,pDraw->y+cy,*s++, hl.front_fb);
         }
      }
   return 1;
} /* JPEGDraw() */

int color_test() {
  EpdRect epd_area = {
      .x = 0,
      .y = 0,
      .width = epd_width(),
      .height = epd_height()
  };

  JPEG_openRAM(&jpg, iron, sizeof(iron), JPEGDraw);
//  JPEG_openRAM(&jpg, bart_400x168, sizeof(bart_400x168), JPEGDraw);
  jpg.ucPixelType = RGB565_LITTLE_ENDIAN;
  JPEG_decode(&jpg, 0, 0, 0);
  JPEG_close(&jpg);

  enum EpdDrawError _err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
  epd_poweroff();
  return _err;
}

// Deepsleep
#define DEEP_SLEEP_SECONDS 300
uint64_t USEC = 1000000;
int getFreePsram(){
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    return info.total_free_bytes;
}

#ifndef ARDUINO_ARCH_ESP32
void app_main() {
  printf("before epd_init() Free PSRAM: %d\n", getFreePsram());

  epd_init(&epd_board_v7, &EC060KH3, EPD_LUT_64K);
  //epd_set_rotation(EPD_ROT_PORTRAIT);
  // Set VCOM for boards that allow to set this in software (in mV).
  epd_set_vcom(1560);
  hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);
  fb = epd_hl_get_framebuffer(&hl);
  printf("after epd_hl_init() Free PSRAM: %d\n", getFreePsram());


  epd_poweron();
  epd_set_gamma_curve(0.4);
  epd_fullclear(&hl, 25);


  color_test();
 // printf("color example\n");

  while (1) {
     vTaskDelay(100);
  }
//  vTaskDelay(pdMS_TO_TICKS(5000));
//  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_SECONDS * USEC);
//  esp_deep_sleep_start();
}

#endif
