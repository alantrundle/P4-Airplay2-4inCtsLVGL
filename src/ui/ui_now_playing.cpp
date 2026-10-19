#include "ui/ui_now_playing.h"
#include "audio_output.h"
#include "artwork_store.h"
#include <TJpg_Decoder.h>
#include "esp_heap_caps.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>


// ================================================================
// LVGL objects
// ================================================================

static lv_obj_t *s_title = nullptr;

static lv_obj_t *s_artist = nullptr;

static lv_obj_t *s_album = nullptr;

static lv_obj_t *s_progress = nullptr;

static lv_obj_t *s_elapsed = nullptr;

static lv_obj_t *s_duration = nullptr;

static lv_obj_t *s_play_state = nullptr;

static lv_obj_t *s_status = nullptr;

static lv_obj_t *s_wifi = nullptr;

static lv_obj_t *s_art_placeholder = nullptr;

static lv_obj_t *s_art_text = nullptr;
static lv_obj_t *s_art_image = nullptr;
static uint16_t *s_art_pixels = nullptr;
static uint16_t *s_art_decode_pixels = nullptr;
static uint16_t s_art_decode_w = 0;
static uint16_t s_art_decode_h = 0;
static lv_image_dsc_t s_art_dsc = {};
static uint32_t s_art_version = 0;
#define ART_W 160
#define ART_H 160

static lv_obj_t *s_volume_caption = nullptr;
static lv_obj_t *s_volume_value = nullptr;
static lv_obj_t *s_volume_slider = nullptr;
static bool s_volume_internal_update = false;


// ================================================================
// Helpers
// ================================================================

static void format_time(
    uint32_t seconds,
    char *out,
    size_t out_size)
{
    const uint32_t mins =
        seconds / 60U;

    const uint32_t secs =
        seconds % 60U;


    snprintf(
        out,
        out_size,
        "%lu:%02lu",
        static_cast<unsigned long>(mins),
        static_cast<unsigned long>(secs)
    );
}


static void set_label_text_or(
    lv_obj_t *label,
    const char *text,
    const char *fallback)
{
    if (!label)
        return;


    if (text && text[0])
    {
        lv_label_set_text(
            label,
            text
        );
    }
    else
    {
        lv_label_set_text(
            label,
            fallback
        );
    }
}



static bool art_jpeg_cb(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    if (!s_art_decode_pixels || !bitmap)
        return false;

    if (x < 0 || y < 0)
        return true;

    if ((uint16_t)x >= s_art_decode_w || (uint16_t)y >= s_art_decode_h)
        return true;

    uint16_t cw = w;
    uint16_t ch = h;

    if ((uint32_t)x + cw > s_art_decode_w)
        cw = s_art_decode_w - (uint16_t)x;

    if ((uint32_t)y + ch > s_art_decode_h)
        ch = s_art_decode_h - (uint16_t)y;

    for (uint16_t row = 0; row < ch; ++row) {
        memcpy(
            &s_art_decode_pixels[((uint16_t)y + row) * s_art_decode_w + (uint16_t)x],
            &bitmap[row * w],
            cw * sizeof(uint16_t)
        );
    }

    return true;
}


