#pragma once
#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_DEFAULT (1 << 12)

inline size_t mock_free_psram_size = 4 * 1024 * 1024; // Default 4MB

inline void mock_set_free_psram(size_t size) { mock_free_psram_size = size; }

inline size_t heap_caps_get_free_size(uint32_t caps) {
  if (caps & MALLOC_CAP_SPIRAM) {
    return mock_free_psram_size;
  }
  return 128 * 1024;
}
