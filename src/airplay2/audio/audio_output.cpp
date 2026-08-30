#include <Arduino.h>

extern "C" {
#include "audio_receiver.h"
#include "audio_resample.h"
#include "led.h"
#include "rtsp_server.h"

extern "C" int audio_receiver_buffered_frames(void);
}

#include "audio_output.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <stdlib.h>

#include <Wire.h>


/* ============================================================
 * OUTPUT SELECTION
 * ============================================================ */

#define AUDIO_DAC_ES8311   0
#define AUDIO_DAC_PCM5102  1
#define AUDIO_DAC_BOTH     2

/*
 * Select output here.
 */
#define AUDIO_DAC AUDIO_DAC_BOTH
//#define AUDIO_DAC AUDIO_DAC_PCM5102


/* ============================================================
 * GENERAL CONFIG
 * ============================================================ */

#define AUDIO_MCLK_MULTIPLE 256

#define ES8311_ADDR 0x18
#define P4_PA_CTRL  53

#define AUDIO_START_BUFFER_FRAMES 64
#define AUDIO_LOW_WATER_FRAMES    2

#define TAG "audio_output"

#define OUTPUT_RATE   CONFIG_OUTPUT_SAMPLE_RATE_HZ
#define FRAME_SAMPLES 352

#define MAX_RESAMPLE_FRAMES \
  ((size_t)((FRAME_SAMPLES + 2) * ((double)OUTPUT_RATE / 44100) + 16))


/* ============================================================
 * I2S PINS
 * ============================================================ */

/* Baseline pin definitions, kept unchanged for each DAC. */
#define ES8311_I2S_SCK_PIN   CONFIG_I2S_SCK_IO
#define ES8311_I2S_BCK_PIN   CONFIG_I2S_BCK_IO
#define ES8311_I2S_LRCK_PIN  CONFIG_I2S_WS_IO
#define ES8311_I2S_DOUT_PIN  CONFIG_I2S_DO_IO

#define PCM5102_I2S_SCK_PIN   (-1)
#define PCM5102_I2S_BCK_PIN   27
#define PCM5102_I2S_LRCK_PIN  32
#define PCM5102_I2S_DOUT_PIN  33


#if CONFIG_FREERTOS_UNICORE
#define PLAYBACK_CORE 0
#else
#define PLAYBACK_CORE 1
#endif


/* ============================================================
 * GLOBALS
 * ============================================================ */

static i2s_chan_handle_t es8311_tx_handle = NULL;
static i2s_chan_handle_t pcm5102_tx_handle = NULL;

static volatile bool flush_requested = false;
static volatile bool playback_running = false;

static TaskHandle_t playback_task_handle = NULL;

static volatile int source_rate = 44100;
static volatile bool resample_reinit_needed = false;

/* ES8311 hardware DAC volume. 0xBF is 0 dB / full scale. */
static volatile uint8_t g_es8311_volume_percent = 0;


/* ============================================================
 * SOFTWARE VOLUME
 * ============================================================ */

static void apply_volume(int16_t *buf, size_t n)
{
#ifndef CONFIG_DAC_CONTROLS_VOLUME
    int32_t vol = airplay_get_volume_q15();

    for (size_t i = 0; i < n; i++) {
        buf[i] =
            (int16_t)(((int32_t)buf[i] * vol) >> 15);
    }
#else
    (void)buf;
    (void)n;
#endif
}


/* ============================================================
 * ES8311 SUPPORT
 * ============================================================ */

#if AUDIO_DAC == AUDIO_DAC_ES8311 || AUDIO_DAC == AUDIO_DAC_BOTH

static bool es8311_write(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(val);

    return Wire.endTransmission() == 0;
}


static uint8_t es8311_read(uint8_t reg)
{
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
        return 0xFF;
    }

    if (Wire.requestFrom(ES8311_ADDR, (uint8_t)1) != 1) {
        return 0xFF;
    }

    return Wire.read();
}


