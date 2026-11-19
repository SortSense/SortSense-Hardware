/**
 * @file test_ir_handler.cpp
 * @brief Unit tests for ir_handler LM393 IR break-beam sensor interrupt
 * manager.
 */

#include <unity.h>

#include "Arduino.h"
#include "config.h"
#include "ir_handler.h"

void setUp(void) {
  mock_arduino_reset();
  ir_deinit();
}

void tearDown(void) { ir_deinit(); }

/**
 * TC_IR_01: Verify pin initialization modes and hardware interrupt
 * registration.
 */
void test_ir_init_pin_modes_and_interrupts(void) {
  esp_err_t err = ir_init();
  TEST_ASSERT_EQUAL(ESP_OK, err);

  TEST_ASSERT_EQUAL_UINT8(INPUT_PULLUP, mock_pin_modes[PIN_IR_BEAM_1]);
  TEST_ASSERT_EQUAL_UINT8(INPUT_PULLUP, mock_pin_modes[PIN_IR_BEAM_2]);
  TEST_ASSERT_EQUAL_UINT8(INPUT_PULLUP, mock_pin_modes[PIN_IR_BEAM_3]);
  TEST_ASSERT_EQUAL_UINT8(INPUT_PULLUP, mock_pin_modes[PIN_IR_BEAM_4]);

  TEST_ASSERT_EQUAL_INT(FALLING, mock_interrupt_modes[PIN_IR_BEAM_1]);
  TEST_ASSERT_EQUAL_INT(FALLING, mock_interrupt_modes[PIN_IR_BEAM_2]);
  TEST_ASSERT_EQUAL_INT(FALLING, mock_interrupt_modes[PIN_IR_BEAM_3]);
  TEST_ASSERT_EQUAL_INT(FALLING, mock_interrupt_modes[PIN_IR_BEAM_4]);

  TEST_ASSERT_NOT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_1]);
  TEST_ASSERT_NOT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_2]);
  TEST_ASSERT_NOT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_3]);
  TEST_ASSERT_NOT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_4]);
}

/**
 * TC_IR_02: Verify deinitialization detaches all interrupts.
 */
void test_ir_deinit_detaches_interrupts(void) {
  ir_init();
  ir_deinit();

  TEST_ASSERT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_1]);
  TEST_ASSERT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_2]);
  TEST_ASSERT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_3]);
  TEST_ASSERT_NULL(mock_interrupt_handlers[PIN_IR_BEAM_4]);
}

/**
 * TC_IR_03: Verify initial state returns no pending drop events.
 */
void test_ir_poll_drop_event_empty(void) {
  ir_init();
  int bin = -99;
  TEST_ASSERT_FALSE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(-99, bin);
}

/**
 * TC_IR_04: Verify interrupt trigger captures drop event and resets flag on
 * poll.
 */
void test_ir_interrupt_trigger_and_poll(void) {
  ir_init();
  mock_set_millis(1000);

  // Simulate hardware interrupt on Pin 14 (Bin 0: plastic)
  mock_interrupt_handlers[PIN_IR_BEAM_1]();

  int bin = -1;
  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(0, bin);

  // Subsequent poll should return false (event consumed)
  TEST_ASSERT_FALSE(ir_poll_drop_event(&bin));
}

/**
 * TC_IR_05: Verify 500ms debounce suppresses rapid duplicate triggers.
 */
void test_ir_debounce_same_bin(void) {
  ir_init();
  mock_set_millis(1000);

  mock_interrupt_handlers[PIN_IR_BEAM_1]();

  int bin = -1;
  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(0, bin);

  // Simulate bounce 150ms later (< 500ms debounce threshold)
  mock_set_millis(1150);
  mock_interrupt_handlers[PIN_IR_BEAM_1]();

  TEST_ASSERT_FALSE(ir_poll_drop_event(&bin));
}

/**
 * TC_IR_06: Verify trigger accepted after debounce interval has elapsed.
 */
void test_ir_trigger_after_debounce_window(void) {
  ir_init();
  mock_set_millis(1000);
  mock_interrupt_handlers[PIN_IR_BEAM_1]();

  int bin = -1;
  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));

  // Advance time past debounce window (1000 + 550 = 1550ms)
  mock_set_millis(1550);
  mock_interrupt_handlers[PIN_IR_BEAM_1]();

  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(0, bin);
}

/**
 * TC_IR_07: Verify each pin maps to its corresponding compartment bin.
 */
void test_ir_all_bins_mapped_correctly(void) {
  ir_init();
  int bin = -1;

  // Bin 0 on PIN_IR_BEAM_1 (14)
  mock_set_millis(2000);
  mock_interrupt_handlers[PIN_IR_BEAM_1]();
  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(0, bin);

  // Bin 1 on PIN_IR_BEAM_2 (38)
  mock_set_millis(2600);
  mock_interrupt_handlers[PIN_IR_BEAM_2]();
  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(1, bin);

  // Bin 2 on PIN_IR_BEAM_3 (39)
  mock_set_millis(3200);
  mock_interrupt_handlers[PIN_IR_BEAM_3]();
  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(2, bin);

  // Bin 3 on PIN_IR_BEAM_4 (40)
  mock_set_millis(3800);
  mock_interrupt_handlers[PIN_IR_BEAM_4]();
  TEST_ASSERT_TRUE(ir_poll_drop_event(&bin));
  TEST_ASSERT_EQUAL_INT(3, bin);
}

/**
 * TC_IR_08: Verify beam break detection logic (LOW = broken).
 */
void test_ir_is_beam_broken(void) {
  ir_init();

  mock_pin_values[PIN_IR_BEAM_1] = HIGH;
  TEST_ASSERT_FALSE(ir_is_beam_broken(0));

  mock_pin_values[PIN_IR_BEAM_1] = LOW;
  TEST_ASSERT_TRUE(ir_is_beam_broken(0));

  // Out of range check
  TEST_ASSERT_FALSE(ir_is_beam_broken(4));
  TEST_ASSERT_FALSE(ir_is_beam_broken(99));
}

/**
 * TC_IR_09: Verify boundary checking on verify-drop dispatcher.
 */
void test_ir_send_verify_drop_bounds(void) {
  TEST_ASSERT_FALSE(ir_send_verify_drop(-1));
  TEST_ASSERT_FALSE(ir_send_verify_drop(4));
  TEST_ASSERT_TRUE(ir_send_verify_drop(0));
  TEST_ASSERT_TRUE(ir_send_verify_drop(3));
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_ir_init_pin_modes_and_interrupts);
  RUN_TEST(test_ir_deinit_detaches_interrupts);
  RUN_TEST(test_ir_poll_drop_event_empty);
  RUN_TEST(test_ir_interrupt_trigger_and_poll);
  RUN_TEST(test_ir_debounce_same_bin);
  RUN_TEST(test_ir_trigger_after_debounce_window);
  RUN_TEST(test_ir_all_bins_mapped_correctly);
  RUN_TEST(test_ir_is_beam_broken);
  RUN_TEST(test_ir_send_verify_drop_bounds);

  return UNITY_END();
}
