//
// Created by Kurosu Chan on 2022/8/4.
//
// necessary for using fmt library
#define FMT_HEADER_ONLY

#ifndef HELLO_WORLD_UTILS_H
#define HELLO_WORLD_UTILS_H

#include "fmt/core.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>

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
T retrieve_by_val(std::vector<int> const &keys, std::map<int, T> const &m, int val){
  size_t idx = 0;
  // since keys is sorted we can get result easily
  for (auto key:keys){
    if (val < key){
      break;
    }
    idx += 1;
  }
  if (idx >= keys.size()){
    idx = keys.size() - 1;
  }
  return m.at(keys[idx]);
}

template<class T>
class ValueRetriever {
private:
  std::map<int, T> m;
  std::vector<int> keys;
public:
  /**
   * @brief Construct a new Value Retriever object
   *
   * @param m the map whose keys are int. The map should be immutable or call [update_keys] after changing,
   *        otherwise things will be weired.
   */
  explicit ValueRetriever(std::map<int, T> m) : m(m) {
    update_keys();
  };

  /**
   * @brief rebuild the keys and sort them, which is time consuming I guess.
   *        This function will be called in constructor.
   */
  void update_keys(){
    keys.clear();
    for (auto &kv : m) {
      keys.push_back(kv.first);
    }
    std::sort(keys.begin(), keys.end());
    std::cout << "Here are the updated sorted keys: ";
    for (auto &v : keys){
      std::cout << v << " ";
    }
    std::cout << "\n";
  }

  T retrieve(int val) const {
    return retrieve_by_val(keys, m, val);
  }
};

#endif //HELLO_WORLD_UTILS_H
