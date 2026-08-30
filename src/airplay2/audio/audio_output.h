#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_output_init(void);
void audio_output_start(void);
void audio_output_stop(void);

esp_err_t audio_output_write(const void *data, size_t bytes, TickType_t wait);

void audio_output_set_sample_rate(uint32_t rate);
void audio_output_flush(void);
void audio_output_set_source_rate(int rate);

bool audio_output_es8311_enabled(void);
bool audio_output_pcm5102_enabled(void);
bool audio_output_set_es8311_volume_percent(uint8_t percent);
uint8_t audio_output_get_es8311_volume_percent(void);

#ifdef __cplusplus
}
#endif