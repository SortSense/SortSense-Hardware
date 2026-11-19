#pragma once
#include <stdio.h>

#define ESP_LOGI(tag, format, ...)                                             \
  do {                                                                         \
    (void)(tag);                                                               \
    (void)(format);                                                            \
    if (false) {                                                               \
      printf(format, ##__VA_ARGS__);                                           \
    }                                                                          \
  } while (0)
#define ESP_LOGW(tag, format, ...)                                             \
  do {                                                                         \
    (void)(tag);                                                               \
    (void)(format);                                                            \
    if (false) {                                                               \
      printf(format, ##__VA_ARGS__);                                           \
    }                                                                          \
  } while (0)
#define ESP_LOGE(tag, format, ...)                                             \
  do {                                                                         \
    (void)(tag);                                                               \
    (void)(format);                                                            \
    if (false) {                                                               \
      printf(format, ##__VA_ARGS__);                                           \
    }                                                                          \
  } while (0)
#define ESP_LOGD(tag, format, ...)                                             \
  do {                                                                         \
    (void)(tag);                                                               \
    (void)(format);                                                            \
    if (false) {                                                               \
      printf(format, ##__VA_ARGS__);                                           \
    }                                                                          \
  } while (0)
