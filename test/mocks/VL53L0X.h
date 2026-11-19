#pragma once

#include "Wire.h"
#include <stdbool.h>
#include <stdint.h>

class VL53L0X {
public:
  TwoWire *bus = nullptr;
  uint8_t address = 0x29;
  uint16_t io_timeout = 500;
  bool mock_init_success = true;
  bool mock_timeout_flag = false;
  uint16_t mock_distance = 200;
  uint32_t mock_timing_budget = 33000;

  void setBus(TwoWire *b) { bus = b; }
  void setTimeout(uint16_t timeout) { io_timeout = timeout; }
  uint16_t getTimeout() { return io_timeout; }

  bool init(bool io_2v8 = true) {
    (void)io_2v8;
    return mock_init_success;
  }

  void setAddress(uint8_t new_addr) { address = new_addr; }

  uint8_t getAddress() { return address; }

  bool setMeasurementTimingBudget(uint32_t budget_us) {
    mock_timing_budget = budget_us;
    return true;
  }

  uint16_t readRangeSingleMillimeters() { return mock_distance; }

  bool timeoutOccurred() { return mock_timeout_flag; }

  void reset() {
    bus = nullptr;
    address = 0x29;
    io_timeout = 500;
    mock_init_success = true;
    mock_timeout_flag = false;
    mock_distance = 200;
    mock_timing_budget = 33000;
  }
};
