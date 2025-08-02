/*
 * Copyright 2025 sekigon-gonnoc
 * SPDX-License-Identifier: GPL-2.0 or later
 */

#include "azoteq_iqs7211e.h"
#include "pointing_device_internal.h"
#include "wait.h"
#include "debug.h"
#include "IQS7211_init.h"
#include "gpio.h"
#include "timer.h"
#include <stdlib.h>

const pointing_device_driver_t azoteq_iqs7211e_pointing_device_driver = {
    .init       = azoteq_iqs7211e_init,
    .get_report = azoteq_iqs7211e_get_report,
    .set_cpi    = azoteq_iqs7211e_set_cpi,
    .get_cpi    = azoteq_iqs7211e_get_cpi,
};

static uint16_t     azoteq_iqs7211e_product_number = 0;
static i2c_status_t azoteq_iqs7211e_init_status    = I2C_STATUS_ERROR;
static bool         azoteq_iqs7211e_use_ready_pin  = false;

bool azoteq_iqs7211e_is_ready(void) {
    if (azoteq_iqs7211e_use_ready_pin) {
        // RDY pin is active LOW
        return !readPin(AZOTEQ_IQS7211E_RDY_PIN);
    }
    return true; // If no RDY pin configured, assume always ready
}

void azoteq_iqs7211e_wait_for_ready(uint16_t timeout_ms) {
    if (!azoteq_iqs7211e_use_ready_pin) {
        return; // No RDY pin configured, return immediately
    }

    uint16_t elapsed = 0;
    while (!azoteq_iqs7211e_is_ready() && elapsed < timeout_ms) {
        wait_ms(1);
        elapsed++;
    }

    if (elapsed >= timeout_ms) {
        dprintf("IQS7211E: RDY timeout after %dms\n", timeout_ms);
    }
}

i2c_status_t azoteq_iqs7211e_get_base_data(azoteq_iqs7211e_base_data_t *base_data) {
    // Wait for device to be ready before reading
    azoteq_iqs7211e_wait_for_ready(50);

    if (!azoteq_iqs7211e_is_ready()) {
        dprintf("IQS7211E: Device not ready for data read\n");
        return I2C_STATUS_ERROR;
    }

    uint8_t      transferBytes[18];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_RELATIVE_X, transferBytes, 18, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        base_data->relative_x.l  = transferBytes[0];
        base_data->relative_x.h  = transferBytes[1];
        base_data->relative_y.l  = transferBytes[2];
        base_data->relative_y.h  = transferBytes[3];
        base_data->gesture_x.l   = transferBytes[4];
        base_data->gesture_x.h   = transferBytes[5];
        base_data->gesture_y.l   = transferBytes[6];
        base_data->gesture_y.h   = transferBytes[7];
        base_data->gestures[0]   = transferBytes[8];
        base_data->gestures[1]   = transferBytes[9];
        base_data->info_flags[0] = transferBytes[10];
        base_data->info_flags[1] = transferBytes[11];
        base_data->finger_1_x.l  = transferBytes[12];
        base_data->finger_1_x.h  = transferBytes[13];
        base_data->finger_1_y.l  = transferBytes[14];
        base_data->finger_1_y.h  = transferBytes[15];
        base_data->finger_2_x.l  = transferBytes[16];
        base_data->finger_2_x.h  = transferBytes[17];
    }

    return status;
}

i2c_status_t azoteq_iqs7211e_reset_suspend(bool reset, bool suspend) {
    uint8_t      transferBytes[2];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        if (reset) {
            transferBytes[1] |= (1 << IQS7211E_SW_RESET_BIT);
        }
        status = i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);
    }

    return status;
}

i2c_status_t azoteq_iqs7211e_set_event_mode(bool enabled) {
    uint8_t      transferBytes[2];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_CONFIG_SETTINGS, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        if (enabled) {
            transferBytes[1] |= (1 << IQS7211E_EVENT_MODE_BIT);
        } else {
            transferBytes[1] &= ~(1 << IQS7211E_EVENT_MODE_BIT);
        }
        status = i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_CONFIG_SETTINGS, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);
    }

    return status;
}

