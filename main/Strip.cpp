//
// Created by Kurosu Chan on 2022/7/13.
//

#include "StripCommon.h"
#include "Strip.h"

auto esp_random_binary() {
  auto n = esp_random();
  auto cond = UINT32_MAX / 2;
  if (n > cond) {
    return 1;
  } else {
    return 0;
  }
}

auto esp_random_ternary() {
  auto n = esp_random();
  auto cond = UINT32_MAX / 3;
  if (n > cond * 2) {
    return 2;
  } else if (n > cond) {
    return 1;
  } else {
    return 0;
  }
}

static inline int meterToLEDsCount(float meter, float LEDs_per_meter) {
  return abs(round(meter * LEDs_per_meter)) + 1;
}

static inline float LEDsCountToMeter(uint32_t count, float LEDs_per_meter) {
  if (count <= 1) {
    return 0;
  } else {
    return (count - 1) / LEDs_per_meter;
  }
}

/**
 * @brief get the next state. should be a pure function.
 * @param state the current state.
 * @param retriever
 * @param fps frames per second
 * @return the next state.
 */
inline RunState
nextState(RunState state, const ValueRetriever<float> &retriever, float circleLength, float trackLength, float fps) {
  float position; // late
  float shift = state.shift;
  float speed = retriever.retrieve(static_cast<int>(state.shift));
  float extra = 0; // late
  // distance unit is `meter`
  // total length will not be considered here.
  shift = shift + speed / fps; // speed per frame in m/s
  position = shift;
  if (position > circleLength) {
    position = fmod(position, circleLength);
    if (position <= trackLength) {
      extra = trackLength - position;
    }
  }
  return RunState{
      position,
      speed,
      shift,
      extra
  };
}

/**
 * @brief update the strip with the new state and write the pixel data to the strip.
 * @param pixels
 * @param fps
 */
inline RunState Track::updateStrip(Adafruit_NeoPixel *pixels, float circle_length, float track_length, float fps,
                                   float LEDs_per_meter) {
  auto max_shift = getMaxShiftLength();
  // if the shift is larger than the max length, we should stop updating the state.
  // do an early return.
  if (floor(state.shift) >= max_shift) {
    state.speed = 0;
    return state;
  } else {
    auto next = nextState(state, retriever, circle_length, track_length, fps);
    auto [position, speed, shift, extra] = next;
    this->state = next;
    if (extra != 0) {
      // position may be larger than trackLength if float is not precise enough.
      if (circle_length < position) {
        // fill to the end
        pixels->fill(color, meterToLEDsCount(circle_length - extra, LEDs_per_meter), 0);
      }
      pixels->fill(color, 0, meterToLEDsCount(position, LEDs_per_meter));
    } else {
      pixels->fill(color, meterToLEDsCount(position - track_length, LEDs_per_meter),
                   meterToLEDsCount(track_length, LEDs_per_meter));
    }
    return next;
  }
}

