//
// Created by Kurosu Chan on 2022/7/13.
//

#include "StripCommon.h"
float fps=6; //distance travelled ! max = 7 !
uint8_t stop =0;
void Strip::Get_color(void)  {
  color[0] = pref.getUInt("color0", Adafruit_NeoPixel::Color(255, 0, 255));
  color[1] = pref.getUInt("color1", Adafruit_NeoPixel::Color(255, 255, 0));
  color[2] = pref.getUInt("color2", Adafruit_NeoPixel::Color(0, 255, 255));
  
  }
void Strip::Get_speed(void)  {
  for(uint8_t i=0;i<8;i++){       ///speed_female 100:...[i00] 80:...[i01]  60:...[i02]
    speed_female[i*100] = pref.getUInt("speed_female["+i*100,speed800[0][i]);  
    speed_female[i*100+1] = pref.getUInt("speed_female["+(i*100+1),speed800[1][i]);
    speed_female[i*100+2] = pref.getUInt("speed_female["+(i*100+2),speed800[2][i]);
  }
for(uint8_t i=0;i<10;i++){
    speed_male[i*100] = pref.getUInt("speed_male["+i*100,speed1000[0][i]);
    speed_male[i*100+1] = pref.getUInt("speed_male["+(i*100+1),speed1000[1][i]);
    speed_male[i*100+2] = pref.getUInt("speed_male["+(i*100+2),speed1000[2][i]);
  }
 
  }

void Strip::RUN800() const {
  uint  position[3]={0};  //position of each 3 leds
  float shift[3]={0};
  uint8_t skip[3] ={0}; //if led is skipping to 0
  for(;;){

    pixels->clear();
    //100 scores 
    if (shift[0] < 800){
      uint8_t c_hdd = shift[0] / 100;     //section locating
      shift[0] += speed_female[c_hdd*100]/fps/10;    //distance  :m
    }
    else
      shift[0] = 800;
    position[0] = shift[0] * 10;
    //80 scores 
    if (shift[1] < 800){
      uint8_t c_hdd = shift[1] / 100;     //section locating
      shift[1] += speed_female[c_hdd*100+1]/fps/10;    //distance  :m
    }
    else
      shift[1] = 800;
    position[1] = shift[1] * 10;
    //60 scores 
    if (shift[2] < 800){
      uint8_t c_hdd = shift[2] / 100;     //section locating
      shift[2] += speed_female[c_hdd*100+2]/fps/10;    //distance  :m
    }
    else
      shift[2] = 800;
    position[2] = shift[2] * 10;
//position message packet
    for(uint8_t i = 0;i<3; i++){    //each position actually
      if(position[i] > 4000){
        position[i] -= 4000;
        if(position[i] < 10) 
          skip[i] = 1 ;
        }
    }
    
for(uint8_t i=0; i<3;i++){      //fill /  if skip
    if(skip[i] == 1){
      pixels->fill(color[i],4000-position[i]);
      pixels->fill(color[i],0,position[i]);
    }
    else
      pixels->fill(color[i], position[i]-length, length);
    }
    pixels->show();
    if(shift[2] == 800 || stop ==1){
      stop=0;
      break;
    }
      
    vTaskDelay((1000/fps-4000*0.03)/portTICK_PERIOD_MS);
  }
  status_char->setValue(StripStatus::STOP);
  status_char->notify();
  shift_char->setValue(shift);
  shift_char->notify();
}

