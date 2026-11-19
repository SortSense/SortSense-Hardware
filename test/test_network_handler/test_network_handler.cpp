/**
 * @file test_network_handler.cpp
 * @brief Unit tests for network_handler WebSocket vision streaming and AI
 * classification contract parsing.
 */

#include <string.h>
#include <unity.h>

#include "config.h"
#include "network_handler.h"

// Mock control headers
#include "WebSocketsClient.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static int s_receivedBinIndex = -999;
static char s_receivedCategory[32] = {0};
static float s_receivedConfidence = -1.0f;
static uint32_t s_callbackCallCount = 0;

static void mockClassificationCallback(int binIndex, const char *category,
                                       float confidence) {
  s_receivedBinIndex = binIndex;
  if (category != nullptr) {
    strncpy(s_receivedCategory, category, sizeof(s_receivedCategory) - 1);
    s_receivedCategory[sizeof(s_receivedCategory) - 1] = '\0';
  }
  s_receivedConfidence = confidence;
  s_callbackCallCount++;
}

void setUp(void) {
  mock_semphr_reset();
  WebSocketsClient::mockReset();
  s_receivedBinIndex = -999;
  memset(s_receivedCategory, 0, sizeof(s_receivedCategory));
  s_receivedConfidence = -1.0f;
  s_callbackCallCount = 0;
}

void tearDown(void) {}

/**
 * TC_NET_01: Verify argument validation during network_init.
 */
void test_network_init_null_args(void) {
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, network_init(nullptr, 8000, "/ws"));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                    network_init("192.168.1.100", 8000, nullptr));
}

/**
 * TC_NET_02: Verify successful initialization and endpoint mapping.
 */
void test_network_init_success(void) {
  esp_err_t err = network_init("10.0.0.50", 8080, "/ws/vision");
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_STRING("10.0.0.50", WebSocketsClient::s_mockHost);
  TEST_ASSERT_EQUAL_UINT16(8080, WebSocketsClient::s_mockPort);
  TEST_ASSERT_EQUAL_STRING("/ws/vision", WebSocketsClient::s_mockPath);
}

/**
 * TC_NET_03: Verify network_loop calls underlying WebSocket client loop.
 */
void test_network_loop_execution(void) {
  network_init("10.0.0.50", 8080, "/ws/vision");
  TEST_ASSERT_EQUAL_UINT32(0, WebSocketsClient::s_mockLoopCallCount);

  network_loop();
  TEST_ASSERT_EQUAL_UINT32(1, WebSocketsClient::s_mockLoopCallCount);

  network_loop();
  TEST_ASSERT_EQUAL_UINT32(2, WebSocketsClient::s_mockLoopCallCount);
}

/**
 * TC_NET_04: Verify connection status transitions on CONNECTED/DISCONNECTED
 * events.
 */
void test_network_connection_state(void) {
  network_init("10.0.0.50", 8080, "/ws/vision");
  TEST_ASSERT_FALSE(network_is_connected());

  WebSocketsClient::mockTriggerEvent(WStype_CONNECTED, nullptr, 0);
  TEST_ASSERT_TRUE(network_is_connected());

  WebSocketsClient::mockTriggerEvent(WStype_DISCONNECTED, nullptr, 0);
  TEST_ASSERT_FALSE(network_is_connected());
}

/**
 * TC_NET_05: Verify JSON classification parsing with full valid contract.
 */
void test_network_json_parsing_valid(void) {
  const char json[] =
      "{\"category\": \"plastic\", \"bin_index\": 0, \"confidence\": 0.91}";
  int binIndex = -1;
  char category[32] = {0};
  float confidence = 0.0f;

  bool ok = network_parse_classification_json(
      reinterpret_cast<const uint8_t *>(json), strlen(json), &binIndex,
      category, sizeof(category), &confidence);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_INT(0, binIndex);
  TEST_ASSERT_EQUAL_STRING("plastic", category);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.91f, confidence);
}

/**
 * TC_NET_06: Verify JSON parsing fallback defaults for missing fields.
 */
void test_network_json_parsing_missing_fields(void) {
  const char json[] = "{}";
  int binIndex = 99;
  char category[32] = {0};
  float confidence = 99.0f;

  bool ok = network_parse_classification_json(
      reinterpret_cast<const uint8_t *>(json), strlen(json), &binIndex,
      category, sizeof(category), &confidence);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_INT(-1, binIndex);
  TEST_ASSERT_EQUAL_STRING("none", category);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, confidence);
}

