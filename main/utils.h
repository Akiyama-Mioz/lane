//
// Created by Kurosu Chan on 2022/8/4.
//
#ifndef HELLO_WORLD_UTILS_H
#define HELLO_WORLD_UTILS_H

#include <chrono>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <pb_encode.h>
#include <map>
#include <esp_timer.h>
#include <concepts>

std::string to_hex(const std::basic_string<char> &s);
std::string to_hex(const char *s, size_t len);
std::string to_hex(const uint8_t *s, size_t len);

/**
 * @brief a function retrieve value by the number nearing the key. always move a unit up.
 *
 * @tparam T the type of value of map
 * @param keys a std::vector of int which should be sorted
 * @param m  a map whose key is int
 * @param val a value you want
 * @return T
 */
template <typename T>
T retrieveByVal(std::vector<int> const &keys, std::map<int, T> const &m, int val) {
  size_t idx = 0;
  // since keys is sorted we can get result easily
  for (auto key : keys) {
    if (val < key) {
      break;
    }
    idx += 1;
  }
  if (idx >= keys.size()) {
    idx = keys.size() - 1;
  }
  return m.at(keys[idx]);
}

template <class T>
class ValueRetriever {
private:
  std::map<int, T> m;
  std::vector<int> keys;
  int max_key = 0;

public:
  [[nodiscard]] int getMaxKey() const {
    return max_key;
  }

  const std::map<int, T> &getMap() const {
    return m;
  }

  [[nodiscard]] const std::vector<int> &getKeys() const {
    return keys;
  }

  /**
   * @brief Construct a new Value Retriever object
   *
   * @param m the map whose keys are int. The map should be immutable or call [updateKeys] after changing,
   *        otherwise things will be weired.
   */
  explicit ValueRetriever(std::map<int, T> m) : m(m) {
    updateKeys();
  };

  /**
   * @brief rebuild the keys and sort them, which is time consuming I guess.
   *        This function will be called in constructor.
   */
  void updateKeys() {
    keys.clear();
    // https://stackoverflow.com/questions/26281979/c-loop-through-map
    for (auto &[k, v] : m) {
      keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());
    max_key = *std::max_element(keys.begin(), keys.end());
  }

  T retrieve(int val) const {
    return retrieveByVal(keys, m, val);
  }
};

/**
 * @brief A auxiliary class to handle the callback of nanopb encoding.
 *        This exists because the need for
 * @warning: T must be a pointer type (with a little star behind it)
 */
template <class T>
struct PbEncodeCallback {
  using PbEncodeFunc = bool (*)(pb_ostream_t *stream, const pb_field_t *field, T const *arg);
  using PbEncodeVoid = bool (*)(pb_ostream_t *stream, const pb_field_t *field, void *const *arg);
  /**
   * @brief The parameter should be passed to the func with pointer type T.
   */
  T arg;
  /**
   * @brief The function to be called when encoding.
   */
  PbEncodeFunc func;

  /**
   * @brief cast the function to the type of PbEncodeCallbackVoid.
   * @return a function pointer of bool (*)(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
   */
  inline PbEncodeVoid func_to_void() {
    return reinterpret_cast<PbEncodeVoid>(func);
  }

  /**
   * @brief cast the arg (which should already be a pointer) to void *.
   * @return void *
   */
  inline void *arg_to_void() {
    return const_cast<void *>(reinterpret_cast<const void *>(arg));
  }

  /**
   * @note constructor is deleted. use the struct initializer instead.
   * @warning T must be a pointer type (with a little star behind it)
   * @see <a href="https://en.cppreference.com/w/c/language/struct_initialization">Struct Initialization</a>
   */
  // PbEncodeCallback() = delete;
};

using tp = decltype(std::chrono::steady_clock::now());

class Instant {
  tp time;

public:
  Instant() {
    this->time = std::chrono::steady_clock::now();
  }

  /// get the difference between now and the time Instant declared
  auto elapsed() {
    auto now      = std::chrono::steady_clock::now();
    auto duration = now - this->time;
    return duration;
  }

  void reset() {
    auto now   = std::chrono::steady_clock::now();
    this->time = now;
  }

  auto elapsed_and_reset() {
    auto now      = std::chrono::steady_clock::now();
    auto duration = now - this->time;
    this->time    = now;
    return duration;
  }

  /// get inner time
  [[nodiscard]] decltype(std::chrono::steady_clock::now()) getTime() const {
    return time;
  }
};

