//
// Created by Kurosu Chan on 2022/7/13.
//

#include "StripCommon.h"
#include "Strip.h"


RunState nextState(RunState state, const ValueRetriever<float> &retriever, int totalLength, float fps) {
  uint32_t position; // late
  float shift; // late
  float speed = retriever.retrieve(static_cast<int>(state.shift));
  bool skip = state.isSkip;
  //distance unit is `meter`
  if (state.shift < totalLength)
    shift = speed / fps / 100;
  else {
    shift = totalLength;
  }
  position = shift * 10;
  if (position > 4000) {
    position %= 4000;
    if (position < 10)
      skip = true;
  }
  return RunState{
      position,
      speed,
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
  auto [position, speed, shift, skip] = next;
  this->state = next;
  if (skip) {
    pixels->fill(color, 4000 - position);
    pixels->fill(color, 0, position);
  } else {
    pixels->fill(color, position - trackLength, trackLength);
  }
}

void Strip::run(Track *begin, Track *end) {
  // use the max value of the 0 track to determine the length of the track.
  auto keys = begin->retriever.getKeys();
  auto l = begin->retriever.getMaxKey();
  int totalLength = 100 * l;
  std::for_each(begin, end, [](Track &track) {
    track.resetState();
  });

  struct TimerParam {
    std::vector<Track> *tracks;
    NimBLECharacteristic *state_char;
  };
  auto param = TimerParam{&tracks, state_char};

  auto timer_cb = [](TimerHandle_t xTimer) {
    auto param = *(TimerParam *) pvTimerGetTimerID(xTimer);
    auto &tracks = *param.tracks;
    auto ble_char = param.state_char;
    TrackStates states = TrackStates_init_zero;
    auto callbacks = PbCallback<std::vector<Track> *>{
        &tracks,
        [](pb_ostream_t *stream, const pb_field_t *field,
           std::vector<Track> *const *arg) -> bool {
          auto tracks = *arg;
          for (auto &track: *tracks) {
            auto state = track.state;
            auto [position, speed, shift, skip] = state;
            auto state_encode = TrackState{
                .id = track.id,
                .shift = shift,
                .speed = speed,
            };
            if (!pb_encode_tag_for_field(stream, field)) {
              return false;
            }
            if (!pb_encode_submessage(stream, TrackState_fields, &state_encode)) {
              return false;
            }
          }
          return true;
        }
    };
    uint8_t buf[256];
    pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
    states.states.arg = callbacks.arg_to_void();
    states.states.funcs.encode = callbacks.func_to_void();
    bool res = pb_encode(&stream, TrackStates_fields, &states);
    if (!res) {
      // do nothing. You can't use printf/log in the timer callback.
    } else {
      ble_char->setValue(buf, stream.bytes_written);
      ble_char->notify();
    }
  };
  // encode and send the states to the characteristic
  // 1 second send one message.
  auto timer = xTimerCreate("timer", 1000 / portTICK_PERIOD_MS, pdTRUE, reinterpret_cast<void *>(&param), timer_cb);
  xTimerStart(timer, 0);

  while (status != StripStatus::STOP) {
    pixels->clear();
    // pointer can be captured by value
    std::for_each(begin, end, [=](Track &track) {
      track.updateStrip(pixels, totalLength, l, fps);
    });
    pixels->show();
    // TODO: add priority to the tracks and then sort the states.
    // We can use ID
    // first should be the fastest one, but we want to wait the slowest one to stop.
    if (end->state.shift >= totalLength) {
      this->status = StripStatus::STOP;
    }
    // compile time calculation
    // 46 ms per frame? You must be kidding me. THAT'S TOO FAST.
    constexpr auto delay = (1000 / fps - 4000 * 0.03) / portTICK_PERIOD_MS;
    vTaskDelay(delay);
  }
  xTimerStop(timer, portMAX_DELAY);
  status_char->setValue(StripStatus::STOP);
  status_char->notify();
}

void Strip::run(std::vector<Track> &tracks) {
  // https://stackoverflow.com/questions/23316368/converting-iterator-to-pointer-really-it
  // https://iris.artins.org/software/converting-an-stl-vector-iterator-to-a-raw-pointer/
  this->run(&*tracks.begin(), &*tracks.end());
}

void Strip::stripTask() {
  pixels->clear();
  pixels->show();
  // a delay that will be applied to the end of each loop.
  for (;;) {
    if (pixels != nullptr) {
      if (status == StripStatus::RUN) {
        run(tracks);
      } else if (status == StripStatus::STOP) {
        pixels->clear();
        pixels->show();
        // 500 ms halt delay
        constexpr auto delay = 500 / portTICK_PERIOD_MS;
        vTaskDelay(delay);
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

    status_char = service->createCharacteristic(LIGHT_CHAR_STATUS_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                                                NIMBLE_PROPERTY::NOTIFY);
    auto status_cb = new StatusCharCallback(*this);
    status_char->setValue(status);
    status_char->setCallbacks(status_cb);

    config_char = service->createCharacteristic(LIGHT_CHAR_CONFIG_UUID,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto config_cb = new ConfigCharCallback(*this);
    config_char->setCallbacks(config_cb);
    // READ ONLY
    state_char = service->createCharacteristic(LIGHT_CHAR_STATE_UUID,
                                               NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

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
    pref.begin("record", false);
    tracks.clear();
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