static bool es8311_set_volume_percent_internal(uint8_t percent)
{
    if (percent > 100) percent = 100;

    /* ES8311 DAC digital volume register 0x32: 0x00..0xBF. */
    uint8_t reg = (uint8_t)(((uint32_t)percent * 0xBFu) / 100u);

    if (!es8311_write(0x32, reg)) {
        return false;
    }

    g_es8311_volume_percent = percent;
    Serial.printf("[ES8311] volume=%u%% reg32=0x%02X\n",
                  (unsigned)percent, (unsigned)reg);
    return true;
}


static bool es8311_set_sample_rate(uint32_t sample_rate)
{
    uint32_t mclk =
        sample_rate * AUDIO_MCLK_MULTIPLE;

    Serial.printf(
        "[ES8311] clock config rate=%lu mclk=%lu\n",
        (unsigned long)sample_rate,
        (unsigned long)mclk
    );

    /* Proven-good ES8311 clocking: MCLK = 256 x Fs. */
    if (sample_rate == 44100 ||
        sample_rate == 48000) {
        es8311_write(0x02, 0x00);
    }
    else {
        Serial.printf(
            "[ES8311] unsupported rate %lu\n",
            (unsigned long)sample_rate
        );

        return false;
    }

    es8311_write(0x03, 0x10);
    es8311_write(0x04, 0x10);
    es8311_write(0x05, 0x00);
    es8311_write(0x06, 0x03);
    es8311_write(0x07, 0x00);
    es8311_write(0x08, 0xFF);

    return true;
}


static void es8311_init_manual(uint32_t sample_rate)
{
    Serial.println("[ES8311] manual init");

    es8311_write(0x00, 0x1F);
    delay(20);

    es8311_write(0x00, 0x00);
    es8311_write(0x00, 0x80);
    delay(20);

    es8311_write(0x01, 0x3F);

    es8311_write(
        0x00,
        es8311_read(0x00) & 0xBF
    );

    es8311_write(0x09, 0x0C);
    es8311_write(0x0A, 0x0C);

    es8311_write(0x0D, 0x01);
    es8311_write(0x0E, 0x02);

    es8311_write(0x12, 0x00);
    es8311_write(0x13, 0x10);

    es8311_write(0x1C, 0x6A);
    es8311_write(0x37, 0x08);

    es8311_write(0x17, 0xC8);
    es8311_write(0x14, 0x1A);

    es8311_write(0x32, 0xC5);

    uint8_t reg31 =
        es8311_read(0x31);

    reg31 &=
        ~(BIT(6) | BIT(5));

    es8311_write(
        0x31,
        reg31
    );

    es8311_set_sample_rate(
        sample_rate
    );

    /* Normalise startup volume to a documented 0x00..0xBF value. */
    es8311_set_volume_percent_internal(g_es8311_volume_percent);

    Serial.printf(
        "[ES8311] R00=%02X R01=%02X R02=%02X R03=%02X "
        "R04=%02X R06=%02X R08=%02X R31=%02X R32=%02X\n",
        es8311_read(0x00),
        es8311_read(0x01),
        es8311_read(0x02),
        es8311_read(0x03),
        es8311_read(0x04),
        es8311_read(0x06),
        es8311_read(0x08),
        es8311_read(0x31),
        es8311_read(0x32)
    );
}

#endif


/* ============================================================
 * INIT
 * ============================================================ */

