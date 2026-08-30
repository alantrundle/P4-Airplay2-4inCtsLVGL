#include "airplayService.h"

#include <Arduino.h>
#include <WiFi.h>

extern "C" {
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "settings.h"
#include "playback_control.h"
#include "ptp_clock.h"
#include "hap.h"
#include "audio_receiver.h"
#include "audio_output.h"
#include "audio_stream.h"
#include "rtsp_server.h"
#include "mdns_airplay.h"
}

static const char *TAG = "airplay_service";

#define CHECK_STEP(fn) do {                                          \
    esp_err_t err = (fn);                                            \
    if (err != ESP_OK) {                                             \
        ESP_LOGE(TAG, "%s failed: %s", #fn, esp_err_to_name(err));   \
        Serial.printf("[AIRPLAY] %s failed: %s\n", #fn, esp_err_to_name(err)); \
        return false;                                                \
    }                                                                \
    ESP_LOGI(TAG, "%s OK", #fn);                                     \
    Serial.printf("[AIRPLAY] %s OK\n", #fn);                         \
} while (0)

bool airplay_begin(void)
{
    Serial.println("[AIRPLAY] Starting real AirPlay stack");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[AIRPLAY] WiFi not connected");
        return false;
    }

    Serial.print("[AIRPLAY] IP: ");
    Serial.println(WiFi.localIP());

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("[AIRPLAY] NVS erase required");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        Serial.printf("[AIRPLAY] nvs_flash_init failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    Serial.println("[AIRPLAY] nvs_flash_init OK");

    CHECK_STEP(settings_init());
    CHECK_STEP(playback_control_init());

    // Try it, but do not abort if it fails.
    ret = audio_realtime_preallocate();
    if (ret != ESP_OK) {
        Serial.printf("[AIRPLAY] audio_realtime_preallocate warning: %s\n", esp_err_to_name(ret));
    } else {
        Serial.println("[AIRPLAY] audio_realtime_preallocate OK");
    }

    CHECK_STEP(ptp_clock_init());
    CHECK_STEP(hap_init());
    CHECK_STEP(audio_receiver_init());
    CHECK_STEP(audio_output_init());

    Serial.println("[AIRPLAY] Starting mDNS advertisement");


    Serial.println("[AIRPLAY] Starting audio output");
    audio_output_start();

    CHECK_STEP(rtsp_server_start());

    Serial.println("[AIRPLAY] Starting mDNS advertisement");
    mdns_airplay_init();
    Serial.println("[AIRPLAY] mDNS advertisement started");

    playback_control_set_source(PLAYBACK_SOURCE_AIRPLAY);

    Serial.println("[AIRPLAY] AirPlay ready");
    return true;
}