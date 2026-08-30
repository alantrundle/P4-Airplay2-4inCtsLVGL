#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rtsp_server_start(void);
void rtsp_server_stop(void);

void airplay_set_volume(float volume_db);
int32_t airplay_get_volume_q15(void);

void rtsp_server_request_resume(void);

#ifdef __cplusplus
}
#endif