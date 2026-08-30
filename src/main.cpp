#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "airplayService.h"
#include "audio_output.h"
#include "display_ui.h"


// ================================================================
// WiFi
// ================================================================

#define WIFI_SSID "MyNetwork"
#define WIFI_PASS "cdf45424e4"


// ================================================================
// WiFi connection
// ================================================================

static bool connect_wifi()
{
    Serial.println();
    Serial.println("Starting WiFi");

    WiFi.mode(WIFI_STA);

    display_ui_set_wifi_connecting();

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASS
    );

    Serial.print("Connecting WiFi");

    const uint32_t start = millis();

    uint32_t last_dot = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
        // Keep LVGL alive while WiFi is connecting.
        display_ui_loop();

        const uint32_t now = millis();

        if ((now - last_dot) >= 500)
        {
            Serial.print(".");
            last_dot = now;
        }

        if ((now - start) >= 30000)
        {
            Serial.println();
            Serial.println("WiFi connection timed out");

            display_ui_set_wifi_failed();

            return false;
        }

        delay(10);
    }


    Serial.println();
    Serial.println("WiFi connected");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());


    display_ui_set_wifi_connected(
        WiFi.localIP()
    );


    return true;
}


// ================================================================
// Memory statistics
// ================================================================

static void printMemoryStats()
{
    Serial.println();
    Serial.println("========== MEMORY ==========");

    Serial.printf(
        "Internal free : %u bytes\n",
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
    );

    Serial.printf(
        "DMA free      : %u bytes\n",
        heap_caps_get_free_size(MALLOC_CAP_DMA)
    );

    Serial.printf(
        "PSRAM free    : %u bytes\n",
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
    );

    Serial.printf(
        "Total heap    : %u bytes\n",
        ESP.getFreeHeap()
    );

    Serial.printf(
    "Largest DMA block: %u\n",
    heap_caps_get_largest_free_block(MALLOC_CAP_DMA)
    );

    Serial.printf(
    "Largest internal block: %u\n",
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)
    );

    Serial.println("============================");
    Serial.println();
}


// ================================================================
// Setup
// ================================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);


    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32-P4-WIFI6 AirPlay + LVGL");
    Serial.println("========================================");
    Serial.println();


    // ============================================================
    // Display / LVGL
    // ============================================================

    Serial.println("Initialising display...");

    if (!display_ui_begin())
    {
        Serial.println();
        Serial.println("*** DISPLAY INITIALISATION FAILED ***");
        Serial.println("Continuing without display...");
        Serial.println();
    }
    else
    {
        Serial.println("Display initialisation complete");
    }


    printMemoryStats();


    // ============================================================
    // WiFi
    // ============================================================

    if (!connect_wifi())
    {
        Serial.println();
        Serial.println("WiFi failed");
        Serial.println("AirPlay will not be started");

        display_ui_set_airplay_failed();

        return;
    }


    // Give LVGL a chance to update the WiFi information.
    for (int i = 0; i < 10; ++i)
    {
        display_ui_loop();
        delay(10);
    }


    // ============================================================
    // AirPlay
    // ============================================================

    Serial.println();
    Serial.println("Starting AirPlay service...");

    display_ui_set_airplay_starting();

    display_ui_loop();


    bool ap_ok = airplay_begin();


    if (ap_ok)
    {
        Serial.println("AirPlay service started");

        display_ui_set_airplay_started();
    }
    else
    {
        Serial.println("AirPlay service failed");

        display_ui_set_airplay_failed();
    }


    // Force a few UI updates after AirPlay initialisation.
    for (int i = 0; i < 10; ++i)
    {
        display_ui_loop();
        delay(10);
    }


    printMemoryStats();


    Serial.println();
    Serial.println("Setup complete");
}


// ================================================================
// Main loop
// ================================================================

void loop()
{
    // LVGL must be serviced continuously.
    display_ui_loop();

    delay(1);
}