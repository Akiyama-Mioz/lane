//
// Created by Kurosu Chan on 2022/7/13.
//

#ifndef HELLO_WORLD_STRIP_H
#define HELLO_WORLD_STRIP_H

#include "utils.h"
#include <Adafruit_NeoPixel.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NimBLEDevice.h"
#include "Preferences.h"
#include "map"
#include "vector"

// change this to match the length of StripStatus
constexpr uint8_t StripStatus_LENGTH = 4;
enum class StripStatus {
  STOP = 0,
  NORMAL,
  CUSTOM
};

enum class StripError {
  OK = 0,
  ERROR,
  HAS_INITIALIZED,
};

struct TrackState {
  uint32_t position;
  float shift;
  bool isSkip;
};

TrackState nextState(TrackState state, const ValueRetriever<float> &retriever, int totalLength, float fps);

class Track {
public:
  const char *speed_uuid;
  const char *shift_uuid;
  NimBLECharacteristic *shift_char = nullptr;
  NimBLECharacteristic *speed_char = nullptr;
  TrackState state = TrackState{0, 0, false};
  ValueRetriever<float> retriever = ValueRetriever<float>(std::map<int, float>());
  uint32_t color = Adafruit_NeoPixel::Color(255, 255, 255);
  void resetState() {
    this->state = TrackState{0, 0, false};
  }
  void updateStrip(Adafruit_NeoPixel *pixels, int totalLength, int trackLength, float fps);
};

class Strip {
protected:
  bool is_initialized = false;
  bool is_ble_initialized = false;
public:
  float fps = 6; //distance travelled ! max = 7 !
  Preferences pref;
  int pin = 14;
  // 10 LEDs/m for 24v
  int max_LEDs = 4000;
  // length should be less than max_LEDs
  uint16_t length = 10;
  uint32_t count = 0;
  uint8_t brightness = 32;
  Adafruit_NeoPixel *pixels = nullptr;
  StripStatus status = StripStatus::STOP;
  int delay_ms = 100;
  int halt_delay = 500;

  // I don't know how to release the memory of the NimBLECharacteristic
  // or other BLE stuff. So I choose to not free the memory. (the device
  // should be always alive with BLE anyway.)
  NimBLECharacteristic *status_char = nullptr;
  NimBLECharacteristic *brightness_char = nullptr;
  NimBLECharacteristic *max_LEDs_char = nullptr;
  NimBLECharacteristic *delay_char = nullptr;
  NimBLECharacteristic *halt_delay_char = nullptr;
  NimBLECharacteristic *speed_custom_char = nullptr;

  NimBLEService *service = nullptr;
  const char *LIGHT_SERVICE_UUID = "15ce51cd-7f34-4a66-9187-37d30d3a1464";
  const char *LIGHT_CHAR_BRIGHTNESS_UUID = "e3ce8b08-4bb9-4696-b862-3e62a1100adc";
  const char *LIGHT_CHAR_STATUS_UUID = "24207642-0d98-40cd-84bb-910008579114";
  const char *LIGHT_CHAR_DELAY_UUID = "adbbac7f-2c08-4a8d-b3f7-d38d7bd5bc41";
  const char *LIGHT_CHAR_HALT_DELAY_UUID = "00923454-81de-4e74-b2e0-a873f2cbddcc";

  const char *LIGHT_CHAR_SPEED0_UUID = "e89cf8f0-7b7e-4a2e-85f4-85c814ab5cab";
  const char *LIGHT_CHAR_SPEED1_UUID = "ed3eefa1-3c80-b43f-6b65-e652374650b5";
  const char *LIGHT_CHAR_SPEED2_UUID = "765961ad-e273-ddda-ce56-25a46e3017f9";
  const char *LIGHT_CHAR_SPEED_CUSTOM_UUID = "d00cdf29-0113-4523-a5b9-a192f0e20201";
  const char *LIGHT_CHAR_SHIFT0_UUID = "97157804-329c-49cb-a9d9-0f35ea15d985";
  const char *LIGHT_CHAR_SHIFT1_UUID = "7149b3d4-d239-4aa1-8093-3589ace8b63c";
  const char *LIGHT_CHAR_SHIFT2_UUID = "25be10f2-82af-4cfc-a56b-61ab055da11a";
  const char *LIGHT_CHAR_SHIFT_CUSTOM_UUID = "33556c07-817c-4006-b2c7-7394dede7a1c";

  std::array<Track, 3> normal_tracks;
  std::array<Track, 1> custom_tracks;

  /**
   * @brief Loop the strip.
   * @warning This function will never return and you should call this in creatTask/Thread
   *          or something equivalent in RTOS.
   * @param void void
   * @return No return
   */
  [[noreturn]]
  void stripTask();

  StripError initBLE(NimBLEServer *server);

  void setMaxLEDs(int new_max_LEDs);

  void setBrightness(uint8_t new_brightness);

  void runCustom();

  void runNormal();

  static Strip *get();

  Strip(Strip const &) = delete;

  Strip &operator=(Strip const &) = delete;

  Strip(Strip &&) = delete;

  Strip &operator=(Strip &&) = delete;

  // usually you won't destruct it because it's running in MCU and the resource will be released
  ~Strip() = delete;

  StripError
  begin(int max_LEDs, int16_t PIN, uint8_t brightness = 32);

protected:
  Strip() = default;

  void run(Track *tracksBegin, Track *tracksEnd);
  void run(std::vector<Track> &tracks);
};


#endif //HELLO_WORLD_STRIP_H
