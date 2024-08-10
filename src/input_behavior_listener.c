/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_behavior_listener

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// #if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <zmk/endpoints.h>
#include <zmk/hid.h>
#include <zmk/mouse/hid.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_INPUT_BEHAVIOR_LISTENER_USE_HID_IO)
    #if IS_ENABLED(CONFIG_ZMK_HID_IO)
        #define USE_HID_IO 1
        #include <zmk/hid-io/endpoints.h>
        #include <zmk/hid-io/hid.h>
    #endif
#endif

#include <math.h>
#ifndef M_PI
#define M_PI (3.14159265358979323846f)
#endif
#include <zephyr/sys/util.h> // for CLAMP

#define ONE_IF_DEV_OK(n)                                                                           \
    COND_CODE_1(DT_NODE_HAS_STATUS(DT_INST_PHANDLE(n, device), okay), (1 +), (0 +))

#define VALID_LISTENER_COUNT (DT_INST_FOREACH_STATUS_OKAY(ONE_IF_DEV_OK) 0)

#if VALID_LISTENER_COUNT > 0

enum input_behavior_listener_xy_data_mode {
    INPUT_LISTENER_XY_DATA_MODE_NONE,
    INPUT_LISTENER_XY_DATA_MODE_REL,
    INPUT_LISTENER_XY_DATA_MODE_ABS,
};

struct input_behavior_listener_xy_data {
    enum input_behavior_listener_xy_data_mode mode;
    int16_t x;
    int16_t y;
};

struct input_behavior_listener_data {
    union {
        struct {
            float sin;
            float cos;

            struct input_behavior_listener_xy_data data;
            struct input_behavior_listener_xy_data wheel_data;

            uint8_t button_set;
            uint8_t button_clear;
        } mouse;
    };
};

struct input_behavior_listener_config {
    bool xy_swap;
    bool x_invert;
    bool y_invert;
    uint16_t scale_multiplier;
    uint16_t scale_divisor;
    uint16_t rotate_deg;
    int8_t evt_type;
    int8_t x_input_code;
    int8_t y_input_code;
    uint8_t layers_count;
    uint8_t layers[ZMK_KEYMAP_LAYERS_LEN];
    uint8_t bindings_count;
    struct zmk_behavior_binding bindings[];
};

static void handle_rel_code(const struct input_behavior_listener_config *config,
                            struct input_behavior_listener_data *data, struct input_event *evt) {
    switch (evt->code) {
    case INPUT_REL_X:
        data->mouse.data.mode = INPUT_LISTENER_XY_DATA_MODE_REL;
        data->mouse.data.x += evt->value;
        break;
    case INPUT_REL_Y:
        data->mouse.data.mode = INPUT_LISTENER_XY_DATA_MODE_REL;
        data->mouse.data.y += evt->value;
        break;
    case INPUT_REL_WHEEL:
        data->mouse.wheel_data.mode = INPUT_LISTENER_XY_DATA_MODE_REL;
        data->mouse.wheel_data.y += evt->value;
        break;
    case INPUT_REL_HWHEEL:
        data->mouse.wheel_data.mode = INPUT_LISTENER_XY_DATA_MODE_REL;
        data->mouse.wheel_data.x += evt->value;
        break;
    default:
        break;
    }
}

static void handle_abs_code(const struct input_behavior_listener_config *config,
                            struct input_behavior_listener_data *data, struct input_event *evt) {}

static void handle_key_code(const struct input_behavior_listener_config *config,
                            struct input_behavior_listener_data *data, struct input_event *evt) {
    int8_t btn;

    switch (evt->code) {
    case INPUT_BTN_0:
    case INPUT_BTN_1:
    case INPUT_BTN_2:
    case INPUT_BTN_3:
    case INPUT_BTN_4:
        btn = evt->code - INPUT_BTN_0;
        if (evt->value > 0) {
            WRITE_BIT(data->mouse.button_set, btn, 1);
        } else {
            WRITE_BIT(data->mouse.button_clear, btn, 1);
        }
        break;
    default:
        break;
    }
}

static void swap_xy(struct input_event *evt) {
    switch (evt->code) {
    case INPUT_REL_X:
        evt->code = INPUT_REL_Y;
        break;
    case INPUT_REL_Y:
        evt->code = INPUT_REL_X;
        break;
    case INPUT_REL_WHEEL:
        evt->code = INPUT_REL_HWHEEL;
        break;
    case INPUT_REL_HWHEEL:
        evt->code = INPUT_REL_WHEEL;
        break;
    }
}