static bool load_artwork_if_changed(void)
{
    uint32_t v = artwork_store_version();

    if (v == s_art_version)
        return s_art_image && !lv_obj_has_flag(s_art_image, LV_OBJ_FLAG_HIDDEN);

    s_art_version = v;

    uint8_t *data = nullptr;
    size_t len = 0;
    char type[16] = {0};
    uint32_t copy_v = 0;

    if (!artwork_store_copy(&data, &len, type, sizeof(type), &copy_v))
        return false;

    bool ok = false;

    if (strstr(type, "image/jpeg")) {
        uint16_t jw = 0;
        uint16_t jh = 0;

        if (TJpgDec.getJpgSize(&jw, &jh, data, len) == JDR_OK &&
            jw > 0 && jh > 0) {

            /*
             * TJpg_Decoder only supports power-of-two decode scaling.
             * Decode to the nearest useful size, then resample the
             * decoded RGB565 pixels to EXACTLY 160x160.
             */
            uint8_t scale = 1;

            while (scale < 8 &&
                   (jw / scale > ART_W || jh / scale > ART_H)) {
                scale <<= 1;
            }

            s_art_decode_w = jw / scale;
            s_art_decode_h = jh / scale;

            if (s_art_decode_w == 0) s_art_decode_w = 1;
            if (s_art_decode_h == 0) s_art_decode_h = 1;

            const size_t decode_count =
                (size_t)s_art_decode_w * (size_t)s_art_decode_h;

            s_art_decode_pixels = static_cast<uint16_t *>(
                heap_caps_malloc(
                    decode_count * sizeof(uint16_t),
                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
                )
            );

            if (!s_art_pixels) {
                s_art_pixels = static_cast<uint16_t *>(
                    heap_caps_malloc(
                        ART_W * ART_H * sizeof(uint16_t),
                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
                    )
                );
            }

            if (s_art_decode_pixels && s_art_pixels) {
                memset(
                    s_art_decode_pixels,
                    0,
                    decode_count * sizeof(uint16_t)
                );

                TJpgDec.setCallback(art_jpeg_cb);
                TJpgDec.setJpgScale(scale);
                TJpgDec.setSwapBytes(false);

                if (TJpgDec.drawJpg(0, 0, data, len) == JDR_OK) {

                    // Nearest-neighbour scale to exactly 160x160.
                    for (uint32_t dy = 0; dy < ART_H; ++dy) {
                        uint32_t sy =
                            (dy * (uint32_t)s_art_decode_h) / ART_H;

                        if (sy >= s_art_decode_h)
                            sy = s_art_decode_h - 1;

                        for (uint32_t dx = 0; dx < ART_W; ++dx) {
                            uint32_t sx =
                                (dx * (uint32_t)s_art_decode_w) / ART_W;

                            if (sx >= s_art_decode_w)
                                sx = s_art_decode_w - 1;

                            s_art_pixels[dy * ART_W + dx] =
                                s_art_decode_pixels[
                                    sy * s_art_decode_w + sx
                                ];
                        }
                    }

                    memset(&s_art_dsc, 0, sizeof(s_art_dsc));
                    s_art_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
                    s_art_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
                    s_art_dsc.header.w = ART_W;
                    s_art_dsc.header.h = ART_H;
                    s_art_dsc.header.stride = ART_W * sizeof(uint16_t);
                    s_art_dsc.data_size = ART_W * ART_H * sizeof(uint16_t);
                    s_art_dsc.data =
                        reinterpret_cast<const uint8_t *>(s_art_pixels);

                    lv_image_set_src(s_art_image, &s_art_dsc);
                    ok = true;
                }
            }

            if (s_art_decode_pixels) {
                heap_caps_free(s_art_decode_pixels);
                s_art_decode_pixels = nullptr;
            }

            s_art_decode_w = 0;
            s_art_decode_h = 0;
        }
    }
    else if (strstr(type, "image/png")) {
        Serial.println("[UI-ART] PNG received; JPEG decoder cannot decode PNG yet");
    }

    free(data);
    return ok;
}

// ================================================================
// ES8311 live volume control
// ================================================================

static void volume_slider_event_cb(lv_event_t *e)
{
    if (s_volume_internal_update || !audio_output_es8311_enabled())
        return;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
        return;

    lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int32_t value = lv_slider_get_value(slider);
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    if (audio_output_set_es8311_volume_percent(static_cast<uint8_t>(value)))
    {
        char text[8];
        snprintf(text, sizeof(text), "%ld%%", static_cast<long>(value));
        if (s_volume_value)
            lv_label_set_text(s_volume_value, text);
    }
}

// ================================================================
// Create Now Playing screen
// ================================================================

