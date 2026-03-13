/**
 * @file rship_config.h
 * @brief Rship SDK Configuration
 *
 * This file defines compile-time configuration for the Rship embedded SDK.
 * Select ONE profile by defining it before including this header, or define
 * individual settings for custom configurations.
 *
 * Profiles:
 *   RSHIP_PROFILE_A - Tiny MCU (bare-metal, minimal RTOS)
 *   RSHIP_PROFILE_B - MCU with RTOS (FreeRTOS, Zephyr, etc.)
 *   RSHIP_PROFILE_C - Embedded Linux SBC
 *
 * @copyright Copyright (c) 2024 Rship
 * @license AGPL-3.0-or-later
 */

#ifndef RSHIP_CONFIG_H
#define RSHIP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Profile Selection
 *
 * Define ONE of these before including this header:
 *   #define RSHIP_PROFILE_A
 *   #define RSHIP_PROFILE_B
 *   #define RSHIP_PROFILE_C
 *
 * Or define RSHIP_PROFILE_CUSTOM and set all values manually.
 *============================================================================*/

/* Default to Profile B if nothing specified */
#if !defined(RSHIP_PROFILE_A) && !defined(RSHIP_PROFILE_B) && \
    !defined(RSHIP_PROFILE_C) && !defined(RSHIP_PROFILE_CUSTOM)
#define RSHIP_PROFILE_B
#endif

/*============================================================================
 * Profile A: Tiny MCU (8-128 KB RAM, bare-metal or minimal RTOS)
 *
 * Targets: STM32F0/F1/F4, nRF52, ESP8266, ARM Cortex-M0/M0+/M3/M4
 * Features: Minimal, gateway-dependent, polling-only, no dynamic allocation
 *============================================================================*/
#ifdef RSHIP_PROFILE_A

/* Entity limits - kept small for memory constraints */
#define RSHIP_MAX_INSTANCES         1
#define RSHIP_MAX_TARGETS           4
#define RSHIP_MAX_EMITTERS          8
#define RSHIP_MAX_ACTIONS           8

/* Buffer sizes */
#define RSHIP_TX_BUFFER_SIZE        512
#define RSHIP_RX_BUFFER_SIZE        512
#define RSHIP_MAX_MESSAGE_SIZE      256

/* Feature flags */
#define RSHIP_FEATURE_WEBSOCKET     0   /* Use gateway protocol instead */
#define RSHIP_FEATURE_JSON          0   /* MessagePack only */
#define RSHIP_FEATURE_QUERIES       0   /* Not supported */
#define RSHIP_FEATURE_REPORTS       0   /* Not supported */
#define RSHIP_FEATURE_THREADING     0   /* Polling only */
#define RSHIP_FEATURE_LOGGING       0   /* Minimal/none */
#define RSHIP_FEATURE_DYNAMIC_ALLOC 0   /* Static allocation only */
#define RSHIP_FEATURE_TLS           0   /* No TLS */
#define RSHIP_FEATURE_GATEWAY_CLIENT 1  /* Gateway protocol client */

/* ID configuration */
#define RSHIP_ID_PREFIX_LEN         4   /* Short prefix for memory */
#define RSHIP_SHORT_ID_MAX_LEN      16

/* String limits */
#define RSHIP_NAME_MAX_LEN          24
#define RSHIP_CATEGORY_MAX_LEN      16
#define RSHIP_MESSAGE_MAX_LEN       32
#define RSHIP_COLOR_MAX_LEN         8   /* #RRGGBB */

#endif /* RSHIP_PROFILE_A */

/*============================================================================
 * Profile B: MCU with RTOS (128 KB - 1 MB RAM)
 *
 * Targets: ARM Cortex-M4/M7 + FreeRTOS/Zephyr, ESP32, nRF52/53
 * Features: Direct WebSocket, bounded dynamic allocation, task-based
 *============================================================================*/
#ifdef RSHIP_PROFILE_B

/* Entity limits */
#define RSHIP_MAX_INSTANCES         4
#define RSHIP_MAX_TARGETS           16
#define RSHIP_MAX_EMITTERS          32
#define RSHIP_MAX_ACTIONS           32

/* Buffer sizes */
#define RSHIP_TX_BUFFER_SIZE        4096
#define RSHIP_RX_BUFFER_SIZE        4096
#define RSHIP_MAX_MESSAGE_SIZE      2048

/* Feature flags */
#define RSHIP_FEATURE_WEBSOCKET     1
#define RSHIP_FEATURE_JSON          0   /* Set to 1 if needed */
#define RSHIP_FEATURE_QUERIES       1
#define RSHIP_FEATURE_REPORTS       1
#define RSHIP_FEATURE_THREADING     1
#define RSHIP_FEATURE_LOGGING       1
#define RSHIP_FEATURE_DYNAMIC_ALLOC 0   /* Use pools, not malloc */
#define RSHIP_FEATURE_TLS           0   /* Optional, set to 1 for mbedTLS */
#define RSHIP_FEATURE_GATEWAY_CLIENT 0

