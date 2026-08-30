#pragma once

#include <lvgl.h>
#include <stdint.h>

bool ui_manager_begin(lv_obj_t *parent);

void ui_manager_process(uint32_t now_ms);

void ui_manager_set_status(const char *status);

void ui_manager_set_wifi(const char *wifi_text);