#pragma once
#include "FreeRTOS.h"
#include <stdint.h>

typedef void *SemaphoreHandle_t;

inline uint32_t mock_semaphore_give_count = 0;
inline uint32_t mock_semaphore_take_count = 0;
inline BaseType_t mock_semaphore_take_result = pdTRUE;
inline SemaphoreHandle_t mock_last_given_semaphore = nullptr;

inline void mock_semphr_reset() {
  mock_semaphore_give_count = 0;
  mock_semaphore_take_count = 0;
  mock_semaphore_take_result = pdTRUE;
  mock_last_given_semaphore = nullptr;
}

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  static int dummyMutex = 42;
  return (SemaphoreHandle_t)&dummyMutex;
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) {
  (void)sem;
  (void)timeout;
  mock_semaphore_take_count++;
  return mock_semaphore_take_result;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
  mock_semaphore_give_count++;
  mock_last_given_semaphore = sem;
  return pdTRUE;
}

inline void vSemaphoreDelete(SemaphoreHandle_t sem) { (void)sem; }