i2c_status_t azoteq_iqs7211e_acknowledge_reset(void) {
    azoteq_iqs7211e_wait_for_ready(50);
    uint8_t      transferBytes[2];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        azoteq_iqs7211e_wait_for_ready(50);
        transferBytes[0] |= (1 << IQS7211E_ACK_RESET_BIT);
        status = i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);
        dprintf("IQS7211E: Acknowledged reset, status %d\n", status);
    }

    return status;
}

i2c_status_t azoteq_iqs7211e_reati(void) {
    azoteq_iqs7211e_wait_for_ready(100);
    uint8_t      transferBytes[2];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        azoteq_iqs7211e_wait_for_ready(100);
        transferBytes[0] |= (1 << IQS7211E_TP_RE_ATI_BIT);
        status = i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);
        dprintf("IQS7211E: RE-ATI enabled, status %d %d %d\n", status, transferBytes[0], transferBytes[1]);
    }

    return status;
}

uint16_t azoteq_iqs7211e_get_product(void) {
    // Wait for device to be ready before reading
    azoteq_iqs7211e_wait_for_ready(100);

    if (!azoteq_iqs7211e_is_ready()) {
        azoteq_iqs7211e_product_number = 0xff;
        dprintf("IQS7211E: Device not ready for product read\n");
        return 0;
    }

    uint8_t      transferBytes[2];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_PROD_NUM, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        azoteq_iqs7211e_product_number = transferBytes[0] | (transferBytes[1] << 8);
    }

    dprintf("IQS7211E: Product number %u, %d\n", azoteq_iqs7211e_product_number, status);
    return azoteq_iqs7211e_product_number;
}

void azoteq_iqs7211e_set_cpi(uint16_t cpi) {}

uint16_t azoteq_iqs7211e_get_cpi(void) {
    return 0;
}

