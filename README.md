# Inpuut Behavior Listener x Auto Toggle Mouse Key Layer

This module added behavior bindings to input config of input subsystem for ZMK.

## What it does

The module make a clone version of original input_listener.c, modified to allow a config being only enabled on specific layers. Also, allow to add behavior bindings per config. A simple input behavior 'auto toggle layer' is presented, it shows in practical case of auto toggle 'mouse key layer' while input events arriving from device and then switch off after a dedicated period.

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
                compatible = "zmk,input-behavior-listener";
                
                /* the input point device alias */
                device = <&pd0>;

                /* only enable in layer DEF & MSK */
                layers = <DEF MSK>;

                /* event code value to override raw input event */
                evt-type = <INPUT_EV_REL>;
                x-input-code = <INPUT_REL_X>;
                y-input-code = <INPUT_REL_Y>;
                scale-multiplier = <1>;
                scale-divisor = <1>;

                /* bind a behavior to auto activate MSK layer for &mkp */
                bindings = <&ib_tog_layer MSK>;
        };
  
        /* input config for mouse scroll mode on high order layer (MSC) */
        tb0_msl_ibl {
                compatible = "zmk,input-behavior-listener";
                device = <&pd0>;
                layers = <MSC>;
                evt-type = <INPUT_EV_REL>;
                x-input-code = <INPUT_REL_MISC>;
                y-input-code = <INPUT_REL_WHEEL>;
                scale-multiplier = <1>;
                scale-divisor = <8>;
                y-invert;
        };

        /* define how long to last for mouse key layer after activated */
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
