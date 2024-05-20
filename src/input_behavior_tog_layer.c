/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_behavior_tog_layer

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/behavior.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/keymap.h>
#include <zmk/behavior.h>

// #if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_tog_layer_config {
    uint32_t time_to_live_ms;
};

struct behavior_tog_layer_data {
    uint8_t toggle_layer;
    struct k_work_delayable toggle_layer_activate_work;
    struct k_work_delayable toggle_layer_deactivate_work;
    const struct device *dev;
};

static void toggle_layer_deactivate_cb(struct k_work *work) {
    struct k_work_delayable *work_delayable = (struct k_work_delayable *)work;
    struct behavior_tog_layer_data *data = CONTAINER_OF(work_delayable, 
                                                        struct behavior_tog_layer_data,
                                                        toggle_layer_deactivate_work);
    if (!zmk_keymap_layer_active(data->toggle_layer)) {
      return;
    }
    LOG_DBG("deactivate layer %d", data->toggle_layer);
    zmk_keymap_layer_deactivate(data->toggle_layer);
}

static void toggle_layer_activate_cb(struct k_work *work) {
    struct k_work_delayable *work_delayable = (struct k_work_delayable *)work;
    struct behavior_tog_layer_data *data = CONTAINER_OF(work_delayable, 
                                                        struct behavior_tog_layer_data,
                                                        toggle_layer_activate_work);
    LOG_DBG("activate layer %d", data->toggle_layer);
    zmk_keymap_layer_activate(data->toggle_layer);
}

static int to_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_tog_layer_data *data = (struct behavior_tog_layer_data *)dev->data;
    const struct behavior_tog_layer_config *cfg = dev->config;
    data->toggle_layer = binding->param1;
    if (!zmk_keymap_layer_active(data->toggle_layer)) {
        // LOG_DBG("schedule activate layer %d", data->toggle_layer);
        k_work_schedule(&data->toggle_layer_activate_work, K_MSEC(0));
    }
    k_work_schedule(&data->toggle_layer_deactivate_work, K_MSEC(cfg->time_to_live_ms));
    return ZMK_BEHAVIOR_TRANSPARENT;
}

static int input_behavior_to_init(const struct device *dev) {
    struct behavior_tog_layer_data *data = dev->data;
    data->dev = dev;
    k_work_init_delayable(&data->toggle_layer_activate_work, toggle_layer_activate_cb);
    k_work_init_delayable(&data->toggle_layer_deactivate_work, toggle_layer_deactivate_cb);
    return 0;
};

static const struct behavior_driver_api behavior_tog_layer_driver_api = {
    .binding_pressed = to_keymap_binding_pressed,
};

#define KP_INST(n)                                                                      \
    static struct behavior_tog_layer_data behavior_tog_layer_data_##n = {};             \
    static struct behavior_tog_layer_config behavior_tog_layer_config_##n = {           \
        .time_to_live_ms = DT_INST_PROP(n, time_to_live_ms),                            \
    };                                                                                  \
    BEHAVIOR_DT_INST_DEFINE(n, input_behavior_to_init, NULL,                            \
                            &behavior_tog_layer_data_##n,                               \
                            &behavior_tog_layer_config_##n,                             \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                            &behavior_tog_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

// #endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