void Strip::RUN1000() const {
  uint  position[3]={0};  //position of each 3 leds
  float shift[3]={0};
  uint8_t skip[3] ={0}; //if led is skipping to 0
  for(;;){
    pixels->clear();
    //100 scores 
    if (shift[0] < 1000){
      uint8_t c_hdd = shift[0] / 100;     //section locating
      shift[0] += speed_male[c_hdd*100]/fps/10;    //distance  :m
    }
    else
      shift[0] = 1000;
    position[0] = shift[0] * 10;
    //80 scores 
    if (shift[1] < 1000){
      uint8_t c_hdd = shift[1] / 100;     //section locating
      shift[1] += speed_male[c_hdd*100+1]/fps/10;    //distance  :m
    }
    else
      shift[1] = 1000;
    position[1] = shift[1] * 10;
    //60 scores 
    if (shift[2] < 1000){
      uint8_t c_hdd = shift[2] / 100;     //section locating
      shift[2] += speed_male[c_hdd*100+2]/fps/10;    //distance  :m
    }
    else
      shift[2] = 1000;
    position[2] = shift[2] * 10;
//position message packet
    for(uint8_t i = 0;i<3; i++){    //each position actually
      if(position[i] > 4000){
        position[i] %= 4000;
        if(position[i] < 10) 
          skip[i] = 1 ;
        }
    }
    
for(uint8_t i=0; i<3;i++){
    if(skip[i] == 1){
      pixels->fill(color[i],4000-position[i]);
      pixels->fill(color[i],0,position[i]);
    }
    else
      pixels->fill(color[i], position[i]-length, length);
    }
    pixels->show();
    if(shift[2] == 1000 || stop ==1){
      stop=0;
      break;
    }
    vTaskDelay((1000/fps-4000*0.03)/portTICK_PERIOD_MS);
  }
  status_char->setValue(StripStatus::STOP);
  status_char->notify();
  shift_char->setValue(shift);
  shift_char->notify();
}

void Strip::stripTask() {
  pixels->clear();
  pixels->show();
  // a delay that will be applied to the end of each loop.
  for (;;) {
    if (pixels != nullptr) {
      
      if (status == StripStatus::RUN800) {
        RUN800();
      } else if (status == StripStatus::RUN1000) {
        RUN1000();
      } else if (status == StripStatus::STOP) {
        pixels->updateLength(4000);
        pixels->clear();
        pixels->show();
        stop =1 ;
      }
    }
    const auto stop_halt_delay = 500;
    if (status == StripStatus::STOP) {
      vTaskDelay(stop_halt_delay / portTICK_PERIOD_MS);
    } else {
      vTaskDelay(halt_delay / portTICK_PERIOD_MS);
    }
    // after a round record the count and notify the client.
    if (pixels != nullptr && shift_char != nullptr && status != StripStatus::STOP) {
      if (count < UINT32_MAX) {
        count++;
      } else {
        count = 0;
      }
      
    }
  }
}

/**
 * @brief sets the maximum number of LEDs that can be used.
 * @warning This function will not set the corresponding bluetooth characteristic value.
 * @param new_max_LEDs
 */
void Strip::setMaxLEDs(int new_max_LEDs) {
  max_LEDs = new_max_LEDs;
  // delete already checks if the pointer is still pointing to a valid memory location.
  // If it is already nullptr, then it does nothing
  // free up the allocated memory from new
  delete pixels;
  // prevent dangling pointer
  pixels = nullptr;
  /**
   * @note Adafruit_NeoPixel::updateLength(uint16_t n) has been deprecated and only for old projects that
   *       may still be calling it. New projects should instead use the
   *       'new' keyword with the first constructor syntax (length, pin,
   *       type).
   */
  pixels = new Adafruit_NeoPixel(max_LEDs, pin, NEO_GRB + NEO_KHZ800);
  pixels->setBrightness(brightness);
  pixels->begin();
}

