/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_behavior_scaler

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/keymap.h>
#include <zmk/behavior.h>

#if IS_ENABLED(CONFIG_ZMK_HID_IO)
#include <zmk/hid-io/endpoints.h>
#include <zmk/hid-io/hid.h>
#endif

// #if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

enum scaler_xy_data_mode {
    IB_SCALER_XY_DATA_MODE_NONE,
    IB_SCALER_XY_DATA_MODE_REL,
    IB_SCALER_XY_DATA_MODE_ABS,
};

struct scaler_xy_data {
    enum scaler_xy_data_mode mode;
    int16_t delta;
};

struct behavior_scaler_data {
    const struct device *dev;
    struct scaler_xy_data data;
};

struct behavior_scaler_config {
    int8_t evt_type;
    int8_t input_code;
};

static void handle_rel_code(const struct behavior_scaler_config *config,
                            struct behavior_scaler_data *data, struct input_event *evt) {
    switch (evt->code) {
    case INPUT_REL_X:
    case INPUT_REL_Y:
    case INPUT_REL_WHEEL:
    case INPUT_REL_HWHEEL:
    case INPUT_REL_MISC:
        data->data.mode = IB_SCALER_XY_DATA_MODE_REL;
        data->data.delta += evt->value;
        break;
    default:
        break;
    }
}

static void handle_abs_code(const struct behavior_scaler_config *config,
                            struct behavior_scaler_data *data, struct input_event *evt) {
}

static int scaler_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_scaler_data *data = 
        (struct behavior_scaler_data *)dev->data;
    const struct behavior_scaler_config *config = dev->config;
    
    struct input_event *evt = (struct input_event *)event.position;
    if (evt->type != config->evt_type) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    if (evt->code != config->input_code) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    if (!evt->value) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    switch (evt->type) {
    case INPUT_EV_REL:
        handle_rel_code(config, data, evt);
        break;
    case INPUT_EV_ABS:
        handle_abs_code(config, data, evt);
        break;
    default:
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    if (data->data.mode == IB_SCALER_XY_DATA_MODE_REL) {
        int16_t mul = binding->param1;
        if (!mul) {
            evt->value = 0;
            // LOG_DBG("Suu~~~!");
            return ZMK_BEHAVIOR_OPAQUE;
        }
        int16_t div = binding->param2;
        int16_t delta = data->data.delta;
        int16_t sval = delta * mul / div;
        // LOG_DBG("* %d / %d > delta: %d => %d", mul, div, delta, sval);
        if (sval) {
            data->data.mode = IB_SCALER_XY_DATA_MODE_NONE;
            data->data.delta = 0;
            evt->value = sval;
            // LOG_DBG("* %d / %d > delta: %d => %d", mul, div, delta, sval);
            return ZMK_BEHAVIOR_TRANSPARENT;
        } else {
            return ZMK_BEHAVIOR_OPAQUE;
        }
    }

    return ZMK_BEHAVIOR_TRANSPARENT;
}

static int input_behavior_to_init(const struct device *dev) {
    struct behavior_scaler_data *data = dev->data;
    data->dev = dev;
    return 0;
};

static const struct behavior_driver_api behavior_scaler_driver_api = {
    .binding_pressed = scaler_keymap_binding_pressed,
};

#define IBSLR_INST(n)                                                                       \
    static struct behavior_scaler_data behavior_scaler_data_##n = {};                       \
    static struct behavior_scaler_config behavior_scaler_config_##n = {                     \
        .evt_type = DT_INST_PROP(n, evt_type),                                              \
        .input_code = DT_INST_PROP(n, input_code),                                          \
    };                                                                                      \
    BEHAVIOR_DT_INST_DEFINE(n, input_behavior_to_init, NULL,                                \
                            &behavior_scaler_data_##n,                                      \
                            &behavior_scaler_config_##n,                                    \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,               \
                            &behavior_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IBSLR_INST)

// #endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
