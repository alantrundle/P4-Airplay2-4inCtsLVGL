#include "artwork_store.h"
#include <string.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_lock;
static uint8_t *s_data;
static size_t s_len;
static char s_type[16];
static uint32_t s_version;

static void ensure_lock(void) { if (!s_lock) s_lock = xSemaphoreCreateMutex(); }

bool artwork_store_set(const uint8_t *data, size_t len, const char *content_type) {
    if (!data || !len || !content_type) return false;
    uint8_t *p = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(len, MALLOC_CAP_8BIT);
    if (!p) return false;
    memcpy(p, data, len);
    ensure_lock();
    if (!s_lock || xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) { free(p); return false; }
    uint8_t *old = s_data;
    s_data = p; s_len = len;
    strlcpy(s_type, content_type, sizeof(s_type));
    ++s_version;
    xSemaphoreGive(s_lock);
    free(old);
    return true;
}

void artwork_store_clear(void) {
    ensure_lock();
    if (!s_lock || xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) return;
    uint8_t *old = s_data;
    s_data = NULL; s_len = 0; s_type[0] = 0; ++s_version;
    xSemaphoreGive(s_lock);
    free(old);
}

bool artwork_store_copy(uint8_t **data, size_t *len, char *content_type, size_t type_size, uint32_t *version) {
    if (!data || !len) return false;
    *data = NULL; *len = 0;
    ensure_lock();
    if (!s_lock || xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    if (!s_data || !s_len) { if (version) *version=s_version; xSemaphoreGive(s_lock); return false; }
    uint8_t *p = heap_caps_malloc(s_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(s_len, MALLOC_CAP_8BIT);
    if (!p) { xSemaphoreGive(s_lock); return false; }
    memcpy(p, s_data, s_len); *data=p; *len=s_len;
    if (content_type && type_size) strlcpy(content_type, s_type, type_size);
    if (version) *version=s_version;
    xSemaphoreGive(s_lock);
    return true;
}
uint32_t artwork_store_version(void) { ensure_lock(); if (!s_lock) return 0; uint32_t v; xSemaphoreTake(s_lock, portMAX_DELAY); v=s_version; xSemaphoreGive(s_lock); return v; }
