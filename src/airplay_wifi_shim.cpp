#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

extern "C" {

const char *wifi_get_mac_str(void)
{
    static char mac_str[18];

    uint8_t mac[6] = {0};

    esp_wifi_get_mac(WIFI_IF_STA, mac);

    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);

    return mac_str;
}

}