static esp_err_t init_es8311_exact_baseline(void)
{
#if AUDIO_DAC == AUDIO_DAC_ES8311 || AUDIO_DAC == AUDIO_DAC_BOTH
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_0,
            I2S_ROLE_MASTER
        );

    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 256;
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(
        i2s_new_channel(
            &chan_cfg,
            &es8311_tx_handle,
            NULL
        ),
        TAG,
        "channel create failed"
    );

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            I2S_STD_CLK_DEFAULT_CONFIG(
                OUTPUT_RATE
            ),

        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT,
                I2S_SLOT_MODE_STEREO
            ),

        .gpio_cfg =
        {
            .mclk = (gpio_num_t)ES8311_I2S_SCK_PIN,
            .bclk = (gpio_num_t)ES8311_I2S_BCK_PIN,
            .ws   = (gpio_num_t)ES8311_I2S_LRCK_PIN,
            .dout = (gpio_num_t)ES8311_I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
        },
    };

    std_cfg.clk_cfg.mclk_multiple =
        I2S_MCLK_MULTIPLE_256;

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(
            es8311_tx_handle,
            &std_cfg
        ),
        TAG,
        "std mode init failed"
    );

    ESP_RETURN_ON_ERROR(
        i2s_channel_enable(
            es8311_tx_handle
        ),
        TAG,
        "channel enable failed"
    );

    Wire.begin(7, 8);
    Wire.setClock(100000);

    gpio_reset_pin(
        (gpio_num_t)P4_PA_CTRL
    );

    gpio_set_direction(
        (gpio_num_t)P4_PA_CTRL,
        GPIO_MODE_OUTPUT
    );

    gpio_set_level(
        (gpio_num_t)P4_PA_CTRL,
        0
    );

    delay(200);

    es8311_init_manual(
        OUTPUT_RATE
    );

    delay(200);

    gpio_set_level(
        (gpio_num_t)P4_PA_CTRL,
        1
    );

    Serial.printf(
        "[PA] GPIO53=%d\n",
        gpio_get_level(
            (gpio_num_t)P4_PA_CTRL
        )
    );
#endif

    return ESP_OK;
}


static esp_err_t init_pcm5102_exact_baseline(void)
{
#if AUDIO_DAC == AUDIO_DAC_PCM5102 || AUDIO_DAC == AUDIO_DAC_BOTH
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_1,
            I2S_ROLE_MASTER
        );

    /*
     * PCM5102 is the secondary mirror in BOTH mode.
     *
     * Do not allocate another 16 x 512 DMA ring here: that consumes
     * roughly another 32 KB of scarce internal DMA-capable RAM and
     * leaves Helix AAC with almost no contiguous internal heap.
     *
     * 8 x 256 = ~8 KB at 16-bit stereo, while still providing about
     * 46 ms of hardware buffering at 44.1 kHz.
     */
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 256;
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(
        i2s_new_channel(
            &chan_cfg,
            &pcm5102_tx_handle,
            NULL
        ),
        TAG,
        "channel create failed"
    );

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            I2S_STD_CLK_DEFAULT_CONFIG(
                OUTPUT_RATE
            ),

        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT,
                I2S_SLOT_MODE_STEREO
            ),

        .gpio_cfg =
        {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)PCM5102_I2S_BCK_PIN,
            .ws   = (gpio_num_t)PCM5102_I2S_LRCK_PIN,
            .dout = (gpio_num_t)PCM5102_I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
        },
    };

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(
            pcm5102_tx_handle,
            &std_cfg
        ),
        TAG,
        "std mode init failed"
    );

    ESP_RETURN_ON_ERROR(
        i2s_channel_enable(
            pcm5102_tx_handle
        ),
        TAG,
        "channel enable failed"
    );

    Serial.println("[AUDIO] external PCM5102 selected");
#endif

    return ESP_OK;
}


esp_err_t audio_output_init(void)
{
#if AUDIO_DAC == AUDIO_DAC_ES8311 || AUDIO_DAC == AUDIO_DAC_BOTH
    ESP_RETURN_ON_ERROR(
        init_es8311_exact_baseline(),
        TAG,
        "ES8311 init failed"
    );
#endif

#if AUDIO_DAC == AUDIO_DAC_PCM5102 || AUDIO_DAC == AUDIO_DAC_BOTH
    ESP_RETURN_ON_ERROR(
        init_pcm5102_exact_baseline(),
        TAG,
        "PCM5102 init failed"
    );
#endif

    return ESP_OK;
}


/*
 * PRIMARY write deliberately keeps the baseline timing:
 * - ES8311/BOTH: ES8311 is the baseline primary output and uses the
 *   original wait value.
 * - PCM5102-only: PCM5102 uses the original wait value.
 *
 * In BOTH mode the PCM5102 mirror is non-blocking so it cannot add a
 * second 0..100 ms blocking interval ahead of the original frame pacing.
 */
