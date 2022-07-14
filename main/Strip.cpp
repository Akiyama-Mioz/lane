//
// Created by Kurosu Chan on 2022/7/13.
//

#include "Strip.h"

const int fps = 10;  // refresh each 100ms
const int PIN = 23;// 定义控制引脚
//const int work_LED = 2;
//const int NUMPIXELS = 2; //初始控制灯珠数
const int max_leds = 100;

auto strip = Adafruit_NeoPixel(max_leds, PIN, NEO_GRB + NEO_KHZ800);

void stripTask() {
  strip.begin();
  strip.setBrightness(255);
  printf("Init strip\n");
  for(;;){
    for (int i = 0; i < max_leds; i++) {
      strip.fill(0xffff00, i, 10);
      strip.show();
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}
