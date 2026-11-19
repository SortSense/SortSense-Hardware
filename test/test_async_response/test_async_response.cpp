/**
 * @file test_async_response.cpp
 * @brief Unit tests for AsyncCameraResponse zero-copy streaming and RAII
 * cleanup.
 *
 * Verifies single-chunk streaming, multi-chunk pagination, EOF detection,
 * null buffer safety, exact boundary reads, and RAII mutex/frame release.
 */

#include <string.h>
#include <unity.h>
#include <vector>

#include "async_camera_response.h"

// Mock control interfaces from test/mocks/
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_mock_mutex = nullptr;
static camera_fb_t s_mock_fb;
static uint8_t s_frame_data[250];

void setUp(void) {
  mock_camera_reset();
  mock_semphr_reset();

  s_mock_mutex = (SemaphoreHandle_t)0x12345678;

  for (size_t i = 0; i < sizeof(s_frame_data); ++i) {
    s_frame_data[i] = static_cast<uint8_t>((i * 7 + 3) & 0xFF);
  }

  s_mock_fb.buf = s_frame_data;
  s_mock_fb.len = sizeof(s_frame_data);
  s_mock_fb.width = 640;
  s_mock_fb.height = 480;
  s_mock_fb.format = PIXFORMAT_JPEG;
}

void tearDown(void) {}

/**
 * TC_RESP_01: Verify object initialization with valid frame buffer and mutex.
 */
void test_response_initialization(void) {
  AsyncCameraResponse resp(&s_mock_fb, s_mock_mutex);

  TEST_ASSERT_EQUAL_INT(200, resp.getCode());
  TEST_ASSERT_EQUAL_STRING("image/jpeg", resp.getContentType());
  TEST_ASSERT_EQUAL_UINT32(sizeof(s_frame_data), resp.getContentLength());
  TEST_ASSERT_TRUE(resp._sourceValid());
}

/**
 * TC_RESP_02: Verify single-chunk read exceeding buffer length.
 */