static inline bool is_x_data(const struct input_event *evt) {
    return evt->type == INPUT_EV_REL && (evt->code == INPUT_REL_X || evt->code == INPUT_REL_HWHEEL);
}

static inline bool is_y_data(const struct input_event *evt) {
    return evt->type == INPUT_EV_REL && (evt->code == INPUT_REL_Y || evt->code == INPUT_REL_WHEEL);
}

static bool intercept_with_input_config(const struct input_behavior_listener_config *cfg,
                                        struct input_event *evt) {
    if (!evt->dev) {
        return false;
    }

    uint8_t layer = zmk_keymap_highest_layer_active();
    bool active_layer_check = false;
    for (uint8_t i = 0; i < cfg->layers_count && !active_layer_check; i++) {
        active_layer_check |= layer == cfg->layers[i];
    }
    if (!active_layer_check) {
        return false;
    }

    if (cfg->evt_type >= 0 && evt->type == cfg->evt_type) {
        if ((evt->code == INPUT_REL_X) || (evt->code == INPUT_REL_HWHEEL)) {
            if (cfg->x_input_code >= 0) {
                evt->code = cfg->x_input_code;
            }
        }
        else if ((evt->code == INPUT_REL_Y) || (evt->code == INPUT_REL_WHEEL)) {
            if (cfg->y_input_code >= 0) {
                evt->code = cfg->y_input_code;
            }
        }
    }

    if (cfg->xy_swap) {
        swap_xy(evt);
    }

    if ((cfg->x_invert && is_x_data(evt)) || (cfg->y_invert && is_y_data(evt))) {
        evt->value = -(evt->value);
    }

    evt->value = (int16_t)((evt->value * cfg->scale_multiplier) / cfg->scale_divisor);

    bool to_be_intercapted = true;

    for (uint8_t b = 0; b < cfg->bindings_count; b++) {
        struct zmk_behavior_binding binding = cfg->bindings[b];
        // LOG_DBG("layer: %d input: %s, binding name: %s", layer, evt->dev->name, binding.behavior_dev);

        const struct device *behavior = zmk_behavior_get_binding(binding.behavior_dev);
        if (!behavior) {
            LOG_WRN("No behavior assigned to %s on layer %d", evt->dev->name, layer);
            continue;
        }

        const struct behavior_driver_api *api = (const struct behavior_driver_api *)behavior->api;
        int ret = ZMK_BEHAVIOR_TRANSPARENT;

        if (api->binding_pressed || api->binding_released) {

            struct zmk_behavior_binding_event event = {
                .layer = layer, .timestamp = k_uptime_get(),
                .position = (struct input_event *)evt, // util uint32_t to pass event ptr :)
            };

            bool state = true;
            if (evt->type == INPUT_EV_KEY) {
                if (evt->code >= INPUT_BTN_0 && evt->code <= INPUT_BTN_8) {
                    state = (evt->value > 0);
                }
            }

            if (api->binding_pressed && state) {
                ret = api->binding_pressed(&binding, event);
            }
            else if (api->binding_released && !state) {
                ret = api->binding_released(&binding, event);
            }

        }
        else if (api->sensor_binding_process) {

            struct zmk_behavior_binding_event event = {
                .layer = layer, .timestamp = k_uptime_get(),
                .position = 0,
            };
            if (api->sensor_binding_accept_data) {
                const struct zmk_sensor_config *sensor_config = 
                    (const struct zmk_sensor_config *)cfg;
                const struct zmk_sensor_channel_data val[] = {
                    { .value = { .val1 = (struct input_event *)evt },
                    .channel = SENSOR_CHAN_ALL, },
                };
                int ret = behavior_sensor_keymap_binding_accept_data(
                    &binding, event, sensor_config, sizeof(val), val);
                if (ret < 0) {
                    LOG_WRN("behavior data accept for behavior %s returned an error (%d). "
                            "Processing to continue to next layer",  binding.behavior_dev, ret);
                }
            }
            enum behavior_sensor_binding_process_mode mode =
                    BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER;
            ret = behavior_sensor_keymap_binding_process(&binding, event, mode);

        }

        if (ret == ZMK_BEHAVIOR_OPAQUE) {
            // LOG_DBG("input event processing complete, behavior response was opaque");
            to_be_intercapted = false;
            break;
        } else if (ret < 0) {
            // LOG_DBG("input behavior returned error: %d", ret);
            return ret;
        }
    }

    return to_be_intercapted;
}

