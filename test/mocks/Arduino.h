#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

inline bool mock_psram_found_state = true;

inline void mock_set_psram_found(bool found) { mock_psram_found_state = found; }

inline bool psramFound() { return mock_psram_found_state; }

inline unsigned long millis() { return 1000; }

inline void delay(unsigned long ms) { (void)ms; }

inline int log_printf(const char *fmt, ...) {
  (void)fmt;
  return 0;
}

class MockSerial {
public:
  void begin(uint32_t baud) { (void)baud; }
  void print(const char *s) { (void)s; }
  void print(int n) { (void)n; }
  void println(const char *s = "") { (void)s; }
  void println(int n) { (void)n; }
  void printf(const char *fmt, ...) { (void)fmt; }
};

inline MockSerial Serial;
