#pragma once
#include <stddef.h>
#include <stdint.h>

typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef void *TaskHandle_t;
typedef void *QueueHandle_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0

#define portMAX_DELAY 0xFFFFFFFF
#define pdMS_TO_TICKS(ms) (ms)