void test_response_single_chunk_streaming(void) {
  uint8_t out[300];
  memset(out, 0, sizeof(out));

  AsyncCameraResponse resp(&s_mock_fb, s_mock_mutex);

  const size_t bytesRead = resp._fillBuffer(out, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(sizeof(s_frame_data), bytesRead);
  TEST_ASSERT_EQUAL_MEMORY(s_frame_data, out, sizeof(s_frame_data));

  // Next read must indicate EOF (0 bytes)
  const size_t eofBytes = resp._fillBuffer(out, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(0, eofBytes);
}

/**
 * TC_RESP_03: Verify multi-chunk pagination across arbitrary chunk sizes.
 */
void test_response_multi_chunk_pagination(void) {
  std::vector<uint8_t> reconstructed;
  reconstructed.reserve(sizeof(s_frame_data));

  AsyncCameraResponse resp(&s_mock_fb, s_mock_mutex);

  const size_t chunkSize = 64;
  uint8_t chunk[chunkSize];

  // Chunk 1: 64 bytes (offset 0..63)
  size_t n = resp._fillBuffer(chunk, chunkSize);
  TEST_ASSERT_EQUAL_UINT32(64, n);
  reconstructed.insert(reconstructed.end(), chunk, chunk + n);

  // Chunk 2: 64 bytes (offset 64..127)
  n = resp._fillBuffer(chunk, chunkSize);
  TEST_ASSERT_EQUAL_UINT32(64, n);
  reconstructed.insert(reconstructed.end(), chunk, chunk + n);

  // Chunk 3: 64 bytes (offset 128..191)
  n = resp._fillBuffer(chunk, chunkSize);
  TEST_ASSERT_EQUAL_UINT32(64, n);
  reconstructed.insert(reconstructed.end(), chunk, chunk + n);

  // Chunk 4: 58 bytes (offset 192..249, remaining 250 - 192 = 58)
  n = resp._fillBuffer(chunk, chunkSize);
  TEST_ASSERT_EQUAL_UINT32(58, n);
  reconstructed.insert(reconstructed.end(), chunk, chunk + n);

  // Chunk 5: 0 bytes (EOF)
  n = resp._fillBuffer(chunk, chunkSize);
  TEST_ASSERT_EQUAL_UINT32(0, n);

  TEST_ASSERT_EQUAL_UINT32(sizeof(s_frame_data), reconstructed.size());
  TEST_ASSERT_EQUAL_MEMORY(s_frame_data, reconstructed.data(),
                           sizeof(s_frame_data));
}

/**
 * TC_RESP_04: Verify continuous EOF reporting once buffer is exhausted.
 */
void test_response_eof_detection(void) {
  uint8_t out[300];
  AsyncCameraResponse resp(&s_mock_fb, s_mock_mutex);

  resp._fillBuffer(out, sizeof(out));

  // Multiple calls after EOF must all return 0
  TEST_ASSERT_EQUAL_UINT32(0, resp._fillBuffer(out, 10));
  TEST_ASSERT_EQUAL_UINT32(0, resp._fillBuffer(out, 50));
  TEST_ASSERT_EQUAL_UINT32(0, resp._fillBuffer(out, 100));
}

/**
 * TC_RESP_05: Verify exact chunk boundary streaming.
 */
void test_response_exact_boundary(void) {
  camera_fb_t exact_fb;
  uint8_t buf[128];
  memset(buf, 0xAB, sizeof(buf));
  exact_fb.buf = buf;
  exact_fb.len = sizeof(buf);

  AsyncCameraResponse resp(&exact_fb, s_mock_mutex);

  uint8_t out[128];
  const size_t n1 = resp._fillBuffer(out, 128);
  TEST_ASSERT_EQUAL_UINT32(128, n1);
  TEST_ASSERT_EQUAL_MEMORY(buf, out, 128);

  // Boundary reached: next call must immediately return 0
  const size_t n2 = resp._fillBuffer(out, 64);
  TEST_ASSERT_EQUAL_UINT32(0, n2);
}

/**
 * TC_RESP_06: Verify null frame buffer safety.
 */
void test_response_null_buffer_safety(void) {
  AsyncCameraResponse resp(nullptr, s_mock_mutex);

  TEST_ASSERT_EQUAL_UINT32(0, resp.getContentLength());
  TEST_ASSERT_FALSE(resp._sourceValid());

  uint8_t out[32];
  TEST_ASSERT_EQUAL_UINT32(0, resp._fillBuffer(out, sizeof(out)));
}

/**
 * TC_RESP_07: Verify RAII automatic resource release on destruction.
 */
void test_response_raii_cleanup(void) {
  TEST_ASSERT_EQUAL_UINT32(0, mock_get_fb_return_call_count());
  TEST_ASSERT_EQUAL_UINT32(0, mock_semaphore_give_count);

  {
    AsyncCameraResponse resp(&s_mock_fb, s_mock_mutex);
    TEST_ASSERT_EQUAL_UINT32(0, mock_get_fb_return_call_count());
    TEST_ASSERT_EQUAL_UINT32(0, mock_semaphore_give_count);
  } // resp goes out of scope here

  // Both the frame buffer and the mutex must have been released
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_fb_return_call_count());
  TEST_ASSERT_EQUAL_PTR(&s_mock_fb, mock_get_returned_fb());
  TEST_ASSERT_EQUAL_UINT32(1, mock_semaphore_give_count);
  TEST_ASSERT_EQUAL_PTR(s_mock_mutex, mock_last_given_semaphore);
}

/**
 * TC_RESP_08: Verify RAII destructor handles null mutex gracefully.
 */
void test_response_raii_null_mutex(void) {
  {
    AsyncCameraResponse resp(&s_mock_fb, nullptr);
  }
  // Frame buffer released, but no mutex give called
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_fb_return_call_count());
  TEST_ASSERT_EQUAL_UINT32(0, mock_semaphore_give_count);
}

/**
 * TC_RESP_09: Verify RAII destructor handles null frame buffer gracefully.
 */
void test_response_raii_null_fb(void) {
  {
    AsyncCameraResponse resp(nullptr, s_mock_mutex);
  }
  // No frame buffer released, but mutex given back
  TEST_ASSERT_EQUAL_UINT32(0, mock_get_fb_return_call_count());
  TEST_ASSERT_EQUAL_UINT32(1, mock_semaphore_give_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_response_initialization);
  RUN_TEST(test_response_single_chunk_streaming);
  RUN_TEST(test_response_multi_chunk_pagination);
  RUN_TEST(test_response_eof_detection);
  RUN_TEST(test_response_exact_boundary);
  RUN_TEST(test_response_null_buffer_safety);
  RUN_TEST(test_response_raii_cleanup);
  RUN_TEST(test_response_raii_null_mutex);
  RUN_TEST(test_response_raii_null_fb);
  return UNITY_END();
}
