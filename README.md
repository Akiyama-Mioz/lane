Need certain IDF version to build the project.

In your IDF directory.

```bash
cd $IDF_PATH
# I assume your origin is 
# https://github.com/espressif/esp-idf.git
git fetch origin v4.4.5
git checkout v4.4.5
git submodule update --init --recursive
./install.sh
source export.sh
```

```
-DIDF_MAINTAINER=1
```

```
Backtrace: 0x6380bbe6:0x3ffca950 0x400e75a5:0x3ffca990 0x400f1ec9:0x3ffca9f0 0x400f3565:0x3ffcaa10 0x400f3592:0x3ffcaa70 0x400ee857:0x3ffcaad0 0x400eec05:0x3ffcaaf0 0x400ee2a5:0x3ffcab10 0x400f8666:0x3ffcab30 0x400e7eb7:0x3ffcab50 0x40094d6d:0x3ffcab70
0x400e75a5: NimBLEClient::handleGapEvent(ble_gap_event*, void*) at /Users/crosstyan/Code/track-esp32/components/esp-nimble-cpp/src/NimBLEClient.cpp:979

0x400f1ec9: ble_gap_call_event_cb at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/nimble/host/src/ble_gap.c:716

0x400f3565: ble_gap_conn_broken at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/nimble/host/src/ble_gap.c:1253

0x400f3592: ble_gap_rx_disconn_complete at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/nimble/host/src/ble_gap.c:1280 (discriminator 4)

0x400ee857: ble_hs_hci_evt_disconn_complete at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/nimble/host/src/ble_hs_hci_evt.c:242

0x400eec05: ble_hs_hci_evt_process at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/nimble/host/src/ble_hs_hci_evt.c:903

0x400ee2a5: ble_hs_event_rx_hci_ev at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/nimble/host/src/ble_hs.c:520

0x400f8666: ble_npl_event_run at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/porting/npl/freertos/include/nimble/nimble_npl_os.h:125
 (inlined by) nimble_port_run at /Users/crosstyan/esp-idf/components/bt/host/nimble/nimble/porting/nimble/src/nimble_port.c:78

0x400e7eb7: NimBLEDevice::host_task(void*) at /Users/crosstyan/Code/track-esp32/components/esp-nimble-cpp/src/NimBLEDevice.cpp:843

0x40094d6d: vPortTaskWrapper at /Users/crosstyan/esp-idf/components/freertos/port/xtensa/port.c:142
```