static esp_err_t write_selected_outputs(
    const void *data,
    size_t bytes,
    size_t *written,
    TickType_t wait)
{
#if AUDIO_DAC == AUDIO_DAC_BOTH
    size_t pcm_written = 0;

    /* Secondary mirror: do not disturb original ES8311 timing. */
    (void)i2s_channel_write(
        pcm5102_tx_handle,
        data,
        bytes,
        &pcm_written,
        0
    );

    /* Baseline ES8311 write, unchanged timeout semantics. */
    return i2s_channel_write(
        es8311_tx_handle,
        data,
        bytes,
        written,
        wait
    );

#elif AUDIO_DAC == AUDIO_DAC_ES8311

    return i2s_channel_write(
        es8311_tx_handle,
        data,
        bytes,
        written,
        wait
    );

#else

    return i2s_channel_write(
        pcm5102_tx_handle,
        data,
        bytes,
        written,
        wait
    );

#endif
}


/* ============================================================
 * PLAYBACK TASK
 * ============================================================ */

static void playback_task(void *arg)
{
    (void)arg;

    /*
     * ORIGINAL playback-buffer allocation.
     */
    int16_t *pcm =
        (int16_t *)heap_caps_malloc(
            (size_t)(FRAME_SAMPLES + 1) *
                2 *
                sizeof(int16_t),
            MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
        );

    int16_t *resample_buf =
        (int16_t *)heap_caps_malloc(
            MAX_RESAMPLE_FRAMES *
                2 *
                sizeof(int16_t),
            MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
        );

    if (!pcm || !resample_buf) {

        ESP_LOGE(
            TAG,
            "Failed to allocate playback buffers pcm=%p resample=%p",
            pcm,
            resample_buf
        );

        if (pcm) {
            free(pcm);
        }

        if (resample_buf) {
            free(resample_buf);
        }

        playback_task_handle = NULL;

        vTaskDelete(NULL);

        return;
    }


    ESP_LOGI(
        TAG,
        "Playback buffers allocated in PSRAM"
    );


    bool output_started = false;
    bool startup_phase = true;

    size_t written = 0;

    uint32_t underrun_count = 0;
    uint32_t zero_reads = 0;
    uint32_t last_log_ms = 0;

    static int16_t silence[
        FRAME_SAMPLES * 2
    ] = {0};


    while (playback_running) {

        if (resample_reinit_needed) {

            resample_reinit_needed =
                false;

            audio_resample_init(
                (uint32_t)source_rate,
                OUTPUT_RATE,
                2
            );

            audio_resample_reset();

            ESP_LOGI(
                TAG,
                "Resampler reinit source=%d output=%d",
                source_rate,
                OUTPUT_RATE
            );
        }


        if (flush_requested) {

            flush_requested = false;

            output_started = false;
            startup_phase = true;

            zero_reads = 0;

            audio_resample_reset();

            written = 0;

            write_selected_outputs(
                silence,
                sizeof(silence),
                &written,
                pdMS_TO_TICKS(20)
            );

            ESP_LOGI(
                TAG,
                "Playback flush handled"
            );

            vTaskDelay(
                pdMS_TO_TICKS(20)
            );

            continue;
        }


        int queued =
            audio_receiver_buffered_frames();


        uint32_t now_ms =
            millis();

        if (now_ms -
            last_log_ms >= 3000)
        {
            last_log_ms =
                now_ms;
        }


        const int start_threshold =
            AUDIO_START_BUFFER_FRAMES;

        /*
         * Only gate the initial cold start.
         *
         * AAC-LC arrives as 1024 PCM frames and audio_buffer_queue_decoded()
         * splits that into 352 + 352 + 320 frame ring slots.  Therefore the
         * raw ring count naturally falls to 1 between incoming AAC packets.
         *
         * Treating queued < 2 as an underrun was wrong: it stopped the
         * consumer while valid audio was still queued, then required an
         * artificial 8-slot recovery threshold.  That produced exactly the
         * play / stop / brief play behaviour seen with buffered AirPlay 2.
         *
         * Once playback has started, let audio_receiver_read() decide whether
         * data is actually available.  A zero read is transient and must not
         * reset the output state.
         */
        if (!output_started) {

            if (queued <
                start_threshold)
            {
                if ((now_ms - last_log_ms) >= 1000U) {
                    last_log_ms = now_ms;
                    Serial.printf(
                        "[AUDIO] wait queued=%d need=%d phase=startup\n",
                        queued,
                        start_threshold
                    );
                }

                vTaskDelay(
                    pdMS_TO_TICKS(2)
                );

                continue;
            }

            output_started = true;
            startup_phase = false;
            zero_reads = 0;
        }


        size_t samples =
            audio_receiver_read(
                pcm,
                FRAME_SAMPLES + 1
            );


        if (samples == 0) {

            zero_reads++;

            /*
             * A temporary empty read is the real underrun indication.
             * Do not drop back into a queue-count recovery gate; simply let
             * the receiver refill and retry on the next scheduler tick.
             */
            if (zero_reads == 1 || (zero_reads % 500) == 0) {
                underrun_count++;
                Serial.printf(
                    "[AUDIO] receiver empty queued=%d zero_reads=%lu underruns=%lu\n",
                    audio_receiver_buffered_frames(),
                    (unsigned long)zero_reads,
                    (unsigned long)underrun_count
                );
            }

            vTaskDelay(
                pdMS_TO_TICKS(1)
            );

            continue;
        }


        zero_reads = 0;


        if (samples >
            FRAME_SAMPLES)
        {
            samples =
                FRAME_SAMPLES;
        }


        int16_t *play_buf =
            pcm;

        size_t play_samples =
            samples;


        if (source_rate !=
            OUTPUT_RATE)
        {
            if (!audio_resample_is_active()) {

                audio_resample_init(
                    (uint32_t)source_rate,
                    OUTPUT_RATE,
                    2
                );
            }


            play_samples =
                audio_resample_process(
                    pcm,
                    samples,
                    resample_buf,
                    MAX_RESAMPLE_FRAMES
                );


            play_buf =
                resample_buf;


            if (play_samples == 0) {

                vTaskDelay(
                    pdMS_TO_TICKS(1)
                );

                continue;
            }


            if (play_samples >
                MAX_RESAMPLE_FRAMES)
            {
                play_samples =
                    MAX_RESAMPLE_FRAMES;
            }
        }


        /*
         * IMPORTANT:
         * Keep original working behaviour.
         *
         * Do NOT call apply_volume() here.
         */


        led_audio_feed(
            play_buf,
            play_samples
        );


        const size_t bytes_to_write =
            play_samples *
            2 *
            sizeof(int16_t);


        written = 0;


        esp_err_t err =
            write_selected_outputs(
                play_buf,
                bytes_to_write,
                &written,
                pdMS_TO_TICKS(100)
            );


        if (err != ESP_OK ||
            written != bytes_to_write)
        {
            ESP_LOGW(
                TAG,
                "I2S write issue err=%d written=%u wanted=%u samples=%u",
                err,
                (unsigned)written,
                (unsigned)bytes_to_write,
                (unsigned)play_samples
            );

            vTaskDelay(
                pdMS_TO_TICKS(2)
            );

        } else {

            /*
             * i2s_channel_write() is the playback pacing.
             *
             * The selected primary I2S channel is written with a blocking
             * timeout above.  Do not add another full PCM-frame delay here:
             * doing so double-paces the stream and can starve the receiver.
             */
        }


        taskYIELD();
    }


    audio_resample_reset();


    if (pcm) {
        free(pcm);
    }

    if (resample_buf) {
        free(resample_buf);
    }


    playback_task_handle =
        NULL;


    vTaskDelete(NULL);
}


