#include <stdlib.h>
#include <string.h>

#include "audio_buffer.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "audio_buf";


/* -------------------------------------------------------------------------- */
/* Helpers for slot pool                                                      */
/* -------------------------------------------------------------------------- */

static inline uint8_t *slot_ptr(audio_buffer_t *b, uint16_t slot)
{
    return b->pool + (size_t)slot * b->slot_size;
}


static inline uint32_t slot_timestamp(audio_buffer_t *b, uint16_t slot)
{
    return ((audio_frame_header_t *)slot_ptr(b, slot))->rtp_timestamp;
}


/*
 * RTP timestamp comparison that handles 32-bit wraparound.
 *
 * < 0 : a before b
 * = 0 : equal
 * > 0 : a after b
 */
static inline int32_t ts_cmp(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b);
}


/*
 * Binary search insertion position.
 */
static int sorted_insert_pos(audio_buffer_t *b, uint32_t timestamp)
{
    int lo = 0;
    int hi = b->count;

    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;

        if (ts_cmp(slot_timestamp(b, b->sorted[mid]), timestamp) < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}


/* -------------------------------------------------------------------------- */
/* Queue one PCM chunk                                                        */
/* -------------------------------------------------------------------------- */

bool audio_buffer_queue_chunk(audio_buffer_t *buffer,
                              audio_stats_t *stats,
                              uint32_t timestamp,
                              const int16_t *pcm_data,
                              size_t samples,
                              int channels)
{
    if (!buffer ||
        !buffer->pool ||
        !pcm_data ||
        samples == 0)
    {
        return false;
    }

    if (channels <= 0) {
        channels = 2;
    }

    if (channels > AUDIO_MAX_CHANNELS) {
        if (stats) {
            stats->packets_dropped++;
        }

        ESP_LOGW(
            TAG,
            "Invalid channel count: %d",
            channels
        );

        return false;
    }

    /*
     * Each ring slot stores at most AAC_FRAMES_PER_PACKET PCM frames.
     *
     * Large decoded AAC frames are split by audio_buffer_queue_decoded().
     */
    if (samples > AAC_FRAMES_PER_PACKET) {
        if (stats) {
            stats->packets_dropped++;
        }

        ESP_LOGW(
            TAG,
            "Too many samples for slot: %u max=%u",
            (unsigned)samples,
            (unsigned)AAC_FRAMES_PER_PACKET
        );

        return false;
    }

    const size_t pcm_bytes =
        samples *
        (size_t)channels *
        sizeof(int16_t);

    const size_t total_bytes =
        sizeof(audio_frame_header_t) +
        pcm_bytes;

    if (total_bytes > buffer->slot_size) {
        if (stats) {
            stats->packets_dropped++;
        }

        ESP_LOGW(
            TAG,
            "Frame too large: total=%u slot=%u",
            (unsigned)total_bytes,
            (unsigned)buffer->slot_size
        );

        return false;
    }


    portENTER_CRITICAL(&buffer->lock);

    /*
     * If the queue is full, drop the NEW incoming frame.
     *
     * Do not discard already queued audio.
     */
    if (buffer->count >= buffer->capacity ||
        buffer->free_top <= 0)
    {
        int count = buffer->count;
        int capacity = buffer->capacity;
        int free_top = buffer->free_top;

        portEXIT_CRITICAL(&buffer->lock);

        if (stats) {
            stats->buffer_overruns++;
            stats->packets_dropped++;
        }

        ESP_LOGW(
            TAG,
            "Audio buffer full: dropping incoming frame "
            "ts=%lu samples=%u count=%d capacity=%d free_top=%d",
            (unsigned long)timestamp,
            (unsigned)samples,
            count,
            capacity,
            free_top
        );

        return false;
    }


    uint16_t slot =
        buffer->free_stack[--buffer->free_top];

    uint8_t *ptr =
        slot_ptr(buffer, slot);

    audio_frame_header_t *hdr =
        (audio_frame_header_t *)ptr;

    hdr->rtp_timestamp =
        timestamp;

    hdr->samples_per_channel =
        (uint16_t)samples;

    hdr->channels =
        (uint8_t)channels;

    hdr->reserved =
        0;


    memcpy(
        ptr + sizeof(audio_frame_header_t),
        pcm_data,
        pcm_bytes
    );


    int insert_pos =
        sorted_insert_pos(buffer, timestamp);

    if (insert_pos < buffer->count) {
        memmove(
            &buffer->sorted[insert_pos + 1],
            &buffer->sorted[insert_pos],
            (buffer->count - insert_pos) *
                sizeof(uint16_t)
        );
    }

    buffer->sorted[insert_pos] =
        slot;

    buffer->count++;

    portEXIT_CRITICAL(&buffer->lock);


    xSemaphoreGive(buffer->data_ready);


    if (stats) {
        stats->packets_received++;
        stats->last_timestamp = timestamp;
    }

    return true;
}


/* -------------------------------------------------------------------------- */
/* Initialise buffer                                                          */
/* -------------------------------------------------------------------------- */

esp_err_t audio_buffer_init(audio_buffer_t *buffer)
{
    if (!buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(buffer, 0, sizeof(*buffer));

    portMUX_TYPE lock =
        portMUX_INITIALIZER_UNLOCKED;

    buffer->lock =
        lock;

    buffer->capacity =
        MAX_RING_BUFFER_FRAMES;

    buffer->slot_size =
        BYTES_PER_FRAME;

    buffer->count =
        0;


    /* ---------------------------------------------------------------------- */
    /* Main PCM ring pool                                                     */
    /* ---------------------------------------------------------------------- */

    /*
     * The main audio ring is large, so it must live in PSRAM.
     */
    buffer->pool =
        (uint8_t *)heap_caps_malloc(
            (size_t)buffer->capacity *
                buffer->slot_size,
            MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
        );

    if (!buffer->pool) {
        ESP_LOGE(
            TAG,
            "Failed to allocate audio pool in PSRAM"
        );

        return ESP_ERR_NO_MEM;
    }


    /* ---------------------------------------------------------------------- */
    /* Index arrays                                                           */
    /* ---------------------------------------------------------------------- */

    /*
     * Keep these small arrays in internal RAM.
     *
     * They are accessed frequently while the buffer lock is held, and together
     * only consume roughly 8 KB with the current 2000-slot configuration.
     */
    buffer->sorted =
        (uint16_t *)malloc(
            (size_t)buffer->capacity *
            sizeof(uint16_t)
        );

    buffer->free_stack =
        (uint16_t *)malloc(
            (size_t)buffer->capacity *
            sizeof(uint16_t)
        );

    if (!buffer->sorted ||
        !buffer->free_stack)
    {
        ESP_LOGE(
            TAG,
            "Failed to allocate audio index arrays"
        );

        audio_buffer_deinit(buffer);

        return ESP_ERR_NO_MEM;
    }


    /* ---------------------------------------------------------------------- */
    /* Initialise free-slot stack                                             */
    /* ---------------------------------------------------------------------- */

    buffer->free_top =
        buffer->capacity;

    for (int i = 0;
         i < buffer->capacity;
         i++)
    {
        buffer->free_stack[i] =
            (uint16_t)i;
    }


    /* ---------------------------------------------------------------------- */
    /* Counting semaphore                                                     */
    /* ---------------------------------------------------------------------- */

    buffer->data_ready =
        xSemaphoreCreateCounting(
            buffer->capacity,
            0
        );

    if (!buffer->data_ready) {
        ESP_LOGE(
            TAG,
            "Failed to create audio buffer semaphore"
        );

        audio_buffer_deinit(buffer);

        return ESP_ERR_NO_MEM;
    }


    /* ---------------------------------------------------------------------- */
    /* Decode workspace                                                       */
    /* ---------------------------------------------------------------------- */

    /*
     * IMPORTANT:
     *
     * This workspace does NOT need DMA-capable internal RAM.
     *
     * AAC stereo:
     *
     *   1024 frames/channel
     *   × 2 channels
     *   × sizeof(int16_t)
     *   = 4096 bytes
     *
     * Our workspace supports:
     *
     *   MAX_SAMPLES_PER_FRAME
     *   × AUDIO_MAX_CHANNELS
     *
     * which is currently 4096 × 2 × 2 = 16384 bytes.
     *
     * Keep it in PSRAM so ESP-Hosted SDIO can retain internal/DMA memory.
     */
    const size_t max_pcm_bytes =
        (size_t)MAX_SAMPLES_PER_FRAME *
        (size_t)AUDIO_MAX_CHANNELS *
        sizeof(int16_t);

    const size_t frame_buffer_bytes =
        sizeof(audio_frame_header_t) +
        max_pcm_bytes;

    buffer->frame_buffer =
        (uint8_t *)heap_caps_malloc(
            frame_buffer_bytes,
            MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
        );

    if (!buffer->frame_buffer) {
        ESP_LOGE(
            TAG,
            "Failed to allocate decode workspace in PSRAM"
        );

        audio_buffer_deinit(buffer);

        return ESP_ERR_NO_MEM;
    }

    buffer->decode_buffer =
        (int16_t *)(
            buffer->frame_buffer +
            sizeof(audio_frame_header_t)
        );

    buffer->decode_capacity_samples =
        MAX_SAMPLES_PER_FRAME;


    ESP_LOGI(
        TAG,
        "Sorted buffer created: %d slots x %zu bytes = %zu bytes",
        buffer->capacity,
        buffer->slot_size,
        (size_t)buffer->capacity *
            buffer->slot_size
    );

    ESP_LOGI(
        TAG,
        "Decode workspace in PSRAM: %u bytes, capacity=%u frames/channel",
        (unsigned)frame_buffer_bytes,
        (unsigned)buffer->decode_capacity_samples
    );


    /*
     * Useful while tracking ESP-Hosted SDIO memory starvation.
     */
    ESP_LOGI(
        TAG,
        "After audio buffer init: "
        "internal=%u dma=%u largest_internal=%u largest_dma=%u psram=%u",
        (unsigned)heap_caps_get_free_size(
            MALLOC_CAP_INTERNAL
        ),
        (unsigned)heap_caps_get_free_size(
            MALLOC_CAP_DMA
        ),
        (unsigned)heap_caps_get_largest_free_block(
            MALLOC_CAP_INTERNAL
        ),
        (unsigned)heap_caps_get_largest_free_block(
            MALLOC_CAP_DMA
        ),
        (unsigned)heap_caps_get_free_size(
            MALLOC_CAP_SPIRAM
        )
    );

    return ESP_OK;
}


/* -------------------------------------------------------------------------- */
/* Deinitialise                                                               */
/* -------------------------------------------------------------------------- */

void audio_buffer_deinit(audio_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }


    if (buffer->data_ready) {
        vSemaphoreDelete(
            buffer->data_ready
        );

        buffer->data_ready =
            NULL;
    }


    if (buffer->pool) {
        heap_caps_free(
            buffer->pool
        );

        buffer->pool =
            NULL;
    }


    /*
     * These were allocated using ordinary malloc().
     */
    if (buffer->sorted) {
        free(
            buffer->sorted
        );

        buffer->sorted =
            NULL;
    }


    if (buffer->free_stack) {
        free(
            buffer->free_stack
        );

        buffer->free_stack =
            NULL;
    }


    /*
     * frame_buffer is now explicitly allocated from PSRAM.
     */
    if (buffer->frame_buffer) {
        heap_caps_free(
            buffer->frame_buffer
        );

        buffer->frame_buffer =
            NULL;

        buffer->decode_buffer =
            NULL;

        buffer->decode_capacity_samples =
            0;
    }


    buffer->count =
        0;

    buffer->free_top =
        0;
}


/* -------------------------------------------------------------------------- */
/* Flush                                                                      */
/* -------------------------------------------------------------------------- */

void audio_buffer_flush(audio_buffer_t *buffer)
{
    if (!buffer ||
        !buffer->pool)
    {
        return;
    }

    portENTER_CRITICAL(&buffer->lock);

    /*
     * Return every queued slot to the free-slot stack.
     */
    for (int i = 0;
         i < buffer->count;
         i++)
    {
        buffer->free_stack[
            buffer->free_top++
        ] = buffer->sorted[i];
    }

    buffer->count =
        0;

    portEXIT_CRITICAL(&buffer->lock);


    /*
     * Drain corresponding semaphore notifications.
     */
    while (xSemaphoreTake(
               buffer->data_ready,
               0) == pdTRUE)
    {
    }
}


/* -------------------------------------------------------------------------- */
/* Frame count                                                                */
/* -------------------------------------------------------------------------- */

int audio_buffer_get_frame_count(audio_buffer_t *buffer)
{
    if (!buffer) {
        return 0;
    }

    int frames = 0;

    portENTER_CRITICAL(&buffer->lock);

    frames =
        buffer->count;

    portEXIT_CRITICAL(&buffer->lock);

    return frames;
}


/* -------------------------------------------------------------------------- */
/* Back-pressure check                                                        */
/* -------------------------------------------------------------------------- */

bool audio_buffer_is_nearly_full(audio_buffer_t *buffer)
{
    if (!buffer) {
        return false;
    }

    int count = 0;

    portENTER_CRITICAL(&buffer->lock);

    count =
        buffer->count;

    portEXIT_CRITICAL(&buffer->lock);


    /*
     * Consider >90% full to be back-pressure territory.
     */
    return count >
           (
               buffer->capacity *
               9 /
               10
           );
}


/* -------------------------------------------------------------------------- */
/* Take oldest frame                                                          */
/* -------------------------------------------------------------------------- */

bool audio_buffer_take(audio_buffer_t *buffer,
                       void **item,
                       size_t *item_size,
                       TickType_t ticks)
{
    if (!buffer ||
        !buffer->pool ||
        !item ||
        !item_size)
    {
        return false;
    }


    /*
     * Wait until a frame becomes available.
     */
    if (xSemaphoreTake(
            buffer->data_ready,
            ticks) != pdTRUE)
    {
        return false;
    }


    portENTER_CRITICAL(&buffer->lock);


    if (buffer->count == 0) {
        /*
         * Semaphore and count should normally remain synchronized.
         */
        portEXIT_CRITICAL(&buffer->lock);

        return false;
    }


    uint16_t slot =
        buffer->sorted[0];


    /*
     * Shift remaining sorted indices down.
     */
    if (buffer->count > 1) {
        memmove(
            &buffer->sorted[0],
            &buffer->sorted[1],
            (buffer->count - 1) *
                sizeof(uint16_t)
        );
    }

    buffer->count--;


    portEXIT_CRITICAL(&buffer->lock);


    uint8_t *ptr =
        slot_ptr(buffer, slot);

    audio_frame_header_t *hdr =
        (audio_frame_header_t *)ptr;


    *item =
        ptr;

    *item_size =
        sizeof(audio_frame_header_t) +
        (size_t)hdr->samples_per_channel *
            (size_t)hdr->channels *
            sizeof(int16_t);


    return true;
}


/* -------------------------------------------------------------------------- */
/* Return consumed slot                                                       */
/* -------------------------------------------------------------------------- */

void audio_buffer_return(audio_buffer_t *buffer,
                         void *item)
{
    if (!buffer ||
        !buffer->pool ||
        !item)
    {
        return;
    }


    uint16_t slot =
        (uint16_t)(
            (
                (uint8_t *)item -
                buffer->pool
            ) /
            buffer->slot_size
        );


    /*
     * Basic protection against a bad pointer corrupting the free stack.
     */
    if (slot >= (uint16_t)buffer->capacity) {
        ESP_LOGE(
            TAG,
            "Attempt to return invalid audio slot %u capacity=%d",
            (unsigned)slot,
            buffer->capacity
        );

        return;
    }


    portENTER_CRITICAL(&buffer->lock);


    if (buffer->free_top >= buffer->capacity) {
        /*
         * A double-return would otherwise write beyond free_stack[].
         */
        portEXIT_CRITICAL(&buffer->lock);

        ESP_LOGE(
            TAG,
            "Audio free stack overflow while returning slot %u",
            (unsigned)slot
        );

        return;
    }


    buffer->free_stack[
        buffer->free_top++
    ] = slot;


    portEXIT_CRITICAL(&buffer->lock);
}


/* -------------------------------------------------------------------------- */
/* Decode workspace accessor                                                  */
/* -------------------------------------------------------------------------- */

int16_t *audio_buffer_get_decode_buffer(audio_buffer_t *buffer,
                                        size_t *capacity_samples)
{
    if (!buffer) {
        return NULL;
    }

    if (capacity_samples) {
        *capacity_samples =
            buffer->decode_capacity_samples;
    }

    return buffer->decode_buffer;
}


/* -------------------------------------------------------------------------- */
/* Peek oldest RTP timestamp                                                  */
/* -------------------------------------------------------------------------- */

bool audio_buffer_oldest_timestamp(audio_buffer_t *buffer,
                                   uint32_t *timestamp)
{
    if (!buffer ||
        !buffer->pool ||
        !timestamp)
    {
        return false;
    }


    portENTER_CRITICAL(&buffer->lock);


    if (buffer->count == 0) {
        portEXIT_CRITICAL(&buffer->lock);

        return false;
    }


    *timestamp =
        slot_timestamp(
            buffer,
            buffer->sorted[0]
        );


    portEXIT_CRITICAL(&buffer->lock);

    return true;
}


/* -------------------------------------------------------------------------- */
/* Queue decoded PCM                                                          */
/* -------------------------------------------------------------------------- */

bool audio_buffer_queue_decoded(audio_buffer_t *buffer,
                                audio_stats_t *stats,
                                uint32_t timestamp,
                                const int16_t *pcm_data,
                                size_t samples,
                                int channels)
{
    if (!buffer ||
        !pcm_data ||
        samples == 0)
    {
        return false;
    }


    if (channels <= 0) {
        channels = 2;
    }


    if (channels > AUDIO_MAX_CHANNELS) {
        if (stats) {
            stats->packets_dropped++;
        }

        ESP_LOGW(
            TAG,
            "Invalid decoded channel count: %d",
            channels
        );

        return false;
    }


    size_t offset =
        0;

    uint32_t chunk_timestamp =
        timestamp;


    /*
     * AAC-LC produces 1024 frames/channel.
     *
     * Existing ring slots hold AAC_FRAMES_PER_PACKET (352) frames, so split
     * larger decoded frames across multiple contiguous timestamped slots.
     *
     * Example:
     *
     *   1024 -> 352 + 352 + 320
     */
    while (offset < samples) {
        size_t chunk_samples =
            samples - offset;

        if (chunk_samples >
            AAC_FRAMES_PER_PACKET)
        {
            chunk_samples =
                AAC_FRAMES_PER_PACKET;
        }


        if (!audio_buffer_queue_chunk(
                buffer,
                stats,
                chunk_timestamp,
                pcm_data +
                    (
                        offset *
                        (size_t)channels
                    ),
                chunk_samples,
                channels))
        {
            return false;
        }


        offset +=
            chunk_samples;

        chunk_timestamp +=
            (uint32_t)chunk_samples;
    }


    if (stats) {
        stats->packets_decoded++;
    }


    return true;
}