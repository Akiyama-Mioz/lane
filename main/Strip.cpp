//
// Created by Kurosu Chan on 2022/7/13.
//

#include "StripCommon.h"

void Strip::getColor(void) {
  color[0] = pref.getUInt("color0", Adafruit_NeoPixel::Color(255, 0, 255));
  color[1] = pref.getUInt("color1", Adafruit_NeoPixel::Color(255, 255, 0));
  color[2] = pref.getUInt("color2", Adafruit_NeoPixel::Color(0, 255, 255));
}

struct TrackState {
  uint32_t position;
  float shift;
  bool isSkip;
};

TrackState nextState(TrackState state, const ValueRetriever<float> &retriever, int totalLength, float fps) {
  uint32_t position; // late
  float shift; // late
  bool skip = state.isSkip;
  //distance unit is `meter`
  if (state.shift < totalLength)
    shift = retriever.retrieve(static_cast<int>(state.shift)) / fps / 100;
  else {
    shift = totalLength;
  }
  position = shift * 10;
  if (position > 4000) {
    position %= 4000;
    if (position < 10)
      skip = true;
  }
  return TrackState{
      position,
      shift,
      skip
  };
}

void Strip::runNormal() {
  auto states = std::vector(3, TrackState{0, 0, false});
  // use the max value of the 0 track to determine the length of the track.
  auto l = *std::max_element(speedVals[0].getKeys().begin(), speedVals[0].getKeys().end());
  int totalLength = 100 * l;
  while (status != StripStatus::STOP) {
    std::vector<TrackState> newStates = {};
    pixels->clear();
    // TODO: use zip instead of ugly indexing
    int idx = 0;
    std::transform(states.begin(), states.end(), std::back_inserter(newStates),
                   [=, &idx](const TrackState &state) {
                     auto next = nextState(state, speedVals[idx], totalLength, fps);
                     idx = (idx + 1) % 3;
                     return next;
                   });
    // reset index
    idx = 0;
    for (auto state: newStates){
      if (state.isSkip) {
        pixels->fill(color[idx], 4000 - state.position);
        pixels->fill(color[idx], 0, state.position);
      } else {
        pixels->fill(color[idx], state.position - length, length);
      }
      idx = (idx + 1) % 3;
    }
    pixels->show();
    // 0 should be the fastest one but we want to wait the slowest one to stop.
    if (states[2].shift == totalLength || this->status == StripStatus::STOP) {
      this->status = StripStatus::STOP;
      break;
    }
    states = newStates;
    shift_char->setValue(states[0].shift);
    shift_char->notify();
    vTaskDelay((1000 / fps - 4000 * 0.03) / portTICK_PERIOD_MS);
  }
  status_char->setValue(StripStatus::STOP);
  status_char->notify();
}

void Strip::runCustom() {
  auto state = TrackState{0, 0, false};
  auto l = *std::max_element(speedCustom.getKeys().begin(), speedCustom.getKeys().end());
  int totalLength = 100 * l;

  while (status != StripStatus::STOP) {
    auto [position, shift, skip] = nextState(state, speedCustom, totalLength, fps);
    pixels->clear();

    if (state.isSkip) {
      pixels->fill(color[1], 4000 - position);
      pixels->fill(color[1], 0, position);
    } else {
      pixels->fill(color[1], position - length, length);
    }

    pixels->show();
    shift_char->setValue(shift);
    shift_char->notify();

    if (shift == totalLength || this->status == StripStatus::STOP) {
      this->status = StripStatus::STOP;
      break;
    }
    vTaskDelay((1000 / fps - 4000 * 0.03) / portTICK_PERIOD_MS);
  }
  status_char->setValue(StripStatus::STOP);
  status_char->notify();
}


void Strip::stripTask() {
  pixels->clear();
  pixels->show();
  // a delay that will be applied to the end of each loop.
  for (;;) {
    if (pixels != nullptr) {
      if (status == StripStatus::NORMAL) {
        runNormal();
      } else if (status == StripStatus::CUSTOM) {
        runCustom();
      } else if (status == StripStatus::STOP) {
        pixels->updateLength(4000);
        pixels->clear();
        pixels->show();
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
    uint32_t actual_color = byte1 | // move byte 2 to byte 0
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

    max_LEDs_char = service->createCharacteristic(LIGHT_CHAR_MAX_LEDs_UUID,
                                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto max_LEDs_cb = new MaxLEDsCharCallback(*this);
    max_LEDs_char->setValue(max_LEDs);
    max_LEDs_char->setCallbacks(max_LEDs_cb);

    status_char = service->createCharacteristic(LIGHT_CHAR_STATUS_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto status_cb = new StatusCharCallback(*this);
    status_char->setValue(status);
    status_char->setCallbacks(status_cb);

    speed_custom_char = service->createCharacteristic(LIGHT_CHAR_SPEED_CUSTOM_UUID,
                                                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto speed_custom_cb = new SpeedCustomCharCallback(*this);
    speed_custom_char->setValue(0);
    speed_custom_char->setCallbacks(speed_custom_cb);

    speed0_char = service->createCharacteristic(LIGHT_CHAR_SPEED0_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto speed0_cb = new SpeedCharCallback(*this, 0);
    speed0_char->setValue(0);
    speed0_char->setCallbacks(speed0_cb);

    speed1_char = service->createCharacteristic(LIGHT_CHAR_SPEED1_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto speed1_cb = new SpeedCharCallback(*this, 1);
    speed1_char->setValue(0);
    speed1_char->setCallbacks(speed1_cb);

    speed2_char = service->createCharacteristic(LIGHT_CHAR_SPEED2_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto speed2_cb = new SpeedCharCallback(*this, 2);
    speed2_char->setValue(0);
    speed2_char->setCallbacks(speed2_cb);

//*********shift has only Characteristic.no callback func(read only)
    this->shift_char = service->createCharacteristic(LIGHT_CHAR_SHIFT_UUID,
                                                     NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);


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
    getColor(); //get pref.color
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
