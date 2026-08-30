#include <Arduino.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#include "audio_decoder.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "ALAC.h"

extern "C" {
#include "alac_magic_cookie.h"
#include "libhelix-aac/aacdec.h"
}


/* ========================================================================== */
/* Configuration                                                              */
/* ========================================================================== */

#define MAX_FALLBACK_CHANNELS  2
#define ALAC_COOKIE_SIZE       24

/*
 * AirPlay 2 type 103 AAC-LC normally produces 1024 PCM frames per channel.
 */
#define AAC_DEFAULT_FRAME_SIZE 1024

/*
 * Protect against nonsensical Helix output information.
 *
 * Your audio_buffer currently provides 4096 PCM frames/channel, so this
 * still leaves plenty of room.
 */
#define AAC_MAX_REASONABLE_CHANNELS 2
#define AAC_MAX_REASONABLE_FRAMES   4096


/* ========================================================================== */
/* Decoder type                                                               */
/* ========================================================================== */

typedef enum {
    AUDIO_DECODER_NONE = 0,
    AUDIO_DECODER_PCM,
    AUDIO_DECODER_ALAC,
    AUDIO_DECODER_AAC
} audio_decoder_kind_t;


/* ========================================================================== */
/* Decoder object                                                             */
/* ========================================================================== */

struct audio_decoder {
    audio_decoder_kind_t kind;
    audio_format_t format;

    /* ALAC */
    ALACDecoder *alac;
    bool alac_ready;

    /* AAC / Helix */
    HAACDecoder aac;
    bool aac_ready;

    /* Diagnostics */
    bool aac_first_decode_logged;
};


/* ========================================================================== */
/* Globals                                                                    */
/* ========================================================================== */

static const char *TAG = "audio_dec";


/* ========================================================================== */
/* Heap diagnostics                                                           */
/* ========================================================================== */

static void log_heap(const char *where)
{
    ESP_LOGI(
        TAG,
        "HEAP[%s] internal=%u dma=%u largest_internal=%u largest_dma=%u psram=%u",
        where ? where : "?",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
    );
}


/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

static bool codec_is(const char *codec, const char *name)
{
    return codec &&
           name &&
           strcmp(codec, name) == 0;
}


static int get_channels(const audio_format_t *fmt)
{
    if (!fmt || fmt->channels <= 0) {
        return MAX_FALLBACK_CHANNELS;
    }

    return fmt->channels;
}


static int get_frame_size(const audio_format_t *fmt)
{
    if (!fmt || fmt->frame_size <= 0) {
        return 352;
    }

    return fmt->frame_size;
}


static int get_sample_rate(const audio_format_t *fmt)
{
    if (!fmt || fmt->sample_rate <= 0) {
        return 44100;
    }

    return fmt->sample_rate;
}


/* ========================================================================== */
/* AAC initialisation                                                         */
/* ========================================================================== */

static bool init_aac(audio_decoder_t *decoder)
{
    if (!decoder) {
        return false;
    }

    log_heap("before AACInitDecoder");

    decoder->aac = AACInitDecoder();

    log_heap("after AACInitDecoder");

    if (!decoder->aac) {
        ESP_LOGE(TAG, "AACInitDecoder failed");
        return false;
    }

    AACFrameInfo raw_info;
    memset(&raw_info, 0, sizeof(raw_info));

    raw_info.nChans =
        get_channels(&decoder->format);

    raw_info.sampRateCore =
        get_sample_rate(&decoder->format);

    /*
     * AirPlay buffered stream type 103 in this project is AAC-LC.
     */
    raw_info.profile = AAC_PROFILE_LC;

    int rc =
        AACSetRawBlockParams(
            decoder->aac,
            0,
            &raw_info
        );

    if (rc != 0) {
        ESP_LOGE(
            TAG,
            "AACSetRawBlockParams failed rc=%d",
            rc
        );

        AACFreeDecoder(decoder->aac);
        decoder->aac = nullptr;

        return false;
    }

    decoder->aac_ready = true;

    ESP_LOGI(
        TAG,
        "Helix AAC-LC decoder selected rate=%d channels=%d frame_size=%d",
        get_sample_rate(&decoder->format),
        get_channels(&decoder->format),
        decoder->format.frame_size > 0
            ? decoder->format.frame_size
            : AAC_DEFAULT_FRAME_SIZE
    );

    log_heap("AAC ready");

    return true;
}


