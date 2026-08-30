#pragma once

#include <Arduino.h>

// Initialise the ST7796 display, LVGL and FT6206/FT6336 touch controller.
// Returns true when the LCD/LVGL path was initialised. Touch failure is non-fatal.
bool display_ui_begin();

// Must be called frequently from Arduino loop().
void display_ui_loop();

void display_ui_set_wifi_connecting();
void display_ui_set_wifi_connected(const IPAddress &ip);
void display_ui_set_wifi_failed();

void display_ui_set_airplay_starting();
void display_ui_set_airplay_started();
void display_ui_set_airplay_failed();