void Strip::run(std::vector<Track> &tracks) {
  if (tracks.empty()) {
    ESP_LOGE("Strip::run", "no track to run");
    setStatusNotify(TrackStatus_STOP);
    return;
  }
  // use the max value of the 0 track to determine the length of the track.
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
    auto callbacks = PbEncodeCallback<std::vector<Track> *>{
        &tracks,
        [](pb_ostream_t *stream, const pb_field_t *field,
           std::vector<Track> *const *arg) -> bool {
          auto &tracks = **arg;
          for (auto &track: tracks) {
            auto state = track.state;
            auto [position, speed, shift, extra] = state;
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
      ble_char->setValue(buf, stream.bytes_written);
      ble_char->notify();
    }
  };
  // encode and send the states to the characteristic
  auto timer = xTimerCreate("sendStatus", pdMS_TO_TICKS(BLUE_TRANSMIT_INTERVAL_MS), pdTRUE, static_cast<void *>(&param),
                            timer_cb);
  xTimerStart(timer, 0);
  // should not change any parameter when running
  auto circleLength = static_cast<float>(this->circle_length);
  auto trackLength = LEDsCountToMeter(count_LEDs, this->getLEDsPerMeter());
  ESP_LOGD("Strip::run", "enter loop");
  while (status == TrackStatus_RUN) {
    auto startTime = Instant();
    pixels->clear();
    for (auto &track: tracks) {
      auto next = track.updateStrip(pixels, circleLength, trackLength, FPS, this->getLEDsPerMeter());
      auto [position, speed, shift, extra] = next;
      ESP_LOGV("Strip::run::loop", "track: %d, position: %.2f, speed: %.1f, shift: %.2f", track.id, position, speed,
               shift);
    }
    pixels->show();
    // https://stackoverflow.com/questions/44831793/what-is-the-difference-between-vector-back-and-vector-end
    if (ceil(tracks.back().state.shift) >= totalLength) {
      ESP_LOGI("Strip::run", "Run finished last shift %f", tracks.back().state.shift);
      setStatusNotify(TrackStatus_STOP);
    }
    // I expect the delay to be 1/fps seconds.
    // Remember that the Adafruit_NeoPixel::show()
    // will be blocking for LED_DELAY_TIME_MS milliseconds for ONE LED which could be huge for a large amount LEDs.
    // so the actual delay we have to wait is (expectedDelay - LED_DELAY_TIME_MS * countLEDs).
    //
    //    "There's no easy fix for this, but a few specialized alternative or companion libraries exist that use
    //     very device-specific peripherals to work around it."
    //
    constexpr long long MILLI = 1000;
    auto secondsToMillis = [](uint seconds) {
      return seconds * MILLI;
    };
    auto millisToSeconds = [](long long millis) {
      return millis / static_cast<float>(MILLI);
    };
    constexpr long long expectedDelay = secondsToMillis(1) / FPS;
    auto elapsed = startTime.elapsed();
    auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    auto lucky = esp_random_ternary();
    auto diff = (expectedDelay - elapsedMillis) - lucky;
    if (diff > 0) {
      // remove 1ms randomly
      vTaskDelay(pdMS_TO_TICKS(diff));
    } else {
      ESP_LOGE("Strip::run", "Loop timeout %.2f", millisToSeconds(elapsedMillis));
    }
  }
  ESP_LOGD("Strip::run", "exit loop");
  // promise the last state is sent.
  vTaskDelay(2 * pdMS_TO_TICKS(BLUE_TRANSMIT_INTERVAL_MS));
  xTimerStop(timer, portMAX_DELAY);
}

void Strip::stripTask() {
  pixels->clear();
  pixels->show();
  for (;;) {
    if (pixels != nullptr) {
      if (status == TrackStatus_RUN) {
        run(tracks);
      } else if (status == TrackStatus_STOP) {
        pixels->clear();
        pixels->show();
        constexpr uint delay = pdMS_TO_TICKS(HALT_INTERVAL);
        vTaskDelay(delay);
      }
    }
  }
}

/// i.e. Circle LEDs
void Strip::setMaxLEDs(uint32_t new_max_LEDs) {
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
  pixels = new Adafruit_NeoPixel(max_LEDs, pin, pixel_type);
  pixels->setBrightness(brightness);
  pixels->begin();
}

StripError Strip::initBLE(NimBLEServer *server) {
  if (server == nullptr) {
    return StripError::ERROR;
  }
  if (!is_ble_initialized) {
    service = server->createService(LIGHT_SERVICE_UUID);

    // TODO: use macro or template to generate the code
    // Or maybe use function
    brightness_char = service->createCharacteristic(LIGHT_CHAR_BRIGHTNESS_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto brightness_cb = new BrightnessCharCallback(*this);
    brightness_char->setValue(brightness);
    brightness_char->setCallbacks(brightness_cb);

    options_char = service->createCharacteristic(LIGHT_CHAR_OPTIONS_CHAR,
                                                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    auto options_cb = new OptionsCharCallback(*this);
    options_char->setCallbacks(options_cb);

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
 * @param PIN the pin of strip, default is 14.
 * @param brightness the default brightness of the strip. default is 32.
 * @return StripError::OK if the strip is not inited, otherwise StripError::HAS_INITIALIZED.
 */
StripError Strip::begin(int16_t PIN, uint8_t brightness) {
  if (!is_initialized) {
    pref.begin(STRIP_PREF_RECORD_NAME, false);
    tracks.clear();
    // reserve some space for the tracks
    // avoid the reallocation of the vector
    tracks.reserve(5);
    this->pin = PIN;
    this->brightness = brightness;
    pixels = new Adafruit_NeoPixel(max_LEDs, PIN, pixel_type);
    pixels->begin();
    pixels->setBrightness(brightness);
    is_initialized = true;
    return StripError::OK;
  } else {
    return StripError::HAS_INITIALIZED;
  }
}

void Strip::setBrightness(const uint8_t new_brightness) {
  if (pixels != nullptr) {
    brightness = new_brightness;
    pixels->setBrightness(brightness);
  }
}

void Strip::setStatusNotify(TrackStatus s) {
  this->status = s;
  if (status_char != nullptr) {
    constexpr size_t buf_size = 64;
    std::array<uint8_t, buf_size> buf{};
    TrackStatusMsg msg = TrackStatusMsg_init_zero;
    msg.status = s;
    auto ostream = pb_ostream_from_buffer(buf.data(), buf_size);
    auto res = pb_encode(&ostream, TrackStatusMsg_fields, &msg);
    if (!res) {
      ESP_LOGE("Strip::setStatusNotify", "pb_encode");
      return;
    }
    status_char->setValue(buf);
    status_char->notify();
  }
}

void Strip::setCountLEDs(uint32_t count) {
  this->count_LEDs = count;
}

void Strip::setCircleLength(float meter) {
  circle_length = meter;
}

float Strip::getLEDsPerMeter() const {
  return static_cast<float>(max_LEDs) / static_cast<float>(this->circle_length);
}
