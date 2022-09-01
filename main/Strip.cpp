//
// Created by Kurosu Chan on 2022/7/13.
//

#include "StripCommon.h"
#include "Strip.h"


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

/**
 * @brief update the strip with the new state and write the pixel data to the strip.
 * finally, write the new state to the characteristic and then notify.
 * @param pixels
 * @param totalLength
 * @param trackLength
 * @param fps
 */
void Track::updateStrip(Adafruit_NeoPixel *pixels, int totalLength, int trackLength, float fps) {
  auto next = nextState(state, retriever, totalLength, fps);
  this->state = next;
  if (state.isSkip) {
    pixels->fill(color, 4000 - state.position);
    pixels->fill(color, 0, state.position);
  } else {
    pixels->fill(color, state.position - trackLength, trackLength);
  }
  shift_char->setValue(state.shift);
  shift_char->notify();
}

void Strip::run(Track *tracksBegin, Track *tracksEnd) {
  // use the max value of the 0 track to determine the length of the track.
  auto keys = tracksBegin->retriever.getKeys();
  auto l = *std::max_element(keys.begin(), keys.end());
  int totalLength = 100 * l;
  std::for_each(tracksBegin, tracksEnd, [](Track &track) {
    track.resetState();
  });
  while (status != StripStatus::STOP) {
    pixels->clear();
    // pointer can be captured by value
    std::for_each(tracksBegin, tracksEnd, [=](Track &track) {
      track.updateStrip(pixels, totalLength, l, fps);
    });
    pixels->show();
    // 0 should be the fastest one, but we want to wait the slowest one to stop.
    if (tracksEnd->state.shift >= totalLength) {
      this->status = StripStatus::STOP;
    }
    vTaskDelay((1000 / fps - 4000 * 0.03) / portTICK_PERIOD_MS);
  }
  status_char->setValue(StripStatus::STOP);
  status_char->notify();
}

void Strip::run(std::vector<Track> &tracks) {
  // https://stackoverflow.com/questions/23316368/converting-iterator-to-pointer-really-it
  // https://iris.artins.org/software/converting-an-stl-vector-iterator-to-a-raw-pointer/
  this->run(&*tracks.begin(), &*tracks.end());
}

void Strip::runNormal() {
  run(normal_tracks.begin(), normal_tracks.end());
}

void Strip::runCustom() {
  run(custom_tracks.begin(), custom_tracks.end());
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

    status_char = service->createCharacteristic(LIGHT_CHAR_STATUS_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto status_cb = new StatusCharCallback(*this);
    status_char->setValue(status);
    status_char->setCallbacks(status_cb);

    // TODO: find a way to make it less ugly
    for (auto &track: normal_tracks) {
      track.speed_char = service->createCharacteristic(track.speed_uuid,
                                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
      // shift has read only Characteristic. no callback (read only)
      track.shift_char = service->createCharacteristic(track.shift_uuid,
                                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
      auto speed_cb = new SpeedCharCallback(*this, track);
      track.speed_char->setValue(0);
      track.speed_char->setCallbacks(speed_cb);
    }

    for (auto &track: custom_tracks) {
      track.speed_char = service->createCharacteristic(track.speed_uuid,
                                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
      // shift has read only Characteristic. no callback (read only)
      track.shift_char = service->createCharacteristic(track.shift_uuid,
                                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
      auto speed_cb = new SpeedCharCallback(*this, track);
      track.speed_char->setValue(0);
      track.speed_char->setCallbacks(speed_cb);
    }

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
  // GNU old-style field designator extension
  if (!is_initialized) {
    this->normal_tracks = std::array<Track, 3>{
        Track{
            .speed_uuid =  LIGHT_CHAR_SPEED0_UUID,
            .shift_uuid =  LIGHT_CHAR_SHIFT0_UUID,
            .color =  Adafruit_NeoPixel::Color(255, 0, 0),
        },
        Track{
            .speed_uuid =  LIGHT_CHAR_SPEED1_UUID,
            .shift_uuid =  LIGHT_CHAR_SHIFT1_UUID,
            .color =  Adafruit_NeoPixel::Color(0, 255, 0),
        },
        Track{
            .speed_uuid =  LIGHT_CHAR_SPEED2_UUID,
            .shift_uuid =  LIGHT_CHAR_SHIFT2_UUID,
            .color =  Adafruit_NeoPixel::Color(0, 0, 255),
        },
    };
    this->custom_tracks = std::array<Track, 1>{
        Track{
            .speed_uuid =  LIGHT_CHAR_SPEED_CUSTOM_UUID,
            .shift_uuid =  LIGHT_CHAR_SHIFT_CUSTOM_UUID,
            .color =  Adafruit_NeoPixel::Color(255, 255, 0),
        }
    };
    pref.begin("record", false);
    this->max_LEDs = max_LEDs;
    this->pin = PIN;
    this->brightness = brightness;
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