i2c_status_t azoteq_iqs7211e_write_memory_map(void) {
    uint8_t      transferBytes[30];
    i2c_status_t status = I2C_STATUS_SUCCESS;

    dprintf("IQS7211E: Writing memory map\n");

    // 1. Write ALP Compensation (0x1F - 0x20)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = ALP_COMPENSATION_A_0;
    transferBytes[1] = ALP_COMPENSATION_A_1;
    transferBytes[2] = ALP_COMPENSATION_B_0;
    transferBytes[3] = ALP_COMPENSATION_B_1;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_ALP_ATI_COMP_A, transferBytes, 4, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t1. Write ALP Compensation\n");

    // 2. Write ATI Settings (0x21 - 0x27)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0]  = TP_ATI_MULTIPLIERS_DIVIDERS_0;
    transferBytes[1]  = TP_ATI_MULTIPLIERS_DIVIDERS_1;
    transferBytes[2]  = TP_COMPENSATION_DIV;
    transferBytes[3]  = TP_REF_DRIFT_LIMIT;
    transferBytes[4]  = TP_ATI_TARGET_0;
    transferBytes[5]  = TP_ATI_TARGET_1;
    transferBytes[6]  = TP_MIN_COUNT_REATI_0;
    transferBytes[7]  = TP_MIN_COUNT_REATI_1;
    transferBytes[8]  = ALP_ATI_MULTIPLIERS_DIVIDERS_0;
    transferBytes[9]  = ALP_ATI_MULTIPLIERS_DIVIDERS_1;
    transferBytes[10] = ALP_COMPENSATION_DIV;
    transferBytes[11] = ALP_LTA_DRIFT_LIMIT;
    transferBytes[12] = ALP_ATI_TARGET_0;
    transferBytes[13] = ALP_ATI_TARGET_1;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_TP_GLOBAL_MIRRORS, transferBytes, 14, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t2. Write ATI Settings\n");

    // 3. Write Report rates and timings (0x28 - 0x32)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0]  = ACTIVE_MODE_REPORT_RATE_0;
    transferBytes[1]  = ACTIVE_MODE_REPORT_RATE_1;
    transferBytes[2]  = IDLE_TOUCH_MODE_REPORT_RATE_0;
    transferBytes[3]  = IDLE_TOUCH_MODE_REPORT_RATE_1;
    transferBytes[4]  = IDLE_MODE_REPORT_RATE_0;
    transferBytes[5]  = IDLE_MODE_REPORT_RATE_1;
    transferBytes[6]  = LP1_MODE_REPORT_RATE_0;
    transferBytes[7]  = LP1_MODE_REPORT_RATE_1;
    transferBytes[8]  = LP2_MODE_REPORT_RATE_0;
    transferBytes[9]  = LP2_MODE_REPORT_RATE_1;
    transferBytes[10] = ACTIVE_MODE_TIMEOUT_0;
    transferBytes[11] = ACTIVE_MODE_TIMEOUT_1;
    transferBytes[12] = IDLE_TOUCH_MODE_TIMEOUT_0;
    transferBytes[13] = IDLE_TOUCH_MODE_TIMEOUT_1;
    transferBytes[14] = IDLE_MODE_TIMEOUT_0;
    transferBytes[15] = IDLE_MODE_TIMEOUT_1;
    transferBytes[16] = LP1_MODE_TIMEOUT_0;
    transferBytes[17] = LP1_MODE_TIMEOUT_1;
    transferBytes[18] = REATI_RETRY_TIME;
    transferBytes[19] = REF_UPDATE_TIME;
    transferBytes[20] = I2C_TIMEOUT_0;
    transferBytes[21] = I2C_TIMEOUT_1;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_ACTIVE_MODE_RR, transferBytes, 22, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t3. Write Report rates and timings\n");

    // 4. Write System control settings (0x33 - 0x35)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = SYSTEM_CONTROL_0;
    transferBytes[1] = SYSTEM_CONTROL_1;
    transferBytes[2] = CONFIG_SETTINGS0;
    transferBytes[3] = CONFIG_SETTINGS1;
    transferBytes[4] = OTHER_SETTINGS_0;
    transferBytes[5] = OTHER_SETTINGS_1;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 6, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t4. Write System control settings\n");

    // 5. Write ALP Settings (0x36 - 0x37)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = ALP_SETUP_0;
    transferBytes[1] = ALP_SETUP_1;
    transferBytes[2] = ALP_TX_ENABLE_0;
    transferBytes[3] = ALP_TX_ENABLE_1;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_ALP_SETUP, transferBytes, 4, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t5. Write ALP Settings\n");

    // 6. Write Threshold settings (0x38 - 0x3A)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = TRACKPAD_TOUCH_SET_THRESHOLD;
    transferBytes[1] = TRACKPAD_TOUCH_CLEAR_THRESHOLD;
    transferBytes[2] = ALP_THRESHOLD_0;
    transferBytes[3] = ALP_THRESHOLD_1;
    transferBytes[4] = ALP_SET_DEBOUNCE;
    transferBytes[5] = ALP_CLEAR_DEBOUNCE;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_TP_TOUCH_SET_CLEAR_THR, transferBytes, 6, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t6. Write Threshold settings\n");

    // 7. Write Filter Betas (0x3B - 0x3C)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = ALP_COUNT_BETA_LP1;
    transferBytes[1] = ALP_LTA_BETA_LP1;
    transferBytes[2] = ALP_COUNT_BETA_LP2;
    transferBytes[3] = ALP_LTA_BETA_LP2;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_LP1_FILTERS, transferBytes, 4, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t7. Write Filter Betas\n");

    // 8. Write Hardware settings (0x3D - 0x40)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = TP_CONVERSION_FREQUENCY_UP_PASS_LENGTH;
    transferBytes[1] = TP_CONVERSION_FREQUENCY_FRACTION_VALUE;
    transferBytes[2] = ALP_CONVERSION_FREQUENCY_UP_PASS_LENGTH;
    transferBytes[3] = ALP_CONVERSION_FREQUENCY_FRACTION_VALUE;
    transferBytes[4] = TRACKPAD_HARDWARE_SETTINGS_0;
    transferBytes[5] = TRACKPAD_HARDWARE_SETTINGS_1;
    transferBytes[6] = ALP_HARDWARE_SETTINGS_0;
    transferBytes[7] = ALP_HARDWARE_SETTINGS_1;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_TP_CONV_FREQ, transferBytes, 8, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t8. Write Hardware settings\n");

    // 9. Write TP Settings (0x41 - 0x49)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0]  = TRACKPAD_SETTINGS_0_0;
    transferBytes[1]  = TRACKPAD_SETTINGS_0_1;
    transferBytes[2]  = TRACKPAD_SETTINGS_1_0;
    transferBytes[3]  = TRACKPAD_SETTINGS_1_1;
    transferBytes[4]  = X_RESOLUTION_0;
    transferBytes[5]  = X_RESOLUTION_1;
    transferBytes[6]  = Y_RESOLUTION_0;
    transferBytes[7]  = Y_RESOLUTION_1;
    transferBytes[8]  = XY_DYNAMIC_FILTER_BOTTOM_SPEED_0;
    transferBytes[9]  = XY_DYNAMIC_FILTER_BOTTOM_SPEED_1;
    transferBytes[10] = XY_DYNAMIC_FILTER_TOP_SPEED_0;
    transferBytes[11] = XY_DYNAMIC_FILTER_TOP_SPEED_1;
    transferBytes[12] = XY_DYNAMIC_FILTER_BOTTOM_BETA;
    transferBytes[13] = XY_DYNAMIC_FILTER_STATIC_FILTER_BETA;
    transferBytes[14] = STATIONARY_TOUCH_MOV_THRESHOLD;
    transferBytes[15] = FINGER_SPLIT_FACTOR;
    transferBytes[16] = X_TRIM_VALUE;
    transferBytes[17] = Y_TRIM_VALUE;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_TP_RX_SETTINGS, transferBytes, 18, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t9. Write TP Settings\n");

    // 10. Write Version numbers (0x4A)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = MINOR_VERSION;
    transferBytes[1] = MAJOR_VERSION;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SETTINGS_VERSION, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t10. Write Version numbers\n");

    // 11. Write Gesture Settings (0x4B - 0x55)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0]  = GESTURE_ENABLE_0;
    transferBytes[1]  = GESTURE_ENABLE_1;
    transferBytes[2]  = TAP_TOUCH_TIME_0;
    transferBytes[3]  = TAP_TOUCH_TIME_1;
    transferBytes[4]  = TAP_WAIT_TIME_0;
    transferBytes[5]  = TAP_WAIT_TIME_1;
    transferBytes[6]  = TAP_DISTANCE_0;
    transferBytes[7]  = TAP_DISTANCE_1;
    transferBytes[8]  = HOLD_TIME_0;
    transferBytes[9]  = HOLD_TIME_1;
    transferBytes[10] = SWIPE_TIME_0;
    transferBytes[11] = SWIPE_TIME_1;
    transferBytes[12] = SWIPE_X_DISTANCE_0;
    transferBytes[13] = SWIPE_X_DISTANCE_1;
    transferBytes[14] = SWIPE_Y_DISTANCE_0;
    transferBytes[15] = SWIPE_Y_DISTANCE_1;
    transferBytes[16] = SWIPE_X_CONS_DIST_0;
    transferBytes[17] = SWIPE_X_CONS_DIST_1;
    transferBytes[18] = SWIPE_Y_CONS_DIST_0;
    transferBytes[19] = SWIPE_Y_CONS_DIST_1;
    transferBytes[20] = SWIPE_ANGLE;
    transferBytes[21] = PALM_THRESHOLD;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_GESTURE_ENABLE, transferBytes, 22, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t11. Write Gesture Settings\n");

    // 12. Write Rx Tx Map Settings (0x56 - 0x5C)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0]  = RX_TX_MAP_0;
    transferBytes[1]  = RX_TX_MAP_1;
    transferBytes[2]  = RX_TX_MAP_2;
    transferBytes[3]  = RX_TX_MAP_3;
    transferBytes[4]  = RX_TX_MAP_4;
    transferBytes[5]  = RX_TX_MAP_5;
    transferBytes[6]  = RX_TX_MAP_6;
    transferBytes[7]  = RX_TX_MAP_7;
    transferBytes[8]  = RX_TX_MAP_8;
    transferBytes[9]  = RX_TX_MAP_9;
    transferBytes[10] = RX_TX_MAP_10;
    transferBytes[11] = RX_TX_MAP_11;
    transferBytes[12] = RX_TX_MAP_12;
    transferBytes[13] = RX_TX_MAP_FILLER;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_RX_TX_MAPPING_0_1, transferBytes, 14, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t12. Write Rx Tx Map Settings\n");

    // 13. Write Cycle 0 - 9 Settings (0x5D - 0x6B)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0]  = PLACEHOLDER_0;
    transferBytes[1]  = CH_1_CYCLE_0;
    transferBytes[2]  = CH_2_CYCLE_0;
    transferBytes[3]  = PLACEHOLDER_1;
    transferBytes[4]  = CH_1_CYCLE_1;
    transferBytes[5]  = CH_2_CYCLE_1;
    transferBytes[6]  = PLACEHOLDER_2;
    transferBytes[7]  = CH_1_CYCLE_2;
    transferBytes[8]  = CH_2_CYCLE_2;
    transferBytes[9]  = PLACEHOLDER_3;
    transferBytes[10] = CH_1_CYCLE_3;
    transferBytes[11] = CH_2_CYCLE_3;
    transferBytes[12] = PLACEHOLDER_4;
    transferBytes[13] = CH_1_CYCLE_4;
    transferBytes[14] = CH_2_CYCLE_4;
    transferBytes[15] = PLACEHOLDER_5;
    transferBytes[16] = CH_1_CYCLE_5;
    transferBytes[17] = CH_2_CYCLE_5;
    transferBytes[18] = PLACEHOLDER_6;
    transferBytes[19] = CH_1_CYCLE_6;
    transferBytes[20] = CH_2_CYCLE_6;
    transferBytes[21] = PLACEHOLDER_7;
    transferBytes[22] = CH_1_CYCLE_7;
    transferBytes[23] = CH_2_CYCLE_7;
    transferBytes[24] = PLACEHOLDER_8;
    transferBytes[25] = CH_1_CYCLE_8;
    transferBytes[26] = CH_2_CYCLE_8;
    transferBytes[27] = PLACEHOLDER_9;
    transferBytes[28] = CH_1_CYCLE_9;
    transferBytes[29] = CH_2_CYCLE_9;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_PROXA_CYCLE0, transferBytes, 30, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t13. Write Cycle 0 - 9 Settings\n");

    // 14. Write Cycle 10 - 19 Settings (0x6C - 0x7A)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0]  = PLACEHOLDER_10;
    transferBytes[1]  = CH_1_CYCLE_10;
    transferBytes[2]  = CH_2_CYCLE_10;
    transferBytes[3]  = PLACEHOLDER_11;
    transferBytes[4]  = CH_1_CYCLE_11;
    transferBytes[5]  = CH_2_CYCLE_11;
    transferBytes[6]  = PLACEHOLDER_12;
    transferBytes[7]  = CH_1_CYCLE_12;
    transferBytes[8]  = CH_2_CYCLE_12;
    transferBytes[9]  = PLACEHOLDER_13;
    transferBytes[10] = CH_1_CYCLE_13;
    transferBytes[11] = CH_2_CYCLE_13;
    transferBytes[12] = PLACEHOLDER_14;
    transferBytes[13] = CH_1_CYCLE_14;
    transferBytes[14] = CH_2_CYCLE_14;
    transferBytes[15] = PLACEHOLDER_15;
    transferBytes[16] = CH_1_CYCLE_15;
    transferBytes[17] = CH_2_CYCLE_15;
    transferBytes[18] = PLACEHOLDER_16;
    transferBytes[19] = CH_1_CYCLE_16;
    transferBytes[20] = CH_2_CYCLE_16;
    transferBytes[21] = PLACEHOLDER_17;
    transferBytes[22] = CH_1_CYCLE_17;
    transferBytes[23] = CH_2_CYCLE_17;
    transferBytes[24] = PLACEHOLDER_18;
    transferBytes[25] = CH_1_CYCLE_18;
    transferBytes[26] = CH_2_CYCLE_18;
    transferBytes[27] = PLACEHOLDER_19;
    transferBytes[28] = CH_1_CYCLE_19;
    transferBytes[29] = CH_2_CYCLE_19;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_PROXA_CYCLE10, transferBytes, 30, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t14. Write Cycle 10 - 19 Settings\n");

    // 15. Write Cycle 20 Settings (0x7B - 0x7C)
    azoteq_iqs7211e_wait_for_ready(100);
    transferBytes[0] = PLACEHOLDER_20;
    transferBytes[1] = CH_1_CYCLE_20;
    transferBytes[2] = CH_2_CYCLE_20;
    status |= i2c_write_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_PROXA_CYCLE20, transferBytes, 3, AZOTEQ_IQS7211E_TIMEOUT_MS);
    dprintf("\t15. Write Cycle 20 Settings\n");

    dprintf("IQS7211E: Memory map write complete, status: %d\n", status);
    return status;
}

