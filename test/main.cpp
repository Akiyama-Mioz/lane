#include <iostream>
#include <chrono>
#include <thread>
#include <pb_decode.h>
#include <pb_encode.h>
#include <sstream>
#include "whitelist.h"
#include "simple_log.h"

int main() {
  simple_log::init();
  const auto TAG = "main";
  using namespace white_list;
  auto list = list_t{
      item_t{Addr{{0x00, 0x01, 0x02, 0x03, 0x04, 0x05}}},
      item_t{Name{"T03"}},
      item_t{Name{"T34"}},
      item_t{Addr{{0x00, 0x01, 0x02, 0x03, 0x00, 0x00}}},
  };

  const auto BUF_SIZE = 256;
  uint8_t buf[BUF_SIZE];
  auto ostream   = pb_ostream_from_buffer(buf, BUF_SIZE);
  WhiteListSet S = WhiteListSet_init_zero;
  bool ok        = marshal_set_white_list(&ostream, S, list);
  if (!ok) {
    LOG_E(TAG, "bad marshal");
  }
  std::cout << utils::toHex(buf, ostream.bytes_written) << std::endl;
  LOG_I(TAG, "written %zu", ostream.bytes_written);

  auto new_list = list_t{};

  // don't use the whole stream, cuz the pb don't know when it ends
  auto istream = pb_istream_from_buffer(buf, ostream.bytes_written);
  WhiteListSet OS = WhiteListSet_init_zero;
  auto out_list = unmarshal_set_white_list(&istream, OS);
  if (!out_list.has_value()){
    LOG_E(TAG, "bad unmarshal");
    return 1;
  }

  return 0;
}
