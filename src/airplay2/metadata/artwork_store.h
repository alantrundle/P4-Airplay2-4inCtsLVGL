#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool artwork_store_set(const uint8_t *data, size_t len, const char *content_type);
void artwork_store_clear(void);
bool artwork_store_copy(uint8_t **data, size_t *len, char *content_type, size_t content_type_size, uint32_t *version);
uint32_t artwork_store_version(void);
#ifdef __cplusplus
}
#endif
