/* Copyright 2023 Mechlovin'
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "quantum.h"
#include "usb_device_state.h"

void board_init(void) {
    AFIO->MAPR |= AFIO_MAPR_SPI1_REMAP;
   //JTAG-DP Disabled and SW-DP Enabled
   AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG_Msk) | AFIO_MAPR_SWJ_CFG_DISABLE;
}

void led_init_ports(void) {
  gpio_set_pin_output(B11); // right lock LED (scroll)
  gpio_set_pin_output(B10); // middle lock LED (caps)
  gpio_set_pin_output(B0);  // left lock LED (num)
  gpio_set_pin_output(B12);
  gpio_set_pin_output(A14);
  gpio_set_pin_output(A13);
  gpio_set_pin_output(A10);
  gpio_set_pin_output(A9);

}


// ---------------------------------------------------------------------------
// Layer LEDs
//
// The five layer LEDs are dimmed with software PWM driven by a hardware
// timer (TIM4), so they can fade. 64 brightness steps at 16 kHz gives a
// 250 Hz PWM rate, which is flicker-free.
//
// What they show:
//   - Space bar: all five flash on and fade out after release.
//   - LEDs 1-4: held (or one-shot) modifiers: Shift, Ctrl, Alt, GUI.
//   - LED 5: Fn held (any layer above the base layer).
//   - Host asleep, or no host: all off.
// ---------------------------------------------------------------------------

static const pin_t layer_leds[] = {B12, A14, A13, A9, A10};
#define LAYER_LED_COUNT (sizeof(layer_leds) / sizeof(layer_leds[0]))

#define PWM_STEPS 64         // brightness levels per LED
#define PWM_TICK_HZ 16000    // PWM_STEPS * 250 Hz
#define FADE_MS 250          // space bar fade-out time

// 0 = off, PWM_STEPS = fully on. Written by the main loop, read by the timer.
static volatile uint8_t led_level[LAYER_LED_COUNT];

static void pwm_tick(GPTDriver *gptp) {
    (void)gptp;
    static uint8_t phase = 0;
    phase = (phase + 1) % PWM_STEPS;
    for (uint8_t i = 0; i < LAYER_LED_COUNT; i++) {
        gpio_write_pin(layer_leds[i], led_level[i] > phase);
    }
}

static const GPTConfig pwm_gpt_config = {
    .frequency = 1000000, // 1 MHz timer clock
    .callback  = pwm_tick,
    .cr2       = 0,
    .dier      = 0,
};

void keyboard_post_init_kb(void) {
    gptStart(&GPTD4, &pwm_gpt_config);
    gptStartContinuous(&GPTD4, 1000000 / PWM_TICK_HZ);
    keyboard_post_init_user();
}

// Perceived brightness is roughly the square of the duty cycle, so square the
// fade so it looks smooth to the eye. Input/output: 0..PWM_STEPS.
static uint8_t perceptual(uint16_t linear) {
    return (linear * linear + PWM_STEPS / 2) / PWM_STEPS;
}

static bool     space_held     = false;
static bool     space_fading   = false;
static uint16_t space_released = 0;

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (keycode == KC_SPC) {
        space_held = record->event.pressed;
        if (!space_held) {
            space_fading   = true;
            space_released = timer_read();
        }
    }
    return process_record_user(keycode, record);
}

static bool host_active(void) {
    return usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED;
}

static void set_all_levels(uint8_t level) {
    for (uint8_t i = 0; i < LAYER_LED_COUNT; i++) {
        led_level[i] = level;
    }
}

// Runs every scan loop.
void housekeeping_task_kb(void) {
    if (!host_active()) {
        set_all_levels(0);
        housekeeping_task_user();
        return;
    }

    // Base state: which LEDs are on at full brightness.
    // LEDs 1-4: held (or one-shot) modifiers. LED 5: Fn (any layer above 0).
    uint8_t mask = 0;
    uint8_t mods = get_mods() | get_oneshot_mods();
    if (mods & MOD_MASK_SHIFT) mask |= 1 << 0;
    if (mods & MOD_MASK_CTRL) mask |= 1 << 1;
    if (mods & MOD_MASK_ALT) mask |= 1 << 2;
    if (mods & MOD_MASK_GUI) mask |= 1 << 3;
    if (get_highest_layer(layer_state | default_layer_state) > 0) mask |= 1 << 4;

    // Space bar flash: full while held, then fade out.
    uint8_t flash = 0;
    if (space_held) {
        flash = PWM_STEPS;
    } else if (space_fading) {
        uint16_t elapsed = timer_elapsed(space_released);
        if (elapsed >= FADE_MS) {
            space_fading = false;
        } else {
            flash = perceptual((uint32_t)PWM_STEPS * (FADE_MS - elapsed) / FADE_MS);
        }
    }

    for (uint8_t i = 0; i < LAYER_LED_COUNT; i++) {
        uint8_t base = (mask & (1 << i)) ? PWM_STEPS : 0;
        led_level[i] = base > flash ? base : flash;
    }

    housekeeping_task_user();
}

// Host went to sleep: turn the layer LEDs off (housekeeping doesn't run
// while suspended). The lock LEDs are handled by QMK.
void suspend_power_down_kb(void) {
    set_all_levels(0);
    space_held   = false;
    space_fading = false;
    suspend_power_down_user();
}