/* ========================================================================== */
/* Create                                                                     */
/* ========================================================================== */

audio_decoder_t *audio_decoder_create(
    const audio_decoder_config_t *config)
{
    if (!config) {
        return NULL;
    }

    audio_decoder_t *decoder =
        (audio_decoder_t *)calloc(
            1,
            sizeof(*decoder)
        );

    if (!decoder) {
        return NULL;
    }

    decoder->format = config->format;

    decoder->kind = AUDIO_DECODER_NONE;

    decoder->alac = nullptr;
    decoder->alac_ready = false;

    decoder->aac = nullptr;
    decoder->aac_ready = false;
    decoder->aac_first_decode_logged = false;

    ESP_LOGI(
        TAG,
        "Requested codec=%s rate=%d channels=%d frame_size=%d",
        config->format.codec
            ? config->format.codec
            : "(null)",
        config->format.sample_rate,
        config->format.channels,
        config->format.frame_size
    );


    /* ====================================================================== */
    /* PCM                                                                    */
    /* ====================================================================== */

    if (codec_is(config->format.codec, "L16") ||
        codec_is(config->format.codec, "PCM"))
    {
        decoder->kind = AUDIO_DECODER_PCM;

        ESP_LOGI(TAG, "PCM/L16 decoder selected");

        return decoder;
    }


    /* ====================================================================== */
    /* ALAC                                                                   */
    /* ====================================================================== */

    if (codec_is(config->format.codec, "ALAC") ||
        codec_is(config->format.codec, "AppleLossless"))
    {
        decoder->kind = AUDIO_DECODER_ALAC;

        decoder->alac = new ALACDecoder();

        if (!decoder->alac) {
            ESP_LOGE(TAG, "Failed to allocate ALACDecoder");

            free(decoder);

            return NULL;
        }

        uint8_t cookie[ALAC_COOKIE_SIZE];

        build_alac_magic_cookie(
            cookie,
            &decoder->format
        );

        int32_t rc =
            decoder->alac->Init(
                cookie,
                sizeof(cookie)
            );

        if (rc != 0) {
            ESP_LOGE(
                TAG,
                "ALAC Init failed rc=%ld",
                (long)rc
            );

            delete decoder->alac;
            decoder->alac = nullptr;

            free(decoder);

            return NULL;
        }

        decoder->alac_ready = true;

        ESP_LOGI(
            TAG,
            "ALAC decoder selected frame_size=%d channels=%d",
            get_frame_size(&decoder->format),
            get_channels(&decoder->format)
        );

        return decoder;
    }


    /* ====================================================================== */
    /* AAC                                                                    */
    /* ====================================================================== */

    if (codec_is(config->format.codec, "AAC") ||
        codec_is(config->format.codec, "AAC-LC") ||
        codec_is(config->format.codec, "MPEG4AAC"))
    {
        decoder->kind = AUDIO_DECODER_AAC;

        if (!init_aac(decoder)) {
            ESP_LOGE(
                TAG,
                "AAC decoder initialization failed"
            );

            free(decoder);

            return NULL;
        }

        return decoder;
    }


    /* ====================================================================== */
    /* Unsupported                                                            */
    /* ====================================================================== */

    ESP_LOGW(
        TAG,
        "Unsupported codec: %s",
        config->format.codec
            ? config->format.codec
            : "(null)"
    );

    return decoder;
}


/* ========================================================================== */
/* Destroy                                                                    */
/* ========================================================================== */

void audio_decoder_destroy(
    audio_decoder_t *decoder)
{
    if (!decoder) {
        return;
    }

    if (decoder->alac) {
        delete decoder->alac;
        decoder->alac = nullptr;
    }

    if (decoder->aac) {
        AACFreeDecoder(decoder->aac);
        decoder->aac = nullptr;
    }

    free(decoder);
}