static void clear_xy_data(struct input_behavior_listener_xy_data *data) {
    data->x = data->y = 0;
    data->mode = INPUT_LISTENER_XY_DATA_MODE_NONE;
}

static void input_behavior_handler(const struct input_behavior_listener_config *config,
                                   struct input_behavior_listener_data *data, 
                                   struct input_event *evt) {
    // First, filter to update the event data as needed.
    if (!intercept_with_input_config(config, evt)) {
        return;
    }

    switch (evt->type) {
    case INPUT_EV_REL:
        handle_rel_code(config, data, evt);
        break;
    case INPUT_EV_ABS:
        handle_abs_code(config, data, evt);
        break;
    case INPUT_EV_KEY:
        handle_key_code(config, data, evt);
        break;
    }

    if (evt->sync) {
        if (data->mouse.wheel_data.mode == INPUT_LISTENER_XY_DATA_MODE_REL) {
            if (config->rotate_deg > 0) {
                float x = data->mouse.wheel_data.x;
                float y = data->mouse.wheel_data.y;
                data->mouse.wheel_data.x = (data->mouse.cos * x) - (data->mouse.sin * y);
                data->mouse.wheel_data.y = (data->mouse.sin * x) + (data->mouse.cos * y);
            }
            #if USE_HID_IO
                #if IS_ENABLED(CONFIG_ZMK_HID_IO_MOUSE)
                zmk_hid_mou2_scroll_set(data->mouse.wheel_data.x, data->mouse.wheel_data.y);
                #elif IS_ENABLED(CONFIG_ZMK_HID_IO_JOYSTICK)
                // no joystick scroll implemented
                #endif
            #elif IS_ENABLED(CONFIG_ZMK_MOUSE)
                zmk_hid_mouse_scroll_set(data->mouse.wheel_data.x, data->mouse.wheel_data.y);
            #endif
        }

        if (data->mouse.data.mode == INPUT_LISTENER_XY_DATA_MODE_REL) {
            if (config->rotate_deg > 0) {
                float x = data->mouse.data.x;
                float y = data->mouse.data.y;
                data->mouse.data.x = (data->mouse.cos * x) - (data->mouse.sin * y);
                data->mouse.data.y = (data->mouse.sin * x) + (data->mouse.cos * y);
            }
            #if USE_HID_IO
                #if IS_ENABLED(CONFIG_ZMK_HID_IO_MOUSE)
                zmk_hid_mou2_movement_set(data->mouse.data.x, data->mouse.data.y);
                #elif IS_ENABLED(CONFIG_ZMK_HID_IO_JOYSTICK)
                zmk_hid_joy2_movement_set(data->mouse.data.x, data->mouse.data.y);
                #endif
            #elif IS_ENABLED(CONFIG_ZMK_MOUSE)
                zmk_hid_mouse_movement_set(data->mouse.data.x, data->mouse.data.y);
            #endif
        }

        if (data->mouse.button_set != 0) {
            for (int i = 0; i < ZMK_MOUSE_HID_NUM_BUTTONS; i++) {
                if ((data->mouse.button_set & BIT(i)) != 0) {
                    #if USE_HID_IO
                        #if IS_ENABLED(CONFIG_ZMK_HID_IO_MOUSE)
                        zmk_hid_mou2_button_press(i);
                        #elif IS_ENABLED(CONFIG_ZMK_HID_IO_JOYSTICK)
                        zmk_hid_joy2_button_press(i);
                        #endif
                    #elif IS_ENABLED(CONFIG_ZMK_MOUSE)
                        zmk_hid_mouse_button_press(i);
                    #endif
                }
            }
        }

        if (data->mouse.button_clear != 0) {
            for (int i = 0; i < ZMK_MOUSE_HID_NUM_BUTTONS; i++) {
                if ((data->mouse.button_clear & BIT(i)) != 0) {
                    #if USE_HID_IO
                        #if IS_ENABLED(CONFIG_ZMK_HID_IO_MOUSE)
                        zmk_hid_mou2_button_release(i);
                        #elif IS_ENABLED(CONFIG_ZMK_HID_IO_JOYSTICK)
                        zmk_hid_joy2_button_release(i);
                        #endif
                    #elif IS_ENABLED(CONFIG_ZMK_MOUSE)
                        zmk_hid_mouse_button_release(i);
                    #endif
                }
            }
        }

        #if USE_HID_IO
            #if IS_ENABLED(CONFIG_ZMK_HID_IO_MOUSE)
            zmk_endpoints_send_mouse_report_alt();
            zmk_hid_mou2_scroll_set(0, 0);
            zmk_hid_mou2_movement_set(0, 0);
            #elif IS_ENABLED(CONFIG_ZMK_HID_IO_JOYSTICK)
            zmk_endpoints_send_joystick_report_alt();
            zmk_hid_joy2_movement_set(0, 0);
            // no joystick scroll implemented
            #endif
        #elif IS_ENABLED(CONFIG_ZMK_MOUSE)
            zmk_endpoints_send_mouse_report();
            zmk_hid_mouse_scroll_set(0, 0);
            zmk_hid_mouse_movement_set(0, 0);
        #endif

        clear_xy_data(&data->mouse.data);
        clear_xy_data(&data->mouse.wheel_data);

        data->mouse.button_set = data->mouse.button_clear = 0;
    }
}

