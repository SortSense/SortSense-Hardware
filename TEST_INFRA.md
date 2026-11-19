# E2E Test Infra: SortSense-Hardware

## Test Philosophy
- Requirement-driven, opaque-box, and architectural validation.
- Methodology: Category-Partition + Boundary Value Analysis + Type Safety Static Analysis + Hardware Pinmap Validation + PlatformIO Compilation.

## Feature Inventory
| # | Feature | Source | Tier 1 | Tier 2 | Tier 3 |
|---|---------|--------|:------:|:------:|:------:|
| 1 | Dependencies & PlatformIO Config | ORIGINAL_REQUEST §R1 | ✓ | ✓ | ✓ |
| 2 | Safe Pinout & Peripheral Reservation | ORIGINAL_REQUEST §R2 | ✓ | ✓ | ✓ |
| 3 | Fixed-Width Type Enforcement | ORIGINAL_REQUEST §R1 | ✓ | ✓ | ✓ |
| 4 | OV5640 Camera Driver Initialization | ORIGINAL_REQUEST §R2 | ✓ | ✓ | ✓ |
| 5 | Frame Acquisition & Release | ORIGINAL_REQUEST §R2 | ✓ | ✓ | ✓ |
| 6 | WiFiManager Dynamic Provisioning | ORIGINAL_REQUEST §R3 | ✓ | ✓ | ✓ |
| 7 | Dual-Core FreeRTOS Task Pinning | ORIGINAL_REQUEST §R4 | ✓ | ✓ | ✓ |
| 8 | Asynchronous HTTP Server on Port 80 | ORIGINAL_REQUEST §R4 | ✓ | ✓ | ✓ |
| 9 | GET `/capture` Endpoint | ORIGINAL_REQUEST §R4 | ✓ | ✓ | ✓ |

## Test Architecture
- **Compilation Check**: `pio run -e esp32-s3-devkitc-1` must exit with return code 0, verifying all symbols, includes, and links succeed.
- **Hardware Pinmap Validation**: Validate complete pin assignment JSON against ESP32-S3 silicon and board constraints via `/home/Fabio/.gemini/config/plugins/esp32/scripts/validate_pinmap.py`.
- **Type Safety Static Check**: Scan public headers and sources for prohibited raw `int` types (enforcing `uint8_t`, `int8_t`, `uint16_t`, `uint32_t`, `size_t`).
- **Architectural Concurrency Check**: Verify presence and correct usage of `xTaskCreatePinnedToCore` for Core 0 and Core 1, non-blocking WiFiManager calls, and `vTaskDelay` watchdog yields.
- **Integrity Forensics**: Forensic integrity audit verifying zero hardcoded dummy responses, genuine camera driver calls, and proper frame buffer returns.

## Acceptance Thresholds
- PlatformIO compilation exit code: 0
- Pin conflict errors: 0
- Raw `int` occurrences in public API: 0
- Integrity violation findings: 0