/* ========================================================================== */
/* PCM                                                                        */
/* ========================================================================== */

/*
 * IMPORTANT:
 *
 * output_capacity_samples means PCM frames / samples per channel.
 *
 * Therefore with:
 *
 *   capacity = 4096
 *   channels = 2
 *
 * output physically has room for 8192 int16_t values.
 */
static int decode_pcm(
    audio_decoder_t *decoder,
    const uint8_t *input,
    size_t input_len,
    int16_t *output,
    size_t output_capacity_samples,
    audio_decode_info_t *info)
{
    if (!decoder ||
        !input ||
        !output ||
        output_capacity_samples == 0)
    {
        return -1;
    }

    int channels =
        get_channels(&decoder->format);

    if (channels <= 0 ||
        channels > MAX_FALLBACK_CHANNELS)
    {
        channels = MAX_FALLBACK_CHANNELS;
    }

    size_t bytes_per_frame =
        (size_t)channels *
        sizeof(int16_t);

    if (bytes_per_frame == 0) {
        return -1;
    }

    size_t input_frames =
        input_len / bytes_per_frame;

    size_t frames_to_copy =
        input_frames;

    if (frames_to_copy > output_capacity_samples) {
        frames_to_copy = output_capacity_samples;
    }

    const uint8_t *src = input;
    int16_t *dst = output;

    size_t total_interleaved_samples =
        frames_to_copy *
        (size_t)channels;

    for (size_t i = 0;
         i < total_interleaved_samples;
         ++i)
    {
        uint16_t be =
            ((uint16_t)src[i * 2] << 8) |
            ((uint16_t)src[i * 2 + 1]);

        dst[i] = (int16_t)be;
    }

    if (info) {
        info->channels = channels;
    }

    return (int)frames_to_copy;
}


/* ========================================================================== */
/* ALAC                                                                       */
/* ========================================================================== */

static int decode_alac(
    audio_decoder_t *decoder,
    const uint8_t *input,
    size_t input_len,
    int16_t *output,
    size_t output_capacity_samples,
    audio_decode_info_t *info)
{
    if (!decoder ||
        !decoder->alac ||
        !decoder->alac_ready ||
        !input ||
        input_len == 0 ||
        !output ||
        output_capacity_samples == 0)
    {
        return -1;
    }

    int channels =
        get_channels(&decoder->format);

    if (channels <= 0 ||
        channels > MAX_FALLBACK_CHANNELS)
    {
        channels = MAX_FALLBACK_CHANNELS;
    }

    int frame_size =
        get_frame_size(&decoder->format);

    if (frame_size <= 0) {
        return -1;
    }

    /*
     * frame_size is samples/channel, matching the audio pipeline.
     */
    if ((size_t)frame_size > output_capacity_samples) {
        ESP_LOGW(
            TAG,
            "ALAC output buffer too small cap=%u need=%d frames",
            (unsigned)output_capacity_samples,
            frame_size
        );

        return -1;
    }

    BitBuffer bits;

    BitBufferInit(
        &bits,
        (uint8_t *)input,
        (uint32_t)input_len
    );

    uint32_t out_frames = 0;

    int32_t rc =
        decoder->alac->Decode(
            &bits,
            (uint8_t *)output,
            (uint32_t)frame_size,
            (uint32_t)channels,
            &out_frames
        );

    if (rc != 0 ||
        out_frames == 0)
    {
        static uint32_t fail_count = 0;

        if ((fail_count++ % 100U) == 0) {
            ESP_LOGW(
                TAG,
                "ALAC Decode failed rc=%ld input_len=%u out_frames=%lu",
                (long)rc,
                (unsigned)input_len,
                (unsigned long)out_frames
            );
        }

        return -1;
    }

    if ((size_t)out_frames > output_capacity_samples) {
        ESP_LOGE(
            TAG,
            "ALAC produced too many frames: got=%lu cap=%u",
            (unsigned long)out_frames,
            (unsigned)output_capacity_samples
        );

        /*
         * The decoder has already written at this point, but this condition
         * should never occur when frame_size <= capacity.
         */
        return -1;
    }

    if (info) {
        info->channels = channels;
    }

    return (int)out_frames;
}


