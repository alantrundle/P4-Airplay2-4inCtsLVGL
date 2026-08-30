#pragma once
#include <stdint.h>

static inline void led_set_state(int state) {}
static inline void led_blink(int count) {}

static inline void led_audio_feed(const int16_t *samples, int sample_count) {}