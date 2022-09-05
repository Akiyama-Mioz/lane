//
// Created by Kurosu Chan on 2022/7/13.
//

#include "StripCommon.h"

// a LED count is 0.1m
inline int meterToLEDsCount(float meter) {
  return round(meter * LEDs_PER_METER);
}

inline float LEDsCountToMeter(int count) {
  return count / LEDs_PER_METER;
}

/**
 * @brief get the next state. should be a pure function.
 * @param state the current state.
 * @param retriever
 * @param fps frames per second
 * @return the next state.
 */
inline RunState nextState(RunState state, const ValueRetriever<float> &retriever, int ledCounts, float fps) {
  float position; // late
  float shift = state.shift;
  float speed = retriever.retrieve(static_cast<int>(state.shift));
  bool skip = state.isSkip;
  //distance unit is `meter`
  // total length will not be considered here.
  shift = shift + speed / fps; // speed per frame in m/s
  position = shift;
  if (position > CIRCLE_LENGTH) {
    position = fmod(position, CIRCLE_LENGTH);
    if (position < LEDsCountToMeter(ledCounts)) {
      skip = true;
    } else {
      skip = false;
    }
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
 * @param pixels
 * @param ledCounts - counts of the number of LEDs in one track.
 * @param fps
 */
inline RunState Track::updateStrip(Adafruit_NeoPixel *pixels, int ledCounts, float fps) {
  auto next = nextState(state, retriever, ledCounts, fps);
  auto [position, speed, shift, skip] = next;
  auto maxLength = getMaxLength();
  // if the shift is larger than the max length, we should stop updating the state.
  // do an early return.
  if (floor(state.shift) >= maxLength) {
    return state;
  } else {
    this->state = next;
  }
  if (skip) {
    // fill to the end
    pixels->fill(color, meterToLEDsCount(CIRCLE_LENGTH - position));
    pixels->fill(color, 0, meterToLEDsCount(position));
  } else {
    pixels->fill(color, meterToLEDsCount(position - LEDsCountToMeter(ledCounts)), ledCounts);
  }
  return next;
}

void Strip::run(std::vector<Track> &tracks) {
  if (tracks.empty()) {
    ESP_LOGE("Strip::run", "no track to run");
    setStatus(StripStatus::STOP);
    return;
  }
  // use the max value of the 0 track to determine the length of the track.
  auto keys = tracks.front().retriever.getKeys();
  int totalLength = tracks.front().retriever.getMaxKey();
  ESP_LOGI("Strip::run", "total length is %d m", totalLength);
  for (auto &track: tracks) {
    track.resetState();
  }

  struct TimerParam {
    std::vector<Track> *tracks;
    NimBLECharacteristic *state_char;
  };
  auto param = TimerParam{&tracks, state_char};
  auto timer_cb = [](TimerHandle_t xTimer) {
    auto param = *static_cast<TimerParam *>(pvTimerGetTimerID(xTimer));
    auto &tracks = *param.tracks;
    auto ble_char = param.state_char;
    TrackStates states = TrackStates_init_zero;
    auto callbacks = PbCallback<std::vector<Track> *>{
        &tracks,
        [](pb_ostream_t *stream, const pb_field_t *field,
           std::vector<Track> *const *arg) -> bool {
          auto &tracks = **arg;
          for (auto &track: tracks) {
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
    uint8_t buf[128];
    pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
    states.states.arg = callbacks.arg_to_void();
    states.states.funcs.encode = callbacks.func_to_void();
    bool success = pb_encode(&stream, TrackStates_fields, &states);
    if (!success) {
      // do nothing.
    } else {
      // end() is not the same as back()
      ESP_LOGD("Strip::run::callback", "Last track shift: %f", tracks.back().state.shift);
      ble_char->setValue(buf, stream.bytes_written);
      ble_char->notify();
    }
  };
  // encode and send the states to the characteristic
  // 1 second send one message.
  auto timer = xTimerCreate("timer", pdMS_TO_TICKS(TRANSMIT_INTERVAL), pdTRUE, static_cast<void *>(&param),
                            timer_cb);
  xTimerStart(timer, 0);

  ESP_LOGD("Strip::run", "enter loop");
  while (status != StripStatus::STOP) {
    pixels->clear();
    for (auto &track: tracks) {
      auto next = track.updateStrip(pixels, countLEDs, fps);
      auto [position, speed, shift, skip] = next;
      ESP_LOGV("Strip::run::loop", "track: %d, position: %.2f, speed: %.1f, shift: %.2f", track.id, position, speed,
               shift);
    }
    pixels->show();
    // https://stackoverflow.com/questions/44831793/what-is-the-difference-between-vector-back-and-vector-end
    if (ceil(tracks.back().state.shift) >= totalLength) {
      ESP_LOGI("Strip::run", "Run finished last shift %f", tracks.back().state.shift);
      setStatus(StripStatus::STOP);
    }
    // TODO: no magic number
    // 46 ms per frame?
    constexpr uint delay = pdMS_TO_TICKS(1000 / fps - 4000 * 0.03);
    vTaskDelay(delay);
  }
  ESP_LOGD("Strip::run", "exit loop");
  // promise the last state is sent.
  vTaskDelay(pdMS_TO_TICKS(TRANSMIT_INTERVAL));
  xTimerStop(timer, portMAX_DELAY);
}

void Strip::stripTask() {
  pixels->clear();
  pixels->show();
  for (;;) {
    if (pixels != nullptr) {
      if (status == StripStatus::RUN) {
        run(tracks);
      } else if (status == StripStatus::STOP) {
        pixels->clear();
        pixels->show();
        constexpr uint delay = pdMS_TO_TICKS(HALT_INTERVAL);
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
   *       type)
   */
  pixels = new Adafruit_NeoPixel(max_LEDs, pin, pixelType);
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
StripError Strip::begin(int16_t PIN, uint8_t brightness) {
  if (!is_initialized) {
    pref.begin("record", false);
    tracks.clear();
    // reserve some space for the tracks
    // avoid the reallocation of the vector
    tracks.reserve(5);
    this->max_LEDs = meterToLEDsCount(CIRCLE_LENGTH);
    this->pin = PIN;
    this->brightness = brightness;
    pixels = new Adafruit_NeoPixel(max_LEDs, PIN, pixelType);
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

void Strip::setStatus(StripStatus status) {
  this->status = status;
  if (status_char != nullptr) {
    status_char->setValue(status);
    status_char->notify();
  }
}
