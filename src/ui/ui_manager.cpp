#include "ui/ui_manager.h"
#include "ui/ui_now_playing.h"
#include "audio/audio_output.h"

#include <Arduino.h>
#include <string.h>

extern "C"
{
#include "rtsp_events.h"
}


// ================================================================
// UI state shared with RTSP event callback
// ================================================================

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;


struct UiModel
{
    char title[64];
    char artist[64];
    char album[64];

    uint32_t position_secs;
    uint32_t duration_secs;

    uint32_t position_anchor_ms;

    bool playing;
    bool connected;
    bool has_artwork;

    bool dirty;
};


static UiModel s_model = {};

static uint32_t s_last_render_ms = 0;

static uint32_t s_last_rendered_position = UINT32_MAX;


// ================================================================
// Helpers
// ================================================================

static bool string_has_value(const char *s)
{
    return s != nullptr && s[0] != '\0';
}


static void copy_string_if_present(
    char *dst,
    size_t dst_size,
    const char *src)
{
    if (!dst || dst_size == 0)
        return;

    if (!string_has_value(src))
        return;

    strlcpy(
        dst,
        src,
        dst_size
    );
}


// ================================================================
// AirPlay / RTSP event callback
//
// IMPORTANT:
// Do NOT manipulate LVGL objects from here.
// This callback may run from the RTSP task.
// ================================================================

static void rtsp_ui_event(
    rtsp_event_t event,
    const rtsp_event_data_t *data,
    void *user_data)
{
    (void)user_data;

    const uint32_t now = millis();


    portENTER_CRITICAL(&s_lock);


    switch (event)
    {
        // --------------------------------------------------------
        // AirPlay client connected
        // --------------------------------------------------------

        case RTSP_EVENT_CLIENT_CONNECTED:
        {
            s_model.connected = true;

            s_model.dirty = true;

            break;
        }


        // --------------------------------------------------------
        // Playback started / resumed
        // --------------------------------------------------------

        case RTSP_EVENT_PLAYING:
        {
            s_model.connected = true;

            s_model.playing = true;

            s_model.position_anchor_ms = now;

            s_model.dirty = true;

            break;
        }


        // --------------------------------------------------------
        // Playback paused
        // --------------------------------------------------------

        case RTSP_EVENT_PAUSED:
        {
            if (s_model.playing)
            {
                const uint32_t elapsed =
                    (now - s_model.position_anchor_ms) / 1000U;

                s_model.position_secs += elapsed;


                if (
                    s_model.duration_secs > 0 &&
                    s_model.position_secs > s_model.duration_secs
                )
                {
                    s_model.position_secs =
                        s_model.duration_secs;
                }
            }


            s_model.playing = false;

            s_model.position_anchor_ms = now;

            s_model.dirty = true;

            break;
        }


        // --------------------------------------------------------
        // AirPlay client disconnected
        // --------------------------------------------------------

        case RTSP_EVENT_DISCONNECTED:
        {
            s_model.connected = false;

            s_model.playing = false;

            s_model.position_anchor_ms = now;

            s_model.dirty = true;

            break;
        }


        // --------------------------------------------------------
        // Track metadata / progress
        // --------------------------------------------------------

        case RTSP_EVENT_METADATA:
        {
            if (!data)
                break;


            const rtsp_metadata_t &m =
                data->metadata;


            // ----------------------------------------------------
            // Detect track change
            //
            // This version intentionally does NOT rely upon
            // m.has_title / has_artist / etc.
            //
            // Your currently compiled rtsp_metadata_t does not
            // contain those members.
            // ----------------------------------------------------

            bool new_track = false;


            if (string_has_value(m.title))
            {
                if (
                    strcmp(
                        s_model.title,
                        m.title
                    ) != 0
                )
                {
                    new_track = true;
                }
            }


            if (new_track)
            {
                s_model.position_secs = 0;

                s_model.duration_secs = 0;

                // Do NOT clear has_artwork here. AirPlay commonly sends the
                // PICT/JPEG SET_PARAMETER before the DMAP title metadata.
                // Clearing it here races with that artwork event and causes
                // a mid-track connection to show NO ARTWORK until a later update.
            }


            // ----------------------------------------------------
            // Metadata strings
            //
            // Ignore empty strings so a progress-only message
            // doesn't erase the current metadata.
            // ----------------------------------------------------

            copy_string_if_present(
                s_model.title,
                sizeof(s_model.title),
                m.title
            );


            copy_string_if_present(
                s_model.artist,
                sizeof(s_model.artist),
                m.artist
            );


            copy_string_if_present(
                s_model.album,
                sizeof(s_model.album),
                m.album
            );


            // ----------------------------------------------------
            // Progress
            //
            // A non-zero duration indicates useful progress info.
            // ----------------------------------------------------

            if (m.duration_secs > 0)
            {
                s_model.duration_secs =
                    m.duration_secs;

                s_model.position_secs =
                    m.position_secs;

                s_model.position_anchor_ms =
                    now;
            }


            // ----------------------------------------------------
            // Artwork indicator
            // ----------------------------------------------------

            if (m.has_artwork)
            {
                s_model.has_artwork = true;
            }


            s_model.connected = true;

            s_model.dirty = true;

            break;
        }
    }


    portEXIT_CRITICAL(&s_lock);
}