i2c_status_t azoteq_iqs7211e_check_reset(void) {
    // Wait for device to be ready before reading
    azoteq_iqs7211e_wait_for_ready(50);

    if (!azoteq_iqs7211e_is_ready()) {
        dprintf("IQS7211E: Device not ready for reset check\n");
        return I2C_STATUS_ERROR;
    }

    uint8_t      transferBytes[2];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_INFO_FLAGS, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        return (transferBytes[0] & (1 << IQS7211E_SHOW_RESET_BIT)) ? I2C_STATUS_SUCCESS : I2C_STATUS_ERROR;
    }

    return status;
}

bool azoteq_iqs7211e_read_ati_active(void) {
    // Wait for device to be ready before reading
    azoteq_iqs7211e_wait_for_ready(500);

    if (!azoteq_iqs7211e_is_ready()) {
        dprintf("IQS7211E: Device not ready for ATI check\n");
        return true; // Assume ATI is active if we can't read
    }

    uint8_t      transferBytes[2];
    i2c_status_t status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, IQS7211E_MM_SYS_CONTROL, transferBytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS);

    if (status == I2C_STATUS_SUCCESS) {
        dprintf("IQS7211E: ATI active check, flags: 0x%02X\n", transferBytes[0]);

        return (transferBytes[0] & (1 << IQS7211E_TP_RE_ATI_BIT));
    }

    return true; // Assume ATI is active if we can't read
}

