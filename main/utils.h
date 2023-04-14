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

std::string to_hex(const std::basic_string<char> &s);

std::string to_hex(const char *s, size_t len);

/**
 * @brief a function retrieve value by the number nearing the key. always move a unit up.
 *
 * @tparam T the type of value of map
 * @param keys a std::vector of int which should be sorted
 * @param m  a map whose key is int
 * @param val a value you want
 * @return T
 */
template<typename T>
T retrieveByVal(std::vector<int> const &keys, std::map<int, T> const &m, int val) {
  size_t idx = 0;
  // since keys is sorted we can get result easily
  for (auto key: keys) {
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

template<class T>
class ValueRetriever {
private:
  std::map<int, T> m;
  std::vector<int> keys;
  int max_key = 0;
public:
  [[nodiscard]]
  int getMaxKey() const {
    return max_key;
  }

  const std::map<int, T> &getMap() const {
    return m;
  }

  [[nodiscard]]
  const std::vector<int> &getKeys() const {
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
    for (auto &[k, v]: m) {
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
template<class T>
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
  PbEncodeCallback() = delete;
};

class Instant {
  decltype(std::chrono::steady_clock::now()) time;
public:
  Instant() {
    this->time = std::chrono::steady_clock::now();
  }

  /// get the difference between now and the time Instant declared
  auto elapsed() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now - this->time;
    return duration;
  }

  void reset(){
    auto now = std::chrono::steady_clock::now();
    this->time = now;
  }

  auto elapsed_and_reset() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now - this->time;
    this->time = now;
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
    auto now = esp_timer_get_time();
    auto diff = now - this->time;
    auto duration = std::chrono::duration<int64_t, std::micro>(diff);
    return duration;
  }

  void reset(){
    auto now = esp_timer_get_time();
    this->time = now;
  }

  auto elapsed_and_reset() {
    auto now = esp_timer_get_time();
    auto duration = now - this->time;
    this->time = now;
    return duration;
  }

  auto getTime() {
    return time;
  }
};


#endif //HELLO_WORLD_UTILS_H
