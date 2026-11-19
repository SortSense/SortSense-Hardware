#pragma once

#include <stdbool.h>
#include <stdint.h>

class TwoWire {
public:
  int mock_sda = -1;
  int mock_scl = -1;
  uint32_t mock_freq = 0;
  uint16_t mock_timeout = 0;
  uint8_t mock_tx_addr = 0;

  // Mask of devices answering probe at 0x29 (bit 0 = sensor 0, bit 1 = sensor 1, etc.)
  uint8_t mock_devices_present_mask = 0x0F;
  uint8_t mock_current_sensor_idx = 0;

  bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0) {
    mock_sda = sda;
    mock_scl = scl;
    mock_freq = frequency;
    return true;
  }

  void setTimeOut(uint16_t timeOutMillis) { mock_timeout = timeOutMillis; }

  void beginTransmission(uint8_t address) { mock_tx_addr = address; }

  uint8_t endTransmission(bool sendStop = true) {
    (void)sendStop;
    if (mock_tx_addr == 0x29) {
      if ((mock_devices_present_mask & (1 << mock_current_sensor_idx)) == 0) {
        return 2; // NACK / Not present
      }
    }
    return 0; // ACK
  }

  void reset() {
    mock_sda = -1;
    mock_scl = -1;
    mock_freq = 0;
    mock_timeout = 0;
    mock_tx_addr = 0;
    mock_devices_present_mask = 0x0F;
    mock_current_sensor_idx = 0;
  }
};

inline TwoWire Wire;