void azoteq_iqs7211e_init(void) {
    i2c_init();

    // Initialize RDY pin if configured
    if (AZOTEQ_IQS7211E_RDY_PIN != NO_PIN) {
        setPinInputHigh(AZOTEQ_IQS7211E_RDY_PIN);
        azoteq_iqs7211e_use_ready_pin = true;
        dprintf("IQS7211E: RDY pin configured on pin %d\n", AZOTEQ_IQS7211E_RDY_PIN);
    } else {
        azoteq_iqs7211e_use_ready_pin = false;
        dprintf("IQS7211E: No RDY pin configured\n");
    }

    debug_enable = true;
    dprintf("IQS7211E: Initialization started\n");

    // Wait for device to be ready
    azoteq_iqs7211e_wait_for_ready(100);

    // Software reset
    azoteq_iqs7211e_reset_suspend(true, false);
    wait_ms(50);

    // Wait for device to be ready after reset
    azoteq_iqs7211e_wait_for_ready(200);

    // Check product number
    if (azoteq_iqs7211e_get_product() == AZOTEQ_IQS7211E_PRODUCT_NUM) {
        dprintf("IQS7211E: Device found\n");

        // Check if reset occurred
        if (azoteq_iqs7211e_check_reset() == I2C_STATUS_SUCCESS) {
            dprintf("IQS7211E: Reset event confirmed\n");

            // Write all settings from init file
            azoteq_iqs7211e_init_status = azoteq_iqs7211e_write_memory_map();

            if (azoteq_iqs7211e_init_status == I2C_STATUS_SUCCESS) {
                // Acknowledge reset
                azoteq_iqs7211e_init_status |= azoteq_iqs7211e_acknowledge_reset();
                wait_ms(100);

                // Run ATI
                azoteq_iqs7211e_init_status |= azoteq_iqs7211e_reati();

                // Wait for ATI to complete
                int ati_timeout = 30; // 1000 * 10ms = 10 second timeout
                while (azoteq_iqs7211e_read_ati_active() && ati_timeout > 0) {
                    wait_ms(5);
                    ati_timeout--;
                }

                if (ati_timeout >= 0) {
                    dprintf("IQS7211E: ATI completed\n");

                    // Wait for device to be ready before setting event mode
                    azoteq_iqs7211e_wait_for_ready(500);

                    // Set event mode
                    azoteq_iqs7211e_init_status |= azoteq_iqs7211e_set_event_mode(true);

                    wait_ms(ACTIVE_MODE_REPORT_RATE_0 + 1);

                    dprintf("IQS7211E: Init complete, status: %d\n", azoteq_iqs7211e_init_status);
                } else {
                    dprintf("IQS7211E: ATI timeout\n");
                }
            } else {
                dprintf("IQS7211E: Memory map write failed\n");
            }
        } else {
            dprintf("IQS7211E: No reset event detected\n");
        }
    } else {
        dprintf("IQS7211E: Device not found\n");
    }
}