// ================================================================
// Initialise UI manager
// ================================================================

bool ui_manager_begin(lv_obj_t *parent)
{
    if (!parent)
    {
        Serial.println(
            "[UI] Invalid parent object"
        );

        return false;
    }


    // Create Now Playing screen
    ui_now_playing_create(parent);

    const bool es8311_enabled = audio_output_es8311_enabled();
    ui_now_playing_set_volume_enabled(es8311_enabled);
    if (es8311_enabled)
        ui_now_playing_set_volume_percent(audio_output_get_es8311_volume_percent());


    // Reset state
    portENTER_CRITICAL(&s_lock);


    memset(
        &s_model,
        0,
        sizeof(s_model)
    );


    s_model.position_anchor_ms =
        millis();

    s_model.dirty = true;


    portEXIT_CRITICAL(&s_lock);


    // Register with existing RTSP event dispatcher
    if (
        rtsp_events_register(
            rtsp_ui_event,
            nullptr
        ) != 0
    )
    {
        Serial.println(
            "[UI] Failed to register RTSP event listener"
        );

        return false;
    }


    Serial.println(
        "[UI] RTSP metadata listener registered"
    );


    return true;
}


// ================================================================
// Process pending UI updates
//
// This runs from the normal display/LVGL task.
// ================================================================

void ui_manager_process(uint32_t now_ms)
{
    // ------------------------------------------------------------
    // Limit updates to 5 Hz
    // ------------------------------------------------------------

    if (
        (now_ms - s_last_render_ms) < 250U
    )
    {
        return;
    }


    s_last_render_ms = now_ms;

    // Deliberately no periodic codec-volume -> LVGL synchronization here.
    // User slider events update the volume control directly.


    // ------------------------------------------------------------
    // Take a safe snapshot
    // ------------------------------------------------------------

    UiModel snapshot;

    bool was_dirty = false;


    portENTER_CRITICAL(&s_lock);


    snapshot = s_model;

    was_dirty = s_model.dirty;

    s_model.dirty = false;


    portEXIT_CRITICAL(&s_lock);


    // ------------------------------------------------------------
    // Convert to screen state
    // ------------------------------------------------------------

    ui_now_playing_state_t state = {};


    strlcpy(
        state.title,
        snapshot.title,
        sizeof(state.title)
    );


    strlcpy(
        state.artist,
        snapshot.artist,
        sizeof(state.artist)
    );


    strlcpy(
        state.album,
        snapshot.album,
        sizeof(state.album)
    );


    state.position_secs =
        snapshot.position_secs;


    state.duration_secs =
        snapshot.duration_secs;


    state.playing =
        snapshot.playing;


    state.connected =
        snapshot.connected;


    state.has_artwork =
        snapshot.has_artwork;


    // ------------------------------------------------------------
    // Locally interpolate playback position
    // ------------------------------------------------------------

    if (snapshot.playing)
    {
        const uint32_t elapsed =
            (now_ms - snapshot.position_anchor_ms) / 1000U;


        state.position_secs +=
            elapsed;


        if (
            state.duration_secs > 0 &&
            state.position_secs > state.duration_secs
        )
        {
            state.position_secs =
                state.duration_secs;
        }
    }


    // ------------------------------------------------------------
    // Avoid unnecessary screen redraws
    // ------------------------------------------------------------

    if (
        !was_dirty &&
        state.position_secs == s_last_rendered_position
    )
    {
        return;
    }


    s_last_rendered_position =
        state.position_secs;


    // ------------------------------------------------------------
    // Update LVGL
    // ------------------------------------------------------------

    ui_now_playing_update(
        &state
    );
}


// ================================================================
// AirPlay status
// ================================================================

void ui_manager_set_status(
    const char *status)
{
    ui_now_playing_set_status(
        status
    );
}


// ================================================================
// WiFi status
// ================================================================

void ui_manager_set_wifi(
    const char *wifi_text)
{
    ui_now_playing_set_wifi(
        wifi_text
    );
}