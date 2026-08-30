#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    char title[64];
    char artist[64];
    char album[64];

    uint32_t position_secs;
    uint32_t duration_secs;

    bool playing;
    bool connected;
    bool has_artwork;

} ui_now_playing_state_t;


void ui_now_playing_create(lv_obj_t *parent);

void ui_now_playing_update(
    const ui_now_playing_state_t *state
);

void ui_now_playing_set_status(
    const char *status
);

void ui_now_playing_set_wifi(
    const char *wifi_text
);


// ES8311 volume UI. Safe to call even when ES8311 is not enabled.
void ui_now_playing_set_volume_enabled(bool enabled);
void ui_now_playing_set_volume_percent(uint8_t percent);