report_mouse_t azoteq_iqs7211e_get_report(report_mouse_t mouse_report) {
    report_mouse_t  temp_report = {0};
    static uint16_t previous_x = 0xffff, previous_y = 0xffff;
    static uint16_t finger_2_prev_x = 0xffff, finger_2_prev_y = 0xffff;
    static bool     previous_valid = false;
    static bool     finger_2_prev_valid = false;
    static uint16_t tap_start_x = 0, tap_start_y = 0;
    static uint16_t touch_start_time = 0;
    static uint16_t last_tap_time = 0;
    static uint8_t  tap_count = 0;
    static bool     double_tap_hold = false;
    static bool     is_clicking = false;
    static uint8_t  pending_click_release = 0;

    if (azoteq_iqs7211e_init_status == I2C_STATUS_SUCCESS) {
        // Only read data if device is ready or if no RDY pin is configured
        if (!azoteq_iqs7211e_use_ready_pin || azoteq_iqs7211e_is_ready()) {
            azoteq_iqs7211e_base_data_t base_data = {0};
            i2c_status_t                status    = azoteq_iqs7211e_get_base_data(&base_data);

            if (status == I2C_STATUS_SUCCESS) {
                uint8_t finger_count = base_data.info_flags[1] & 0x03;
                uint32_t current_time = timer_read32();

                // Handle pending click releases
                if (pending_click_release > 0) {
                    if (timer_elapsed32(pending_click_release) > 50) {
                        if (pending_click_release == 1) {
                            temp_report.buttons &= ~MOUSE_BTN1;
                        } else if (pending_click_release == 2) {
                            temp_report.buttons &= ~MOUSE_BTN2;
                        }
                        pending_click_release = 0;
                    }
                }

                if (finger_count == 1) {
                    // Single finger handling
                    uint16_t finger_1_x = AZOTEQ_IQS7211E_COMBINE_H_L_BYTES(base_data.finger_1_x.h, base_data.finger_1_x.l);
                    uint16_t finger_1_y = AZOTEQ_IQS7211E_COMBINE_H_L_BYTES(base_data.finger_1_y.h, base_data.finger_1_y.l);

                    if (!previous_valid) {
                        // Touch start
                        tap_start_x = finger_1_x;
                        tap_start_y = finger_1_y;
                        touch_start_time = current_time;
                    } else if (finger_2_prev_valid) {
                        // Transitioning from two finger to one finger - reset position reference
                        tap_start_x = finger_1_x;
                        tap_start_y = finger_1_y;
                        touch_start_time = current_time;
                        tap_count = 0;
                        double_tap_hold = false;
                        is_clicking = false;
                    } else {
                        // Normal single finger movement
                        int16_t delta_x = finger_1_x - previous_x;
                        int16_t delta_y = finger_1_y - previous_y;

                        temp_report.x = CONSTRAIN_HID_XY(delta_x);
                        temp_report.y = CONSTRAIN_HID_XY(delta_y);
                    }

                    previous_x = finger_1_x;
                    previous_y = finger_1_y;
                    previous_valid = true;
                    finger_2_prev_valid = false;

                } else if (finger_count == 2) {
                    // Two finger handling
                    uint16_t finger_1_x = AZOTEQ_IQS7211E_COMBINE_H_L_BYTES(base_data.finger_1_x.h, base_data.finger_1_x.l);
                    uint16_t finger_1_y = AZOTEQ_IQS7211E_COMBINE_H_L_BYTES(base_data.finger_1_y.h, base_data.finger_1_y.l);
                    uint16_t finger_2_x = AZOTEQ_IQS7211E_COMBINE_H_L_BYTES(base_data.finger_2_x.h, base_data.finger_2_x.l);
                    
                    // Read finger 2 Y coordinate (need additional read)
                    uint8_t finger_2_y_bytes[2];
                    i2c_status_t finger2_status = i2c_read_register(AZOTEQ_IQS7211E_ADDRESS, 0x17, finger_2_y_bytes, 2, AZOTEQ_IQS7211E_TIMEOUT_MS); // IQS7211E_MM_FINGER_2_Y
                    uint16_t finger_2_y = (finger2_status == I2C_STATUS_SUCCESS) ? AZOTEQ_IQS7211E_COMBINE_H_L_BYTES(finger_2_y_bytes[1], finger_2_y_bytes[0]) : 0;

                    if (!finger_2_prev_valid) {
                        // Two finger touch start
                        touch_start_time = current_time;
                        tap_count = 0;
                        double_tap_hold = false;
                        if (is_clicking) {
                            is_clicking = false;
                            temp_report.buttons &= ~MOUSE_BTN1;
                        }
                    } else {
                        // Two finger movement - scroll
                        int16_t y_movement = (finger_1_y + finger_2_y) / 2 - (previous_y + finger_2_prev_y) / 2;
                        int16_t x_movement = (finger_1_x + finger_2_x) / 2 - (previous_x + finger_2_prev_x) / 2;

                        if (y_movement != 0) {
                            temp_report.v = CONSTRAIN_HID(-y_movement); // Scroll wheel
                        }
                        if (x_movement != 0) {
                            temp_report.h = CONSTRAIN_HID(x_movement); // Horizontal scroll
                        }
                    }

                    previous_x = finger_1_x;
                    previous_y = finger_1_y;
                    finger_2_prev_x = finger_2_x;
                    finger_2_prev_y = finger_2_y;
                    previous_valid = true;
                    finger_2_prev_valid = true;

                } else {
                    // No fingers - handle touch end events
                    if (previous_valid || finger_2_prev_valid) {
                        uint16_t touch_duration = timer_elapsed(touch_start_time);

                        if (finger_2_prev_valid) {
                            // Two finger tap - right click
                            if (touch_duration < 200) {
                                temp_report.buttons |= MOUSE_BTN2;
                                pending_click_release = 2;
                            }
                        } else if (previous_valid) {
                            // Single finger tap handling
                            uint16_t tap_distance = abs(previous_x - tap_start_x) + abs(previous_y - tap_start_y);

                            if (touch_duration < 200 && tap_distance < 50) {
                                uint16_t tap_interval = timer_elapsed(last_tap_time);

                                if (tap_interval < 400 && tap_count == 1) {
                                    // Double tap - start drag
                                    double_tap_hold = true;
                                    is_clicking = true;
                                    temp_report.buttons |= MOUSE_BTN1;
                                    tap_count = 0;
                                } else {
                                    // Single tap
                                    if (!double_tap_hold) {
                                        temp_report.buttons |= MOUSE_BTN1;
                                        pending_click_release = 1;
                                    }
                                    tap_count = 1;
                                }
                                last_tap_time = current_time;
                            } else if (double_tap_hold && is_clicking) {
                                // Release double-tap hold
                                double_tap_hold = false;
                                is_clicking = false;
                                temp_report.buttons &= ~MOUSE_BTN1;
                            }

                            // Reset tap count if too much time passed
                            if (timer_elapsed(last_tap_time) > 600) {
                                tap_count = 0;
                            }
                        }
                    }

                    previous_valid = false;
                    finger_2_prev_valid = false;
                }

                // Maintain double-tap hold click
                if (double_tap_hold && is_clicking) {
                    temp_report.buttons |= MOUSE_BTN1;
                }

            } else {
                dprintf("IQS7211E: Get report failed, i2c status: %d\n", status);
            }
        }
    } else {
        dprintf("IQS7211E: Init failed, i2c status: %d, %d\n", azoteq_iqs7211e_init_status, azoteq_iqs7211e_product_number);
    }

    return temp_report;
}

void pointing_device_driver_init(void) {
    azoteq_iqs7211e_init();
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    return azoteq_iqs7211e_get_report(mouse_report);
}

void pointing_device_driver_set_cpi(uint16_t cpi) {
    azoteq_iqs7211e_set_cpi(cpi);
}

uint16_t pointing_device_driver_get_cpi(void) {
    return azoteq_iqs7211e_get_cpi();
}