/* ============================================================
 * START
 * ============================================================ */

void audio_output_start(void)
{
    if (playback_task_handle != NULL) {
        return;
    }

    playback_running =
        true;

    xTaskCreatePinnedToCore(
        playback_task,
        "audio_play",
        4096,
        NULL,
        15,
        &playback_task_handle,
        PLAYBACK_CORE
    );
}


/* ============================================================
 * STOP
 * ============================================================ */

void audio_output_stop(void)
{
    if (playback_task_handle == NULL) {
        return;
    }

    playback_running =
        false;

    int timeout = 40;

    while (
        playback_task_handle != NULL &&
        timeout-- > 0)
    {
        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }

    if (playback_task_handle != NULL) {

        ESP_LOGW(
            TAG,
            "Playback task did not exit within timeout"
        );

    } else {

        ESP_LOGI(
            TAG,
            "Playback task stopped"
        );
    }
}


/* ============================================================
 * DIRECT WRITE
 * ============================================================ */

esp_err_t audio_output_write(
    const void *data,
    size_t bytes,
    TickType_t wait)
{
    size_t written = 0;

    return write_selected_outputs(
        data,
        bytes,
        &written,
        wait
    );
}


/* ============================================================
 * OUTPUT SAMPLE RATE
 * ============================================================ */