/* Query/Report limits */
#define RSHIP_MAX_QUERIES           2
#define RSHIP_MAX_REPORTS           2
#define RSHIP_QUERY_RESULT_SIZE     1024
#define RSHIP_REPORT_RESULT_SIZE    1024

/* ID configuration */
#define RSHIP_ID_PREFIX_LEN         8
#define RSHIP_SHORT_ID_MAX_LEN      32

/* String limits */
#define RSHIP_NAME_MAX_LEN          48
#define RSHIP_CATEGORY_MAX_LEN      32
#define RSHIP_MESSAGE_MAX_LEN       64
#define RSHIP_COLOR_MAX_LEN         8

/* Task configuration */
#define RSHIP_TASK_STACK_SIZE       4096
#define RSHIP_TASK_PRIORITY         5

#endif /* RSHIP_PROFILE_B */

/*============================================================================
 * Profile C: Embedded Linux SBC (64+ MB RAM)
 *
 * Targets: Raspberry Pi, BeagleBone, custom ARM/RISC-V Linux
 * Features: Full implementation, dynamic allocation, multi-threaded
 *============================================================================*/
#ifdef RSHIP_PROFILE_C

/* Entity limits - generous for Linux */
#define RSHIP_MAX_INSTANCES         16
#define RSHIP_MAX_TARGETS           64
#define RSHIP_MAX_EMITTERS          128
#define RSHIP_MAX_ACTIONS           128

/* Buffer sizes */
#define RSHIP_TX_BUFFER_SIZE        16384
#define RSHIP_RX_BUFFER_SIZE        16384
#define RSHIP_MAX_MESSAGE_SIZE      8192

/* Feature flags - everything enabled */
#define RSHIP_FEATURE_WEBSOCKET     1
#define RSHIP_FEATURE_JSON          1
#define RSHIP_FEATURE_QUERIES       1
#define RSHIP_FEATURE_REPORTS       1
#define RSHIP_FEATURE_THREADING     1
#define RSHIP_FEATURE_LOGGING       1
#define RSHIP_FEATURE_DYNAMIC_ALLOC 1
#define RSHIP_FEATURE_TLS           1
#define RSHIP_FEATURE_GATEWAY_CLIENT 0
#define RSHIP_FEATURE_GATEWAY_SERVER 1  /* Can act as gateway for Profile A */

/* Query/Report limits */
#define RSHIP_MAX_QUERIES           8
#define RSHIP_MAX_REPORTS           8
#define RSHIP_QUERY_RESULT_SIZE     8192
#define RSHIP_REPORT_RESULT_SIZE    8192

/* ID configuration */
#define RSHIP_ID_PREFIX_LEN         8
#define RSHIP_SHORT_ID_MAX_LEN      64

/* String limits - dynamic for Profile C */
#define RSHIP_NAME_MAX_LEN          128
#define RSHIP_CATEGORY_MAX_LEN      64
#define RSHIP_MESSAGE_MAX_LEN       256
#define RSHIP_COLOR_MAX_LEN         8

#endif /* RSHIP_PROFILE_C */

/*============================================================================
 * Common Configuration (all profiles)
 *============================================================================*/

/* Connection timing */
#ifndef RSHIP_RECONNECT_MIN_MS
#define RSHIP_RECONNECT_MIN_MS      1000
#endif

#ifndef RSHIP_RECONNECT_MAX_MS
#define RSHIP_RECONNECT_MAX_MS      30000
#endif

#ifndef RSHIP_HEARTBEAT_MS
#define RSHIP_HEARTBEAT_MS          15000
#endif

#ifndef RSHIP_HEARTBEAT_TIMEOUT_MS
#define RSHIP_HEARTBEAT_TIMEOUT_MS  45000
#endif

/* Pulse rate limiting */
#ifndef RSHIP_PULSE_MIN_INTERVAL_MS
#define RSHIP_PULSE_MIN_INTERVAL_MS 10
#endif

/* WebSocket configuration */
#ifndef RSHIP_WS_PATH
#define RSHIP_WS_PATH               "/myko"
#endif

#ifndef RSHIP_WS_FRAME_MAX_SIZE
#define RSHIP_WS_FRAME_MAX_SIZE     RSHIP_MAX_MESSAGE_SIZE
#endif

/* Protocol version */
#define RSHIP_PROTOCOL_VERSION      1

/*============================================================================
 * Validation
 *============================================================================*/

/* Ensure buffer sizes are reasonable */
#if RSHIP_TX_BUFFER_SIZE < 128
#error "RSHIP_TX_BUFFER_SIZE must be at least 128 bytes"
#endif

#if RSHIP_RX_BUFFER_SIZE < 128
#error "RSHIP_RX_BUFFER_SIZE must be at least 128 bytes"
#endif

/* Ensure at least one entity slot */
#if RSHIP_MAX_INSTANCES < 1
#error "RSHIP_MAX_INSTANCES must be at least 1"
#endif

#ifdef __cplusplus
}
#endif

#endif /* RSHIP_CONFIG_H */
