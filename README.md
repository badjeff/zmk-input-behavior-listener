# ZMK Input Behavior Listener

This module add behaviors to input config of input subsystem for ZMK.

## What it does

The module fork the `input_listener.c` add extra configs declared as new a compatible `zmk,input-behavior-listener`, as an optional replacement of official `zmk,input-listener`. It intercepts input events from sensor device, only enabling on specific `layers`, adding `evt-type` and behavior `bindings` for pre-processing via the `input-behavior` bindings.

Input-Behavior:
- `zmk,input-behavior-tog-layer`: Auto Toggle Mouse Key Layer, a.k.a auto-mouse-layer. An input behavior `zmk,input-behavior-tog-layer` is presented, to show a practical user case of auto-toggle 'mouse key layer'. It would be triggered via `behavior_driver_api->binding_pressed()`, on input event raised and then switch off on idle after `time-to-live-ms`.
- `zmk,input-behavior-scaler`: Input Resolution Scaler, a behavior to accumulate delta value before casting to integer, that allows precise scrolling and better linear acceleration on each axis of input device. Some retangular trackpad needs separated scale factor after swaping X/Y axis.
- `zmk,input-behavior-mixer`: TBD, no schedule (yet).

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
                /*         This feature will cuase stackover flow with CONFIG_ZMK_USB_LOGGING=y */
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

                /* invent scrolling direction */
                y-invert;

                /* align to CCW 45 degree */
                rotate-deg = <315>;
                /* NOTE 1: This settings do not compitable with y-invert and x-invert */
                /* NOTE 2: Floating point computation requires alot of ram. */
                /*         This feature will cuase stackove flow with CONFIG_ZMK_USB_LOGGING=y */

                /* bind a behavior to down scaling input value to (1/8) */
                /* NOTE: This behavior memorizes recent pending displacement, it is different to scale-divisor. */
                /*       The delta value is accumlated until result >= 1 after cast. */
                /*       The scrolling will be smoother and allow precise scrolling */
                bindings = <&ib_wheel_scaler 1 8>;
        };

        /* adjust cooldown waiting period for mouse key layer (MSK) after activated */
        ib_tog_layer: ib_tog_layer {
                compatible = "zmk,input-behavior-tog-layer";
                #binding-cells = <1>;
                time-to-live-ms = <1000>;
        };

        /* define a resolution down scaler only for INPUT_REL_WHEEL */
        ib_wheel_scaler: ib_wheel_scaler {
                compatible = "zmk,input-behavior-scaler";
                #binding-cells = <2>;
                evt-type = <INPUT_EV_REL>;
                input-code = <INPUT_REL_WHEEL>;
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

Or, you might try to develop an alttnative HID Usage Page with an experminatal module ([zmk-hid-io](https://github.com/badjeff/zmk-hid-io)).