/* ========================================================================== */
/* AAC / Helix                                                                */
/* ========================================================================== */

static int decode_aac(
    audio_decoder_t *decoder,
    const uint8_t *input,
    size_t input_len,
    int16_t *output,
    size_t output_capacity_samples,
    audio_decode_info_t *info)
{
    if (!decoder ||
        !decoder->aac ||
        !decoder->aac_ready ||
        !input ||
        input_len == 0 ||
        !output ||
        output_capacity_samples == 0)
    {
        return -1;
    }

    int expected_channels =
        get_channels(&decoder->format);

    if (expected_channels <= 0 ||
        expected_channels > AAC_MAX_REASONABLE_CHANNELS)
    {
        expected_channels = 2;
    }

    int expected_frames =
        decoder->format.frame_size > 0
            ? decoder->format.frame_size
            : AAC_DEFAULT_FRAME_SIZE;

    if (expected_frames <= 0 ||
        expected_frames > AAC_MAX_REASONABLE_FRAMES)
    {
        ESP_LOGE(
            TAG,
            "Invalid AAC frame size %d",
            expected_frames
        );

        return -1;
    }

    /*
     * output_capacity_samples is capacity PER CHANNEL.
     *
     * Example:
     *
     *   AAC 1024 frames
     *   stereo
     *
     * needs 1024 frames capacity and physically writes:
     *
     *   1024 * 2 = 2048 int16_t values
     *
     * Your current audio_buffer supplies capacity=4096 and physically
     * allocates 4096 * 2 int16_t values.
     */
    if ((size_t)expected_frames > output_capacity_samples) {
        ESP_LOGE(
            TAG,
            "AAC output buffer too small: cap=%u need=%d frames",
            (unsigned)output_capacity_samples,
            expected_frames
        );

        return -1;
    }

    if (!decoder->aac_first_decode_logged) {
        ESP_LOGI(
            TAG,
            "AAC first frame input=%u cap_frames=%u expected_frames=%d channels=%d output_bytes_available=%u",
            (unsigned)input_len,
            (unsigned)output_capacity_samples,
            expected_frames,
            expected_channels,
            (unsigned)(
                output_capacity_samples *
                (size_t)expected_channels *
                sizeof(int16_t)
            )
        );

        log_heap("before first AACDecode");
    }

    /*
     * Helix API expects a mutable input pointer even though it does not
     * need ownership of our source buffer.
     */
    unsigned char *aac_data =
        (unsigned char *)input;

    int bytes_left =
        (int)input_len;

    /*
     * Helix does not accept an output-capacity parameter.
     *
     * We have already verified our output buffer is large enough for the
     * negotiated AAC frame size before entering AACDecode().
     */
    int rc =
        AACDecode(
            decoder->aac,
            &aac_data,
            &bytes_left,
            (short *)output
        );

    if (!decoder->aac_first_decode_logged) {
        log_heap("after first AACDecode");
    }

    if (rc != 0) {
        static uint32_t fail_count = 0;

        if ((fail_count++ % 50U) == 0) {
            ESP_LOGW(
                TAG,
                "AACDecode failed rc=%d input=%u consumed=%u bytes_left=%d",
                rc,
                (unsigned)input_len,
                (unsigned)(
                    input_len -
                    (size_t)(
                        bytes_left >= 0
                            ? bytes_left
                            : 0
                    )
                ),
                bytes_left
            );
        }

        decoder->aac_first_decode_logged = true;

        return -1;
    }

    AACFrameInfo frame_info;
    memset(&frame_info, 0, sizeof(frame_info));

    AACGetLastFrameInfo(
        decoder->aac,
        &frame_info
    );

    if (frame_info.outputSamps <= 0) {
        ESP_LOGW(
            TAG,
            "AAC decoded zero samples"
        );

        decoder->aac_first_decode_logged = true;

        return -1;
    }

    int decoded_channels =
        frame_info.nChans;

    if (decoded_channels <= 0 ||
        decoded_channels > AAC_MAX_REASONABLE_CHANNELS)
    {
        ESP_LOGE(
            TAG,
            "AAC invalid channel count %d",
            decoded_channels
        );

        decoder->aac_first_decode_logged = true;

        return -1;
    }

    /*
     * Helix outputSamps is the TOTAL number of interleaved int16_t values.
     *
     * Stereo AAC-LC normally reports:
     *
     *   outputSamps = 2048
     *   channels    = 2
     *
     * Therefore:
     *
     *   decoded_frames = 1024
     */
    if ((frame_info.outputSamps % decoded_channels) != 0) {
        ESP_LOGE(
            TAG,
            "AAC output sample count not divisible by channels: samples=%d ch=%d",
            frame_info.outputSamps,
            decoded_channels
        );

        decoder->aac_first_decode_logged = true;

        return -1;
    }

    int decoded_frames =
        frame_info.outputSamps /
        decoded_channels;

    if (decoded_frames <= 0 ||
        decoded_frames > AAC_MAX_REASONABLE_FRAMES)
    {
        ESP_LOGE(
            TAG,
            "AAC invalid decoded frame count %d",
            decoded_frames
        );

        decoder->aac_first_decode_logged = true;

        return -1;
    }

    if ((size_t)decoded_frames > output_capacity_samples) {
        ESP_LOGE(
            TAG,
            "AAC output exceeded supplied capacity: frames=%d capacity=%u samples=%d channels=%d",
            decoded_frames,
            (unsigned)output_capacity_samples,
            frame_info.outputSamps,
            decoded_channels
        );

        decoder->aac_first_decode_logged = true;

        return -1;
    }

    if (info) {
        info->channels =
            decoded_channels;
    }

    if (!decoder->aac_first_decode_logged) {
        ESP_LOGI(
            TAG,
            "AAC FIRST DECODE OK: input=%u consumed=%u frames=%d total_samples=%d ch=%d rate=%d bits=%d profile=%d",
            (unsigned)input_len,
            (unsigned)(
                input_len -
                (size_t)(
                    bytes_left >= 0
                        ? bytes_left
                        : 0
                )
            ),
            decoded_frames,
            frame_info.outputSamps,
            decoded_channels,
            frame_info.sampRateOut,
            frame_info.bitsPerSample,
            frame_info.profile
        );

        log_heap("first AAC frame complete");

        decoder->aac_first_decode_logged = true;
    }

    /*
     * The audio pipeline expects samples/channel (PCM frames), not total
     * interleaved int16_t values.
     */
    return decoded_frames;
}