StripError Strip::initBLE(NimBLEServer *server) {
  if (server == nullptr) {
    return StripError::ERROR;
  }
  if (!is_ble_initialized) {
    service = server->createService(LIGHT_SERVICE_UUID);
    color_char = service->createCharacteristic(LIGHT_CHAR_COLOR_UUID,
                                               NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto color_cb = new ColorCharCallback(*this);
    // https://stackoverflow.com/questions/2182002/convert-big-endian-to-little-endian-in-c-without-using-provided-func
    // well it's actually uint24;
    uint32_t byte1 = ((color[0] >> 16) & 0xff);
    uint32_t byte2 = ((color[0] >> 8) & 0xff) << 8;
    uint32_t byte3 = ((color[0]) & 0xff) << 16;
    uint32_t actual_color =  byte1 | // move byte 2 to byte 0
                             byte2 | // move byte 1 to byte 1
                             byte3; // byte 0 to byte 2 ;

    printf("actual_color: %x\n", actual_color);
    color_char->setValue(actual_color);
    color_char->setCallbacks(color_cb);

    brightness_char = service->createCharacteristic(LIGHT_CHAR_BRIGHTNESS_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto brightness_cb = new BrightnessCharCallback(*this);
    brightness_char->setValue(brightness);
    brightness_char->setCallbacks(brightness_cb);

    delay_char = service->createCharacteristic(LIGHT_CHAR_DELAY_UUID,
                                               NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto delay_cb = new DelayCharCallback(*this);
    delay_char->setValue(delay_ms);
    delay_char->setCallbacks(delay_cb);

    max_leds_char = service->createCharacteristic(LIGHT_CHAR_MAX_LEDS_UUID,
                                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto max_LEDs_cb = new MaxLEDsCharCallback(*this);
    max_leds_char->setValue(max_LEDs);
    max_leds_char->setCallbacks(max_LEDs_cb);

    status_char = service->createCharacteristic(LIGHT_CHAR_STATUS_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto status_cb = new StatusCharCallback(*this);
    status_char->setValue(status);
    status_char->setCallbacks(status_cb);

    speed_char = service->createCharacteristic(LIGHT_CHAR_SPEED_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto speed_cb = new speedCharCallback(*this);
    //speed_char->setValue(speed);
    speed_char->setCallbacks(speed_cb);

    this->shift_char = service->createCharacteristic(LIGHT_CHAR_SHIFT_UUID,
                                                     NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    shift_char->setValue(count);


    halt_delay_char = service->createCharacteristic(LIGHT_CHAR_HALT_DELAY_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto halt_cb = new HaltDelayCharCallback(*this);
    halt_delay_char->setValue(status);
    halt_delay_char->setCallbacks(halt_cb);

    service->start();
    is_ble_initialized = true;
    return StripError::OK;
  } else {
    return StripError::HAS_INITIALIZED;
  }
}

/**
 * @brief Get the instance/pointer of the strip.
 * @return the instance/pointer of the strip.
 */
Strip *Strip::get() {
  static auto *strip = new Strip();
  return strip;
}

/**
 * @brief initialize the strip. this function should only be called once.
 * @param max_LEDs
 * @param PIN the pin of strip, default is 14.
 * @param color the default color of the strip. default is Cyan (0x00FF00).
 * @param brightness the default brightness of the strip. default is 32.
 * @return StripError::OK if the strip is not inited, otherwise StripError::HAS_INITIALIZED.
 */
StripError Strip::begin(int max_LEDs, int16_t PIN, uint8_t brightness) {
  if (!is_initialized) {
    pref.begin("record", false);
    this->max_LEDs = max_LEDs;
    this->pin = PIN;
    this->brightness = brightness;
    Get_color();
    Get_speed();
    pixels = new Adafruit_NeoPixel(max_LEDs, PIN, NEO_GRB + NEO_KHZ800);
    pixels->begin();
    pixels->setBrightness(brightness);
    is_initialized = true;
    return StripError::OK;
  } else {
    return StripError::HAS_INITIALIZED;
  }
}

/**
 * @brief set the color of the strip.
 * @warning This function will not set the corresponding bluetooth characteristic value.
 * @param color the color of the strip.
 */
void Strip::setBrightness(const uint8_t new_brightness) {
  if (pixels != nullptr) {
    brightness = new_brightness;
    pixels->setBrightness(brightness);
  }
}