/**
 * TC_NET_07: Verify JSON parsing rejects malformed payloads and null pointers.
 */
void test_network_json_parsing_malformed(void) {
  const char badJson[] = "{bad_json: 123";
  int binIndex = -1;
  char category[32] = {0};
  float confidence = 0.0f;

  TEST_ASSERT_FALSE(network_parse_classification_json(
      reinterpret_cast<const uint8_t *>(badJson), strlen(badJson), &binIndex,
      category, sizeof(category), &confidence));

  TEST_ASSERT_FALSE(network_parse_classification_json(
      nullptr, 10, &binIndex, category, sizeof(category), &confidence));

  TEST_ASSERT_FALSE(network_parse_classification_json(
      reinterpret_cast<const uint8_t *>(""), 0, &binIndex, category,
      sizeof(category), &confidence));
}

/**
 * TC_NET_08: Verify callback dispatch on incoming WStype_TEXT classification.
 */
void test_network_classification_callback_dispatch(void) {
  network_init("10.0.0.50", 8080, "/ws/vision");
  network_register_classification_callback(mockClassificationCallback);

  const char json[] =
      "{\"category\": \"metal\", \"bin_index\": 2, \"confidence\": 0.88}";

  WebSocketsClient::mockTriggerEvent(
      WStype_TEXT,
      const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(json)),
      strlen(json));

  TEST_ASSERT_EQUAL_UINT32(1, s_callbackCallCount);
  TEST_ASSERT_EQUAL_INT(2, s_receivedBinIndex);
  TEST_ASSERT_EQUAL_STRING("metal", s_receivedCategory);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.88f, s_receivedConfidence);
}

/**
 * TC_NET_09: Verify frame transmission behavior when disconnected.
 */
void test_network_send_frame_when_disconnected(void) {
  network_init("10.0.0.50", 8080, "/ws/vision");
  uint8_t dummyFrame[64] = {0xFF, 0xD8}; // Fake JPEG header

  TEST_ASSERT_FALSE(network_is_connected());
  TEST_ASSERT_FALSE(network_send_frame(dummyFrame, sizeof(dummyFrame)));
  TEST_ASSERT_EQUAL_UINT32(0, WebSocketsClient::s_mockSendBinCallCount);
}

/**
 * TC_NET_10: Verify frame transmission behavior when connected.
 */
void test_network_send_frame_when_connected(void) {
  network_init("10.0.0.50", 8080, "/ws/vision");
  WebSocketsClient::mockTriggerEvent(WStype_CONNECTED, nullptr, 0);
  TEST_ASSERT_TRUE(network_is_connected());

  uint8_t dummyFrame[128] = {0xFF, 0xD8};

  TEST_ASSERT_TRUE(network_send_frame(dummyFrame, sizeof(dummyFrame)));
  TEST_ASSERT_EQUAL_UINT32(1, WebSocketsClient::s_mockSendBinCallCount);
  TEST_ASSERT_EQUAL_UINT32(sizeof(dummyFrame),
                           WebSocketsClient::s_mockLastSentLen);
}

/**
 * TC_NET_11: Verify frame transmission validation rejects null/empty buffers.
 */
void test_network_send_frame_null_or_empty(void) {
  network_init("10.0.0.50", 8080, "/ws/vision");
  WebSocketsClient::mockTriggerEvent(WStype_CONNECTED, nullptr, 0);

  uint8_t dummy[10] = {0};
  TEST_ASSERT_FALSE(network_send_frame(nullptr, 10));
  TEST_ASSERT_FALSE(network_send_frame(dummy, 0));
  TEST_ASSERT_EQUAL_UINT32(0, WebSocketsClient::s_mockSendBinCallCount);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_network_init_null_args);
  RUN_TEST(test_network_init_success);
  RUN_TEST(test_network_loop_execution);
  RUN_TEST(test_network_connection_state);
  RUN_TEST(test_network_json_parsing_valid);
  RUN_TEST(test_network_json_parsing_missing_fields);
  RUN_TEST(test_network_json_parsing_malformed);
  RUN_TEST(test_network_classification_callback_dispatch);
  RUN_TEST(test_network_send_frame_when_disconnected);
  RUN_TEST(test_network_send_frame_when_connected);
  RUN_TEST(test_network_send_frame_null_or_empty);

  return UNITY_END();
}