void audio_output_set_sample_rate(uint32_t rate)
{
    Serial.printf(
        "[AUDIO] requested=%lu forcing=44100\n",
        (unsigned long)rate
    );

    rate = 44100;

#if AUDIO_DAC == AUDIO_DAC_ES8311 || AUDIO_DAC == AUDIO_DAC_BOTH
    i2s_channel_disable(
        es8311_tx_handle
    );

    i2s_std_clk_config_t clk_cfg_es =
        I2S_STD_CLK_DEFAULT_CONFIG(
            44100
        );

    clk_cfg_es.mclk_multiple =
        I2S_MCLK_MULTIPLE_256;

    i2s_channel_reconfig_std_clock(
        es8311_tx_handle,
        &clk_cfg_es
    );

    es8311_set_sample_rate(
        44100
    );

    i2s_channel_enable(
        es8311_tx_handle
    );
#endif

#if AUDIO_DAC == AUDIO_DAC_PCM5102 || AUDIO_DAC == AUDIO_DAC_BOTH
    i2s_channel_disable(
        pcm5102_tx_handle
    );

    i2s_std_clk_config_t clk_cfg_pcm =
        I2S_STD_CLK_DEFAULT_CONFIG(
            44100
        );

    i2s_channel_reconfig_std_clock(
        pcm5102_tx_handle,
        &clk_cfg_pcm
    );

    i2s_channel_enable(
        pcm5102_tx_handle
    );
#endif
}


/* ============================================================
 * OUTPUT / ES8311 VOLUME STATUS
 * ============================================================ */

bool audio_output_es8311_enabled(void)
{
#if AUDIO_DAC == AUDIO_DAC_ES8311 || AUDIO_DAC == AUDIO_DAC_BOTH
    return true;
#else
    return false;
#endif
}

bool audio_output_pcm5102_enabled(void)
{
#if AUDIO_DAC == AUDIO_DAC_PCM5102 || AUDIO_DAC == AUDIO_DAC_BOTH
    return true;
#else
    return false;
#endif
}

uint8_t audio_output_get_es8311_volume_percent(void)
{
    return g_es8311_volume_percent;
}

bool audio_output_set_es8311_volume_percent(uint8_t percent)
{
#if AUDIO_DAC == AUDIO_DAC_ES8311 || AUDIO_DAC == AUDIO_DAC_BOTH
    return es8311_set_volume_percent_internal(percent);
#else
    (void)percent;
    return false;
#endif
}


/* ============================================================
 * FLUSH
 * ============================================================ */

void audio_output_flush(void)
{
    flush_requested =
        true;
}


/* ============================================================
 * SOURCE RATE
 * ============================================================ */

void audio_output_set_source_rate(int rate)
{
    if (rate > 0 &&
        rate != source_rate)
    {
        source_rate =
            rate;

        resample_reinit_needed =
            true;
    }
}