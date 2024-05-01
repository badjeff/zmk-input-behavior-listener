# Inpuut Behavior Listener x Auto Toggle Mouse Key Layer

This module add behaviors to input config of input subsystem for ZMK.

## What it does

The module fork a version of `input_listener.c` as new compatible `zmk,input-behavior-listener` to intercept input events. Make input events being only enabling on specific `layers`. Also, adding `evt-type` and behavior `bindings` for pre-processing similar to conventional behavior mechanism. An input behavior `zmk,input-behavior-tog-layer` is presented, to show a practical user case of auto-toggle 'mouse key layer'. It would be triggered via `behavior_driver_api->binding_pressed()`, on input event raised and then switch off on idle after `time-to-live-ms`.

## Installation

Include this project on your ZMK's west manifest in `config/west.yml`:

```yaml
manifest:
  ...
  projects:
    ...
    - name: zmk-input-behavior-listener
      remote: badjeff
      revision: main
    ...
```

Now, update your `shield.keymap` adding the behaviors.

```keymap

/* include &mkp  */
#include <behaviors/mouse_keys.dtsi>

// index of keymap layers
#define DEF 0 // default layer
#define MSK 1 // mouse key layer
#define MSC 2 // mouse scoll layer

/ {
        /* input config for mouse move mode on default layer (DEF & MSK) */
        tb0_mmv_ibl {
                /* new forked compatible name */
                compatible = "zmk,input-behavior-listener";
                
                /* the input point device alias */
                device = <&pd0>;

                /* only enable in default layer (DEF) & mouse key layer (MSK) */
                layers = <DEF MSK>;

                /* event code value to override raw input event */
                /* designed for switching to mouse scroll, xy-swap, precise-mode+, etc */
                /* NOTE: only apply input-code overriding for INPUT_EV_REL */
                evt-type = <INPUT_EV_REL>;
                x-input-code = <INPUT_REL_X>;
                y-input-code = <INPUT_REL_Y>;
                scale-multiplier = <1>;
                scale-divisor = <1>;

                /* bind a behavior to auto activate MSK layer for &mkp */
                bindings = <&ib_tog_layer MSK>;

                /* align to CCW 45 degree */
                rotate-deg = <315>;
                /* NOTE 1: This settings do not compitable with y-invert and x-invert */
                /* NOTE 2: Floating point computation requires alot of ram. */
                /*         This feature will cuase stackove flow with CONFIG_ZMK_USB_LOGGING=y */
        };
  
        /* input config for mouse scroll mode on momentary mouse scoll layer (MSC) */
        tb0_msl_ibl {
                compatible = "zmk,input-behavior-listener";
                device = <&pd0>;
                layers = <MSC>;
                evt-type = <INPUT_EV_REL>;
                
                /* slienting x-axis with alt event code */
                x-input-code = <INPUT_REL_MISC>;
                y-input-code = <INPUT_REL_WHEEL>;

                /* slow it down, and invent scrolling direction */
                scale-multiplier = <1>;
                scale-divisor = <8>;
                y-invert;

                /* align to CCW 45 degree */
                rotate-deg = <315>;
                /* NOTE 1: This settings do not compitable with y-invert and x-invert */
                /* NOTE 2: Floating point computation requires alot of ram. */
                /*         This feature will cuase stackove flow with CONFIG_ZMK_USB_LOGGING=y */
        };

        /* adjust cooldown waiting period for mouse key layer (MSK) after activated */
        ib_tog_layer: ib_tog_layer {
                compatible = "zmk,input-behavior-tog-layer";
                #binding-cells = <1>;
                time-to-live-ms = <1000>;
        };

        keymap {
                compatible = "zmk,keymap";
                DEF_layer {
                        bindings = < &mo MSK .... ... >;
                };
                MSK_layer {
                        bindings = < ..... &mkp LCLK  &mo MSC >;
                };
                MSC_layer {
                        bindings = < ..... &mkp LCLK  ... >;
                };
       };

};
```

## Troubleshooting

If you got compile error of `undefined reference to 'zmk_hid_mouse_XXXXXX_set'`, you are probably need to build with a ZMK branch with [PR 2027](https://github.com/zmkfirmware/zmk/pull/2027) merged. Without PR 2027, the mouse movement is not presented via HID Report and your cursor won't reflect the readings.

Or, you might try to develop an alttnative HID Usage Page with an experminatal module ([zmk-hid-io](https://github.com/badjeff/zmk-hid-io)) by adding below config in your `<shield>.config`.

```conf
# Enable input behavior listener to use HID IO report
CONFIG_ZMK_INPUT_BEHAVIOR_LISTENER_USE_HID_IO=y
```
