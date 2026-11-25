# Olly BB rev2: personal firmware (diegope)

Personal build for a Duck Blackbird with a Mechlovin Olly BB rev 2.1 PCB
(STM32F103, stm32duino bootloader). It's based on Mechlovin's
`olly_bb_rev2` branch.

## Changes from stock

- **Lock LED fix:** caps, num and scroll lock pins are set as outputs, so
  the LEDs run at full brightness instead of glowing faintly through the
  internal pull-up.
- **Lock LED order:** num, caps, scroll from left to right.
- **Indicator LEDs (right group of five):**
  - LEDs 1–4: held (or one-shot) Shift, Ctrl, Alt, GUI.
  - LED 5: Fn held (any layer above the base layer).
  - Space bar: all five light while held, then fade out over about 250 ms.
  - All off while the host is asleep or no host is active.
- **Fading:** the indicator LEDs use software PWM on TIM4: 64 levels at a
  250 Hz PWM rate.

## Build

```
make mechlovin/olly/bb/rev2:default
```

## Flash

1. Hold **Esc** while plugging in the keyboard. QMK Toolbox shows
   "Olly BB Boot connected" (1EAF:0003).
2. Flash the `.bin` file with QMK Toolbox, or run:

   ```
   dfu-util -d 1eaf:0003 -a 2 -D mechlovin_olly_bb_rev2_default.bin -R
   ```

The bootloader can't read firmware back, so keep a copy of any `.bin`
you rely on.

## VIA

VIA's database has no entry for this board's ID (0x4D4C / 0xD183). Sideload
`via/olly_bb_rev2_via.json` from VIA's Design tab: Settings → Show Design
tab, then Design → Load.