/* ========================================================================== */
/* Public decode                                                              */
/* ========================================================================== */

int audio_decoder_decode(
    audio_decoder_t *decoder,
    const uint8_t *input,
    size_t input_len,
    int16_t *output,
    size_t output_capacity_samples,
    audio_decode_info_t *info)
{
    if (!decoder ||
        !input ||
        !output ||
        output_capacity_samples == 0)
    {
        return -1;
    }

    if (info) {
        info->channels = 0;
    }

    switch (decoder->kind)
    {
        case AUDIO_DECODER_PCM:
            return decode_pcm(
                decoder,
                input,
                input_len,
                output,
                output_capacity_samples,
                info
            );

        case AUDIO_DECODER_ALAC:
            return decode_alac(
                decoder,
                input,
                input_len,
                output,
                output_capacity_samples,
                info
            );

        case AUDIO_DECODER_AAC:
            return decode_aac(
                decoder,
                input,
                input_len,
                output,
                output_capacity_samples,
                info
            );

        case AUDIO_DECODER_NONE:
        default:
            return -1;
    }
}


/* ========================================================================== */
/* Decoder-type helpers                                                       */
/* ========================================================================== */

bool audio_decoder_is_aac(
    const audio_decoder_t *decoder)
{
    return decoder &&
           decoder->kind ==
               AUDIO_DECODER_AAC;
}


bool audio_decoder_is_alac(
    const audio_decoder_t *decoder)
{
    return decoder &&
           decoder->kind ==
               AUDIO_DECODER_ALAC;
}