#endif // VALID_LISTENER_COUNT > 0

#define IBL_EXTRACT_BINDING(idx, drv_inst)                                                         \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(drv_inst, bindings, idx)),           \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(drv_inst, bindings, idx, param1), (0),   \
                              (DT_INST_PHA_BY_IDX(drv_inst, bindings, idx, param1))),              \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(drv_inst, bindings, idx, param2), (0),   \
                              (DT_INST_PHA_BY_IDX(drv_inst, bindings, idx, param2))),              \
    }

#define IBL_INST(n)                                                                                \
    COND_CODE_1(                                                                                   \
        DT_NODE_HAS_STATUS(DT_INST_PHANDLE(n, device), okay),                                      \
        (static const struct input_behavior_listener_config config_##n = {                         \
            .xy_swap = DT_INST_PROP(n, xy_swap),                                                   \
            .x_invert = DT_INST_PROP(n, x_invert),                                                 \
            .y_invert = DT_INST_PROP(n, y_invert),                                                 \
            .scale_multiplier = DT_INST_PROP(n, scale_multiplier),                                 \
            .scale_divisor = DT_INST_PROP(n, scale_divisor),                                       \
            .rotate_deg = DT_INST_PROP(n, rotate_deg),                                             \
            .evt_type = DT_INST_PROP(n, evt_type),                                                 \
            .x_input_code = DT_INST_PROP(n, x_input_code),                                         \
            .y_input_code = DT_INST_PROP(n, y_input_code),                                         \
            .layers_count = DT_INST_PROP_LEN(n, layers),                                           \
            .layers = DT_INST_PROP(n, layers),                                                     \
            .bindings_count = COND_CODE_1(                                                         \
                DT_INST_NODE_HAS_PROP(n, bindings),                                                \
                (DT_INST_PROP_LEN(n, bindings)), (0)),                                             \
            .bindings = COND_CODE_1(                                                               \
                DT_INST_NODE_HAS_PROP(n, bindings),                                                \
                ({LISTIFY(DT_INST_PROP_LEN(n, bindings), IBL_EXTRACT_BINDING, (, ), n)}),          \
                ({})),                                                                             \
        };                                                                                         \
        static struct input_behavior_listener_data data_##n = {                                    \
            .mouse = {                                                                             \
                .sin = sinf((DT_INST_PROP(n, rotate_deg) * M_PI / 180.0f)),                        \
                .cos = cosf((DT_INST_PROP(n, rotate_deg) * M_PI / 180.0f)),                        \
            },                                                                                     \
        };                                                                                         \
        void input_behavior_handler_##n(struct input_event *evt) {                                 \
            input_behavior_handler(&config_##n, &data_##n, evt);                                   \
        }                                                                                          \
        INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_INST_PHANDLE(n, device)),                           \
                             input_behavior_handler_##n);),                                        \
        ())

DT_INST_FOREACH_STATUS_OKAY(IBL_INST)

// #endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
