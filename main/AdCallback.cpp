//
// Created by Kurosu Chan on 2022/8/4.
//

#include "AdCallback.h"


void AdCallback::onResult(BLEAdvertisedDevice *advertisedDevice) {
  auto name = advertisedDevice->getName();
  auto payload = std::string(reinterpret_cast<const char *>(advertisedDevice->getPayload()), 31);
  if (lastDevices[name] == payload) {
    ESP_LOGI("MAIN_AdCallback_onResult", "Duplicate");
    return;
  }
  if (advertisedDevice->getName().find("T03") != std::string::npos) {
    fmt::print("Name: {}, Data: {}, RSSI: {}\n", name,
               to_hex(payload.c_str(), 31),
               advertisedDevice->getRSSI());

    uint8_t buffer[128];
    BLEAdvertisingData data = BLEAdvertisingData_init_zero;

    /**
     * @brief a function to encode a string to a pb_ostream_t with zero-terminator.
     * @see <a href="https://stackoverflow.com/questions/57569586/how-to-encode-a-string-when-it-is-a-pb-callback-t-type">How to encode a string when it is a pb_callback_t type</a>
     * @param[in] arg the string to encode, should be char or uint8_t or something equivalent.
     */
    auto encode_string = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
      const char *str = reinterpret_cast<const char *>(*arg);
      if (!pb_encode_tag_for_field(stream, field)) {
        return false;
      } else {
        return pb_encode_string(stream, (uint8_t *) str, strlen(str));
      }
    };

    const struct {
      uint8_t *content;
      size_t size;
    } payload_data {
      reinterpret_cast<uint8_t *>(advertisedDevice->getPayload()),
      31 // payload size, here it's fixed since bluetooth advertisement payload is 31 bytes.
    };

    /**
     * @brief a function to encode a string to a pb_ostream_t with a certain length
     * @warning This function is using void * as argument, but it is not a pointer to a string.
     *         It is a pointer to a pointer to a struct, see param for details. \n
     *         It's not type-safe, but it's the best we can do in the fucking C (we got lambda in C++
     *         but it doesn't work with 'nanopb', which is a C library)
     * @see <a href="https://martin-ueding.de/posts/currying-in-c/">Currying in C </a>
     * @param[in] arg should be a struct with contains a field named content (uint8_t*) and a field named size (size_t).
     */
    auto encode_str_with_len = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
      struct Payload {
        uint8_t *content;
        size_t size;
      };
      auto payload = static_cast<Payload *>(*arg);
      if (!pb_encode_tag_for_field(stream, field)) {
        return false;
      } else {
        return pb_encode_string(stream, (uint8_t *) payload->content, payload->size);
      }
    };

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    data.name.arg = const_cast<void *>(reinterpret_cast<const void *>(name.c_str()));
    data.name.funcs.encode = encode_string;
    data.manufactureData.arg = const_cast<void *>(reinterpret_cast<const void *>(&payload_data));
    data.manufactureData.funcs.encode = encode_str_with_len;
    data.rssi = advertisedDevice->getRSSI();
    bool status = pb_encode(&stream, BLEAdvertisingData_fields, &data);
    auto length = stream.bytes_written;
    if (this->characteristic != nullptr && status) {
      this->characteristic->setValue(buffer, length);
      this->characteristic->notify();
      std::string buf = std::string(reinterpret_cast<char *>(buffer), length);
      fmt::print("Protobuf: {}, Length: {}\n", to_hex(buf), length);
    } else {
      fmt::print("Error: {}\n", status);
    }
    lastDevices.insert_or_assign(name, payload);
  }
}

void ServerCallbacks::onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
  printf("Client address: ");
  fmt::print(NimBLEAddress(desc->peer_ota_addr).toString());
  printf("\n");
  /** We can use the connection handle here to ask for different connection parameters.
   *  Args: connection handle, min connection interval, max connection interval
   *  latency, supervision timeout.
   *  Units; Min/Max Intervals: 1.25 millisecond increments.
   *  Latency: number of intervals allowed to skip.
   *  Timeout: 10 millisecond increments, try for 5x interval time for best results.
   */
  pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
}

/** Alternative onConnect() method to extract details of the connection.
 *  See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
 */
void ServerCallbacks::onConnect(NimBLEServer *pServer) {
  fmt::print("Client connected. ");
  fmt::print("Multi-connect support: start advertising\n");
  NimBLEDevice::startAdvertising();
}

void ServerCallbacks::onDisconnect(NimBLEServer *pServer) {
  printf("Client disconnected - start advertising\n");
  NimBLEDevice::startAdvertising();
}

void ServerCallbacks::onMTUChange(uint16_t MTU, ble_gap_conn_desc *desc) {
  printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
}
