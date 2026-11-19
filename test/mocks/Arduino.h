#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IRAM_ATTR

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#define LOW 0
#define HIGH 1

#define CHANGE 1
#define FALLING 2
#define RISING 3

#define digitalPinToInterrupt(p) (p)

typedef void (*voidFuncPtr)(void);

inline uint8_t mock_pin_modes[64] = {0};
inline uint8_t mock_pin_values[64] = {1};
inline voidFuncPtr mock_interrupt_handlers[64] = {nullptr};
inline int mock_interrupt_modes[64] = {0};

inline bool mock_psram_found_state = true;
inline unsigned long mock_millis_val = 1000;

inline void mock_set_psram_found(bool found) { mock_psram_found_state = found; }
inline bool psramFound() { return mock_psram_found_state; }

inline void mock_set_millis(unsigned long ms) { mock_millis_val = ms; }
inline unsigned long millis() { return mock_millis_val; }

inline void delay(unsigned long ms) { (void)ms; }

inline int log_printf(const char *fmt, ...) {
  (void)fmt;
  return 0;
}

inline void mock_arduino_reset() {
  memset(mock_pin_modes, 0, sizeof(mock_pin_modes));
  for (size_t i = 0; i < sizeof(mock_pin_values); ++i) {
    mock_pin_values[i] = 1;
  }
  memset(mock_interrupt_handlers, 0, sizeof(mock_interrupt_handlers));
  memset(mock_interrupt_modes, 0, sizeof(mock_interrupt_modes));
  mock_psram_found_state = true;
  mock_millis_val = 1000;
}

inline void pinMode(uint8_t pin, uint8_t mode) {
  if (pin < 64)
    mock_pin_modes[pin] = mode;
}

inline int digitalRead(uint8_t pin) {
  if (pin < 64)
    return mock_pin_values[pin];
  return 1;
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < 64)
    mock_pin_values[pin] = val;
}

inline void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode) {
  if (pin < 64) {
    mock_interrupt_handlers[pin] = handler;
    mock_interrupt_modes[pin] = mode;
  }
}

inline void detachInterrupt(uint8_t pin) {
  if (pin < 64) {
    mock_interrupt_handlers[pin] = nullptr;
    mock_interrupt_modes[pin] = 0;
  }
}

// Critical section mocks
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(portMUX_TYPE *mux) { (void)mux; }
inline void portEXIT_CRITICAL(portMUX_TYPE *mux) { (void)mux; }
#define portENTER_CRITICAL_ISR(mux) portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL_ISR(mux) portEXIT_CRITICAL(mux)

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