class ESPInstant {
  decltype(esp_timer_get_time()) time;

public:
  ESPInstant() {
    this->time = esp_timer_get_time();
  }

  auto elapsed() {
    auto now      = esp_timer_get_time();
    auto diff     = now - this->time;
    auto duration = std::chrono::duration<int64_t, std::micro>(diff);
    return duration;
  }

  void reset() {
    auto now   = esp_timer_get_time();
    this->time = now;
  }

  auto elapsed_and_reset() {
    auto now      = esp_timer_get_time();
    auto duration = now - this->time;
    this->time    = now;
    return duration;
  }

  auto getTime() {
    return time;
  }
};

namespace utils {
template <typename T>
concept is_intmax_t = std::is_same_v<T, decltype(std::ratio<1>::num) &>;

template <typename T>
concept is_ratio = requires(T t) {
  { t.num } -> is_intmax_t;
  { t.den } -> is_intmax_t;
};

template <class Rep, class RI = std::ratio<1>>
  requires is_ratio<RI> && std::is_arithmetic_v<Rep>
class length {
  Rep value;

public:
  using rep   = Rep;
  using ratio = RI;
  using Self  = length<Rep, RI>;
  Rep count() const {
    return value;
  }
  length() = default;
  explicit length(Rep rep) : value(rep) {}

  Self operator+=(const Self rhs) {
    value += rhs.value;
    return *this;
  }

  Self operator-=(const Self rhs) {
    value -= rhs.value;
    return *this;
  }

  Self operator+(Self rhs) {
    return Self(value + rhs.value);
  }

  Self operator-(const Self rhs) {
    return Self(value - rhs.value);
  }

  bool operator==(const Self rhs) const {
    return value == rhs.value;
  }

  bool operator!=(const Self rhs) const {
    return value != rhs.value;
  }

  bool operator<(const Self rhs) const {
    return value < rhs.value;
  }

  bool operator>(const Self rhs) const {
    return value > rhs.value;
  }

  bool operator<=(const Self rhs) const {
    return value <= rhs.value;
  }

  bool operator>=(const Self rhs) const {
    return value >= rhs.value;
  }
};

template <class FromLength, class ToLength,
          class RI = typename std::ratio_divide<typename FromLength::ratio, typename ToLength::ratio>::type>
struct __length_cast {
  ToLength operator()(const FromLength &fl) const {
    using _Ct = typename std::common_type<typename FromLength::rep, typename ToLength::rep, intmax_t>::type;
    return ToLength(static_cast<typename ToLength::rep>(
        static_cast<_Ct>(fl.count()) * static_cast<_Ct>(RI::num) / static_cast<_Ct>(RI::den)));
  };
};

template <class ToLength, class Rep, class RI>
  requires std::is_arithmetic_v<Rep> && is_ratio<RI>
ToLength length_cast(const length<Rep, RI> &fl) {
  return __length_cast<length<Rep, RI>, ToLength>()(fl);
}

using picometer  = length<int, std::pico>;
using micrometer = length<int, std::micro>;
using milimeter  = length<int, std::milli>;
using centimeter = length<int, std::centi>;
using meter      = length<int, std::ratio<1>>;
using inch       = length<int, std::ratio<254, 10000>>;
using feet       = length<int, std::ratio<328, 1000>>;
using kilometer  = length<int, std::kilo>;

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
  Color(uint32_t rgb) {
    r = (rgb >> 16) & 0xff;
    g = (rgb >> 8) & 0xff;
    b = rgb & 0xff;
  }

  /**
   * @brief convert to uint32_t
   * @return implicit conversion. operator overloading.
   */
  operator uint32_t() const {
    return (r << 16) | (g << 8) | b;
  }
};
namespace Colors {
  const auto Red     = Color(0xff0000);
  const auto Green   = Color(0x00ff00);
  const auto Blue    = Color(0x0000ff);
  const auto White   = Color(0xffffff);
  const auto Cyan    = Color(0x00ffff);
  const auto Magenta = Color(0xff00ff);
  const auto Yellow  = Color(0xffff00);
  const auto Amber   = Color(0xffbf00);
  const auto Orange  = Color(0xff8000);
  const auto Purple  = Color(0x8000ff);
  const auto Pink    = Color(0xff0080);
  const auto Azure   = Color(0x0080ff);
}
};

#endif // HELLO_WORLD_UTILS_H