void ui_now_playing_create(
    lv_obj_t *parent)
{
    if (!parent)
        return;


    // ============================================================
    // Background
    // ============================================================

    lv_obj_set_style_bg_color(
        parent,
        lv_color_hex(0x0B0F14),
        0
    );


    lv_obj_set_style_bg_opa(
        parent,
        LV_OPA_COVER,
        0
    );


    lv_obj_set_style_pad_all(
        parent,
        0,
        0
    );


    // ============================================================
    // Header
    // ============================================================

    lv_obj_t *header =
        lv_label_create(parent);


    lv_label_set_text(
        header,
        "AIRPLAY"
    );


    lv_obj_set_style_text_font(
        header,
        &lv_font_montserrat_16,
        0
    );


    lv_obj_set_style_text_color(
        header,
        lv_color_hex(0xE8EDF2),
        0
    );


    lv_obj_set_pos(
        header,
        20,
        12
    );


    // ============================================================
    // AirPlay status
    // ============================================================

    s_status =
        lv_label_create(parent);


    lv_label_set_text(
        s_status,
        "STARTING"
    );


    lv_obj_set_style_text_font(
        s_status,
        &lv_font_montserrat_12,
        0
    );


    lv_obj_set_style_text_color(
        s_status,
        lv_color_hex(0x8C98A4),
        0
    );


    lv_obj_align(
        s_status,
        LV_ALIGN_TOP_RIGHT,
        -20,
        14
    );


    // ============================================================
    // Album art placeholder
    // ============================================================

    s_art_placeholder =
        lv_obj_create(parent);


    lv_obj_set_size(
        s_art_placeholder,
        160,
        160
    );


    lv_obj_set_pos(
        s_art_placeholder,
        20,
        48
    );


    lv_obj_set_style_radius(
        s_art_placeholder,
        12,
        0
    );


    lv_obj_set_style_bg_color(
        s_art_placeholder,
        lv_color_hex(0x202832),
        0
    );


    lv_obj_set_style_bg_opa(
        s_art_placeholder,
        LV_OPA_COVER,
        0
    );


    lv_obj_set_style_border_color(
        s_art_placeholder,
        lv_color_hex(0x3A4652),
        0
    );


    lv_obj_set_style_border_width(
        s_art_placeholder,
        1,
        0
    );


    lv_obj_clear_flag(
        s_art_placeholder,
        LV_OBJ_FLAG_SCROLLABLE
    );


    s_art_text =
        lv_label_create(
            s_art_placeholder
        );


    lv_label_set_text(
        s_art_text,
        "ALBUM\nART"
    );


    lv_obj_set_style_text_font(
        s_art_text,
        &lv_font_montserrat_20,
        0
    );


    lv_obj_set_style_text_color(
        s_art_text,
        lv_color_hex(0x8794A1),
        0
    );


    lv_obj_set_style_text_align(
        s_art_text,
        LV_TEXT_ALIGN_CENTER,
        0
    );


    lv_obj_center(
        s_art_text
    );

    lv_label_set_text(s_art_text, "NO ARTWORK");

    s_art_image = lv_image_create(s_art_placeholder);
    lv_obj_set_size(s_art_image, ART_W, ART_H);
    lv_obj_center(s_art_image);
    lv_obj_add_flag(s_art_image, LV_OBJ_FLAG_HIDDEN);


    // ============================================================
    // Track title
    // ============================================================

    s_title =
        lv_label_create(parent);


    lv_obj_set_size(
        s_title,
        270,
        30
    );


    lv_label_set_long_mode(
        s_title,
        LV_LABEL_LONG_MODE_DOTS
    );


    lv_label_set_text(
        s_title,
        "Waiting for music..."
    );


    lv_obj_set_style_text_font(
        s_title,
        &lv_font_montserrat_24,
        0
    );


    lv_obj_set_style_text_color(
        s_title,
        lv_color_white(),
        0
    );


    lv_obj_set_pos(
        s_title,
        198,
        58
    );


    // ============================================================
    // Artist
    // ============================================================

    s_artist =
        lv_label_create(parent);


    lv_obj_set_size(
        s_artist,
        270,
        24
    );


    lv_label_set_long_mode(
        s_artist,
        LV_LABEL_LONG_MODE_DOTS
    );


    lv_label_set_text(
        s_artist,
        "Artist"
    );


    lv_obj_set_style_text_font(
        s_artist,
        &lv_font_montserrat_18,
        0
    );


    lv_obj_set_style_text_color(
        s_artist,
        lv_color_hex(0xC1CAD3),
        0
    );


    lv_obj_set_pos(
        s_artist,
        198,
        103
    );


    // ============================================================
    // Album
    // ============================================================

    s_album =
        lv_label_create(parent);


    lv_obj_set_size(
        s_album,
        270,
        22
    );


    lv_label_set_long_mode(
        s_album,
        LV_LABEL_LONG_MODE_DOTS
    );


    lv_label_set_text(
        s_album,
        "Album"
    );


    lv_obj_set_style_text_font(
        s_album,
        &lv_font_montserrat_16,
        0
    );


    lv_obj_set_style_text_color(
        s_album,
        lv_color_hex(0x8995A1),
        0
    );


    lv_obj_set_pos(
        s_album,
        198,
        139
    );


    // ============================================================
    // Playback state
    // ============================================================

    s_play_state =
        lv_label_create(parent);


    lv_label_set_text(
        s_play_state,
        "IDLE"
    );


    lv_obj_set_style_text_font(
        s_play_state,
        &lv_font_montserrat_12,
        0
    );


    lv_obj_set_style_text_color(
        s_play_state,
        lv_color_hex(0x8C98A4),
        0
    );


    lv_obj_set_pos(
        s_play_state,
        198,
        181
    );


    // ============================================================
    // Progress bar
    // ============================================================

    s_progress =
        lv_bar_create(parent);


    lv_obj_set_size(
        s_progress,
        440,
        10
    );


    lv_obj_set_pos(
        s_progress,
        20,
        234
    );


    lv_bar_set_range(
        s_progress,
        0,
        1000
    );


    lv_bar_set_value(
        s_progress,
        0,
        LV_ANIM_OFF
    );


    lv_obj_set_style_radius(
        s_progress,
        5,
        LV_PART_MAIN
    );


    lv_obj_set_style_radius(
        s_progress,
        5,
        LV_PART_INDICATOR
    );


    lv_obj_set_style_bg_color(
        s_progress,
        lv_color_hex(0x25303A),
        LV_PART_MAIN
    );


    lv_obj_set_style_bg_opa(
        s_progress,
        LV_OPA_COVER,
        LV_PART_MAIN
    );


    lv_obj_set_style_bg_color(
        s_progress,
        lv_color_hex(0xD8E0E7),
        LV_PART_INDICATOR
    );


    lv_obj_set_style_bg_opa(
        s_progress,
        LV_OPA_COVER,
        LV_PART_INDICATOR
    );


    // ============================================================
    // Elapsed time
    // ============================================================

    s_elapsed =
        lv_label_create(parent);


    lv_label_set_text(
        s_elapsed,
        "0:00"
    );


    lv_obj_set_style_text_font(
        s_elapsed,
        &lv_font_montserrat_14,
        0
    );


    lv_obj_set_style_text_color(
        s_elapsed,
        lv_color_hex(0xAEB8C2),
        0
    );


    lv_obj_set_pos(
        s_elapsed,
        20,
        254
    );


    // ============================================================
    // Duration
    // ============================================================

    s_duration =
        lv_label_create(parent);


    lv_label_set_text(
        s_duration,
        "0:00"
    );


    lv_obj_set_style_text_font(
        s_duration,
        &lv_font_montserrat_14,
        0
    );


    lv_obj_set_style_text_color(
        s_duration,
        lv_color_hex(0xAEB8C2),
        0
    );


    lv_obj_align(
        s_duration,
        LV_ALIGN_TOP_RIGHT,
        -20,
        254
    );



    // ============================================================
    // ES8311 volume - shown only when that DAC is enabled
    // ============================================================

    if (audio_output_es8311_enabled())
    {
        s_volume_caption = lv_label_create(parent);
        lv_label_set_text(s_volume_caption, "VOLUME");
        lv_obj_set_style_text_font(s_volume_caption, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(s_volume_caption, lv_color_hex(0x8C98A4), 0);
        lv_obj_set_pos(s_volume_caption, 20, 282);

        s_volume_value = lv_label_create(parent);
        char volume_text[8];
        snprintf(volume_text, sizeof(volume_text), "%u%%",
                 static_cast<unsigned>(audio_output_get_es8311_volume_percent()));
        lv_label_set_text(s_volume_value, volume_text);
        lv_obj_set_style_text_font(s_volume_value, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(s_volume_value, lv_color_hex(0xE8EDF2), 0);
        lv_obj_align(s_volume_value, LV_ALIGN_TOP_RIGHT, -20, 282);

        s_volume_slider = lv_slider_create(parent);
        lv_obj_set_size(s_volume_slider, 300, 8);
        lv_obj_set_pos(s_volume_slider, 90, 286);
        lv_slider_set_range(s_volume_slider, 0, 100);
        lv_slider_set_value(s_volume_slider,0, LV_ANIM_OFF);
        lv_obj_set_style_radius(s_volume_slider, 4, LV_PART_MAIN);
        lv_obj_set_style_radius(s_volume_slider, 4, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_volume_slider, lv_color_hex(0x25303A), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_volume_slider, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_volume_slider, lv_color_hex(0xD8E0E7), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(s_volume_slider, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_volume_slider, lv_color_hex(0xE8EDF2), LV_PART_KNOB);
        lv_obj_set_style_pad_all(s_volume_slider, 8, LV_PART_KNOB);
        lv_obj_add_event_cb(s_volume_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);              
        lv_obj_set_ext_click_area(s_volume_slider, 18);
        lv_obj_add_flag(s_volume_slider, LV_OBJ_FLAG_PRESS_LOCK);
    }

    // ============================================================
    // WiFi status
    // ============================================================

    s_wifi =
        lv_label_create(parent);


    lv_label_set_text(
        s_wifi,
        "WiFi: --"
    );


    lv_obj_set_width(
        s_wifi,
        300
    );


    lv_label_set_long_mode(
        s_wifi,
        LV_LABEL_LONG_MODE_DOTS
    );


    lv_obj_set_style_text_font(
        s_wifi,
        &lv_font_montserrat_12,
        0
    );


    lv_obj_set_style_text_color(
        s_wifi,
        lv_color_hex(0x66727E),
        0
    );


    lv_obj_set_pos(
        s_wifi,
        20,
        304
    );
}


// ================================================================
// Update Now Playing screen
// ================================================================

void ui_now_playing_update(
    const ui_now_playing_state_t *state)
{
    if (!state)
        return;


    // ============================================================
    // Metadata
    // ============================================================

    set_label_text_or(
        s_title,
        state->title,
        "Waiting for music..."
    );


    set_label_text_or(
        s_artist,
        state->artist,
        "Artist"
    );


    set_label_text_or(
        s_album,
        state->album,
        "Album"
    );


    // ============================================================
    // Playback state
    // ============================================================

    if (s_play_state)
    {
        if (!state->connected)
        {
            lv_label_set_text(
                s_play_state,
                "IDLE"
            );
        }
        else if (state->playing)
        {
            lv_label_set_text(
                s_play_state,
                "PLAYING"
            );
        }
        else
        {
            lv_label_set_text(
                s_play_state,
                "PAUSED"
            );
        }
    }


    // ============================================================
    // Artwork placeholder
    // ============================================================

    if (s_art_text && s_art_image)
    {
        bool shown = state->has_artwork && load_artwork_if_changed();
        if (shown) {
            lv_obj_add_flag(s_art_text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_art_image, LV_OBJ_FLAG_HIDDEN);

            // Real artwork fills the existing 160x160 area.
            // Hide the placeholder border so rounded cover corners
            // do not show a square frame around the image.
            lv_obj_set_style_border_width(s_art_placeholder, 0, 0);
        } else {
            lv_label_set_text(s_art_text, "NO ARTWORK");
            lv_obj_clear_flag(s_art_text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_art_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_border_width(s_art_placeholder, 1, 0);
        }
    }


    // ============================================================
    // Position and progress
    // ============================================================

    uint32_t position =
        state->position_secs;


    const uint32_t duration =
        state->duration_secs;


    if (
        duration > 0 &&
        position > duration
    )
    {
        position = duration;
    }


    int32_t progress = 0;


    if (duration > 0)
    {
        progress =
            static_cast<int32_t>(
                (
                    static_cast<uint64_t>(position) *
                    1000ULL
                ) /
                duration
            );


        if (progress > 1000)
        {
            progress = 1000;
        }
    }


    if (s_progress)
    {
        lv_bar_set_value(
            s_progress,
            progress,
            LV_ANIM_OFF
        );
    }


    // ============================================================
    // Time labels
    // ============================================================

    char elapsed_text[16];

    char duration_text[16];


    format_time(
        position,
        elapsed_text,
        sizeof(elapsed_text)
    );


    format_time(
        duration,
        duration_text,
        sizeof(duration_text)
    );


    if (s_elapsed)
    {
        lv_label_set_text(
            s_elapsed,
            elapsed_text
        );
    }


    if (s_duration)
    {
        lv_label_set_text(
            s_duration,
            duration_text
        );
    }

    // Keep the UI in sync with volume changes made elsewhere (e.g. serial).
    if (s_volume_slider && audio_output_es8311_enabled())
    {
        const uint8_t volume = audio_output_get_es8311_volume_percent();
        if (lv_slider_get_value(s_volume_slider) != volume)
        {
            s_volume_internal_update = true;
            lv_slider_set_value(s_volume_slider, volume, LV_ANIM_OFF);
            s_volume_internal_update = false;
        }

        if (s_volume_value)
        {
            char volume_text[8];
            snprintf(volume_text, sizeof(volume_text), "%u%%",
                     static_cast<unsigned>(volume));
            lv_label_set_text(s_volume_value, volume_text);
        }
    }

}



// ================================================================
// External ES8311 volume UI control
// ================================================================

void ui_now_playing_set_volume_enabled(bool enabled)
{
    // The controls are only created when ES8311 support is present.
    // This function may still be called safely by ui_manager.
    lv_obj_t *objects[] = {
        s_volume_caption,
        s_volume_value,
        s_volume_slider
    };

    for (lv_obj_t *obj : objects)
    {
        if (!obj)
            continue;

        if (enabled && audio_output_es8311_enabled())
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_now_playing_set_volume_percent(uint8_t percent)
{
    if (!audio_output_es8311_enabled())
        return;

    if (percent > 100)
        percent = 100;

    if (s_volume_slider &&
        lv_slider_get_value(s_volume_slider) != static_cast<int32_t>(percent))
    {
        s_volume_internal_update = true;
        lv_slider_set_value(s_volume_slider, percent, LV_ANIM_OFF);
        s_volume_internal_update = false;
    }

    if (s_volume_value)
    {
        char text[8];
        snprintf(text, sizeof(text), "%u%%",
                 static_cast<unsigned>(percent));
        lv_label_set_text(s_volume_value, text);
    }
}


// ================================================================
// AirPlay status
// ================================================================

void ui_now_playing_set_status(
    const char *status)
{
    if (!s_status)
        return;


    lv_label_set_text(
        s_status,
        status ? status : ""
    );
}


// ================================================================
// WiFi status
// ================================================================

void ui_now_playing_set_wifi(
    const char *wifi_text)
{
    if (!s_wifi)
        return;


    lv_label_set_text(
        s_wifi,
        wifi_text ? wifi_text : ""
    );
}
