#include "esp_log.h"
#include "esp_mac.h"
#include "mdns.h"

#include <stdio.h>
#include <string.h>

#include "hap.h"
#include "mdns_airplay.h"
#include "rtsp_handlers.h"
#include "settings.h"

static const char *TAG = "mdns_airplay";

#ifdef CONFIG_AIRPLAY_FORCE_V1
#define AIRPLAY_PROTOCOL_VERSION "1"
#else
#define AIRPLAY_PROTOCOL_VERSION "2"
#endif

#define AIRPLAY_SOURCE_VERSION "377.40.00"
#define AIRPLAY_FLAGS          "0x4"
#define AIRPLAY_MODEL          "AudioAccessory5,1"

void mdns_airplay_init(void)
{
    char mac_plain[13];
    char device_id[18];
    char features_str[32];
    char service_name[96];
    char pk_str[65];
    char device_name[65];

    settings_get_device_name(device_name, sizeof(device_name));

    if (device_name[0] == '\0') {
        snprintf(device_name, sizeof(device_name), "ESP32 AirPlay");
    }

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    snprintf(device_id, sizeof(device_id),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);

    snprintf(mac_plain, sizeof(mac_plain),
             "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);

    const uint8_t *pk = hap_get_public_key();

    if (pk) {
        for (int i = 0; i < 32; i++) {
            snprintf(pk_str + ((size_t)i * 2), 3, "%02x", pk[i]);
        }
        pk_str[64] = '\0';
    } else {
        memset(pk_str, '0', 64);
        pk_str[64] = '\0';
    }

    snprintf(features_str, sizeof(features_str),
             "0x%X,0x%X",
             AIRPLAY_FEATURES_LO,
             AIRPLAY_FEATURES_HI);

    snprintf(service_name, sizeof(service_name),
             "%s@%s",
             mac_plain,
             device_name);

    ESP_LOGI(TAG, "AirPlay device name: %s", device_name);
    ESP_LOGI(TAG, "AirPlay hostname: at1053-airplay.local");
    ESP_LOGI(TAG, "AirPlay deviceid: %s", device_id);
    ESP_LOGI(TAG, "RAOP service name: %s", service_name);

    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_ERROR_CHECK(mdns_hostname_set("at1053-airplay"));
    ESP_ERROR_CHECK(mdns_instance_name_set(device_name));

#ifndef CONFIG_AIRPLAY_FORCE_V1
    mdns_txt_item_t airplay_txt[] = {
        {"deviceid", device_id},
        {"features", features_str},
        {"flags", AIRPLAY_FLAGS},
        {"model", AIRPLAY_MODEL},
        {"pk", pk_str},
        {"pi", "00000000-0000-0000-0000-000000000000"},
        {"srcvers", AIRPLAY_SOURCE_VERSION},
        {"vv", AIRPLAY_PROTOCOL_VERSION},
        {"acl", "0"},
    };

    err = mdns_service_add(device_name,
                           "_airplay",
                           "_tcp",
                           7000,
                           airplay_txt,
                           sizeof(airplay_txt) / sizeof(airplay_txt[0]));

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to add _airplay._tcp: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "_airplay._tcp added as: %s", device_name);
    }
#endif

#ifdef CONFIG_AIRPLAY_FORCE_V1
    mdns_txt_item_t raop_txt[] = {
        {"am", AIRPLAY_MODEL},
        {"tp", "UDP"},
        {"sm", "false"},
        {"sv", "false"},
        {"ek", "1"},
        {"et", "0,1"},
        {"md", "0,1,2"},
        {"cn", "0,1"},
        {"ch", "2"},
        {"ss", "16"},
        {"sr", "44100"},
        {"vn", "3"},
        {"txtvers", "1"},
    };
#else
    mdns_txt_item_t raop_txt[] = {
        {"am", AIRPLAY_MODEL},

        {"txtvers", "1"},
        {"ch", "2"},
        {"ss", "16"},
        {"sr", "44100"},

        {"cn", "0,1,2,3"},
        {"da", "true"},
        {"et", "0,3,5"},
        {"md", "0,1,2"},
        {"tp", "UDP"},

        {"ft", features_str},
        {"pk", pk_str},
        {"sf", AIRPLAY_FLAGS},
        {"vn", "65537"},
        {"vs", AIRPLAY_SOURCE_VERSION},
        {"vv", AIRPLAY_PROTOCOL_VERSION},
    };
#endif

    err = mdns_service_add(service_name,
                           "_raop",
                           "_tcp",
                           7000,
                           raop_txt,
                           sizeof(raop_txt) / sizeof(raop_txt[0]));

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to add _raop._tcp: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "_raop._tcp added as: %s", service_name);
    }
}