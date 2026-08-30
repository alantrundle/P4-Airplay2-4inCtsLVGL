#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "audio_timing.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "ntp_clock.h"
#include "ptp_clock.h"

#define DEFAULT_BUFFER_LATENCY_US     200000 // 200ms startup jitter buffer
#define HARDWARE_OUTPUT_LATENCY_US    46000  // ~46ms I2S DMA latency
#define MIN_STARTUP_FRAMES            4
#define DRIFT_ADJUST_THRESHOLD_FRAMES 2
#define TIMING_THRESHOLD_US           15000 // 40ms early/late threshold
// If a frame is late by more than this, flush the whole buffer at once
// instead of draining one frame per DMA callback (which would cause seconds
// of silence while thousands of stale frames are individually dropped).
// Kept independent of DEFAULT_BUFFER_LATENCY_US so reducing the startup
// buffer doesn't also reduce the late-detection threshold.
#define BULK_FLUSH_LATE_THRESHOLD_US 2000000 // 2 seconds
// MAX_CONSECUTIVE_EARLY: number of consecutive early frames before we conclude
// the anchor is genuinely stuck/wrong.  At ~8 ms per frame this is ~6 seconds
// of silence, which is well beyond any normal pre-buffer depth.
#define MAX_CONSECUTIVE_EARLY 100
// MAX_CONSECUTIVE_LATE: number of consecutive individually-late frames before
// we conclude the whole buffer is stale and do a bulk flush.  At ~8 ms/frame
// this is ~24 ms — just enough to distinguish a genuine stale-buffer from a
// one-off WiFi jitter spike, without the 20-frame drain+log storm.
#define MAX_CONSECUTIVE_LATE 200

// POST_FLUSH_STALE_THRESHOLD_US: in post_flush mode the bypass plays frames
// unconditionally to avoid silence during the phone's pre-buffer window
// (typically 2–4 s).  Frames that are MORE than this many µs early are from
// the wrong seek position (old audio still draining through the TCP pipeline)
// and must be discarded rather than played.  10 s is well above the deepest
// observed AirPlay 2 pre-buffer depth and well below any real seek delta.
#define POST_FLUSH_STALE_THRESHOLD_US 10000000LL // 10 seconds
// POST_FLUSH_TIMEOUT_US: maximum duration of the post_flush bypass.  After a
// seek/flush the phone's pre-buffer window causes frames to appear hundreds of
// ms early.  We play them immediately for this duration so the user hears audio
// right away, then revert to normal timing so the anchor can enforce A/V sync.
#define POST_FLUSH_TIMEOUT_US 500000LL // 500 ms

static const char *TAG = "audio_time";
// consecutive_early_frames is now a field in audio_timing_t so it resets
// automatically whenever a new anchor is set.

static uint32_t frame_samples_from_format(const audio_format_t *format) {
  if (format->frame_size > 0) {
    return (uint32_t)format->frame_size;
  }
  if (format->max_samples_per_frame > 0) {
    return format->max_samples_per_frame;
  }
  return AAC_FRAMES_PER_PACKET;
}

static void update_timing_targets(audio_timing_t *timing,
                                  const audio_format_t *format) {
  timing->nominal_frame_samples = frame_samples_from_format(format);

  if (format->sample_rate <= 0 || timing->nominal_frame_samples == 0) {
    timing->target_buffer_frames = MIN_STARTUP_FRAMES;
    return;
  }

  uint64_t latency_samples =
      ((uint64_t)timing->output_latency_us * (uint64_t)format->sample_rate) /
      1000000ULL;
  uint32_t target_frames =
      (uint32_t)((latency_samples + timing->nominal_frame_samples - 1) /
                 timing->nominal_frame_samples);
  if (target_frames < MIN_STARTUP_FRAMES) {
    target_frames = MIN_STARTUP_FRAMES;
  }
  timing->target_buffer_frames = target_frames;
}

typedef enum {
  SYNC_MODE_NONE, // No clock sync, use local anchor time
  SYNC_MODE_PTP,  // AirPlay 2 PTP sync
  SYNC_MODE_NTP,  // AirPlay 1 NTP sync
} sync_mode_t;

// Compute how early (positive) or late (negative) a frame is in microseconds
static bool compute_early_us(const audio_timing_t *timing,
                             const audio_format_t *format,
                             uint32_t rtp_timestamp, sync_mode_t sync_mode,
                             int64_t *early_us) {
  if (!timing->anchor_valid || format->sample_rate <= 0) {
    return false;
  }

  int32_t rtp_delta = (int32_t)(rtp_timestamp - timing->anchor_rtp_time);
  int64_t frame_offset_ns =
      ((int64_t)rtp_delta * 1000000000LL) / format->sample_rate;

  int64_t target_ns;
  switch (sync_mode) {
  case SYNC_MODE_PTP:
    // AirPlay 2: use network time with PTP offset for multi-room sync
    target_ns = (int64_t)timing->anchor_network_time_ns -
                ptp_clock_get_offset_ns() + frame_offset_ns;
    break;
  case SYNC_MODE_NTP:
    // AirPlay 1: use network time with NTP offset for multi-room sync
    // offset = remote_time - local_time, so local = remote - offset
    target_ns = (int64_t)timing->anchor_network_time_ns -
                ntp_clock_get_offset_ns() + frame_offset_ns;
    break;
  default:
    // Fallback: use local anchor time (no multi-room sync)
    target_ns = timing->anchor_local_time_ns + frame_offset_ns;
    break;
  }

  // Subtract hardware latency to account for I2S DMA delay
  target_ns -= (int64_t)HARDWARE_OUTPUT_LATENCY_US * 1000LL;

  int64_t now_ns = (int64_t)esp_timer_get_time() * 1000LL;
  *early_us = (target_ns - now_ns) / 1000LL;

  return true;
}

void audio_timing_init(audio_timing_t *timing, size_t pending_capacity) {
  if (!timing) {
    return;
  }

  memset(timing, 0, sizeof(*timing));
  timing->output_latency_us = DEFAULT_BUFFER_LATENCY_US;
  timing->playing = true;

  if (pending_capacity > 0) {
    timing->pending_frame = (uint8_t *)malloc(pending_capacity);
    if (timing->pending_frame) {
      timing->pending_frame_capacity = pending_capacity;
    }
  }
}

void audio_timing_reset(audio_timing_t *timing) {
  if (!timing) {
    return;
  }

  timing->playout_started = false;
  timing->anchor_valid = false;
  timing->pending_valid = false;
  timing->pending_frame_len = 0;
  timing->ready_time_us = 0;
  timing->consecutive_early_frames = 0;
  timing->consecutive_late_frames = 0;
  timing->post_flush = false;
  timing->post_flush_start_us = 0;
  timing->deferred_flush_pending = false;
  timing->flush_until_ts = 0;
}

void audio_timing_set_format(audio_timing_t *timing,
                             const audio_format_t *format) {
  if (!timing || !format) {
    return;
  }

  update_timing_targets(timing, format);
}

void audio_timing_set_output_latency(audio_timing_t *timing,
                                     const audio_format_t *format,
                                     uint32_t latency_us) {
  if (!timing || !format) {
    return;
  }

  timing->output_latency_us = latency_us;
  update_timing_targets(timing, format);
}

uint32_t audio_timing_get_output_latency(const audio_timing_t *timing) {
  if (!timing) {
    return 0;
  }

  return timing->output_latency_us;
}

uint32_t audio_timing_get_hardware_latency(void) {
  return HARDWARE_OUTPUT_LATENCY_US;
}

void audio_timing_set_anchor(audio_timing_t *timing,
                             const audio_format_t *format, uint64_t clock_id,
                             uint64_t network_time_ns, uint32_t rtp_time) {
  if (!timing || !format) {
    return;
  }

  (void)clock_id;

  int64_t now_ns = (int64_t)esp_timer_get_time() * 1000LL;

  timing->anchor_rtp_time = rtp_time;
  timing->anchor_network_time_ns = network_time_ns;
  timing->anchor_local_time_ns = now_ns;
  timing->ptp_locked = ptp_clock_is_locked();
  timing->anchor_valid = true;
  // Reset frame counters so pre-buffered audio after a pause/resume or
  // track skip does not accumulate into the new anchor's counts.
  timing->consecutive_early_frames = 0;
  timing->consecutive_late_frames = 0;
}

void audio_timing_set_playing(audio_timing_t *timing, bool playing) {
  if (!timing) {
    return;
  }

  ESP_LOGI(TAG, "set_playing: %s -> %s", timing->playing ? "playing" : "paused",
           playing ? "playing" : "paused");

  timing->playing = playing;
  if (!playing) {
    // Discard any partially-pending frame so resume starts cleanly from
    // the oldest frame in the sorted buffer.
    timing->pending_valid = false;
    timing->pending_frame_len = 0;
  }
}

size_t audio_timing_read(audio_timing_t *timing,
                         audio_buffer_t *buffer,
                         const audio_stream_t *stream,
                         audio_stats_t *stats,
                         int16_t *out,
                         size_t samples)
{
  if (!timing || !buffer || !stream || !out || samples == 0) {
    return 0;
  }

  if (!timing->playing) {
    return 0;
  }

  const audio_format_t *format = &stream->format;
  int buffered_frames = audio_buffer_get_frame_count(buffer);

  /*
   * Startup gate.
   *
   * Previous behaviour could hold hundreds of decoded frames while waiting
   * for anchor/timing to become perfect. That caused:
   *
   *   zero-read queued=200/300+
   *
   * Once we have a healthy startup buffer, allow playout to begin.
   */
  if (!timing->playout_started && !timing->pending_valid) {
    if (buffered_frames >= 128) {
      timing->playout_started = true;
      timing->ready_time_us = esp_timer_get_time();

      ESP_LOGI(TAG,
               "Startup bypass: buffered=%d anchor=%d",
               buffered_frames,
               timing->anchor_valid ? 1 : 0);
    } else {
      if (buffered_frames < (int)timing->target_buffer_frames) {
        return 0;
      }

      if (!timing->anchor_valid) {
        int64_t now_us = esp_timer_get_time();

        if (timing->ready_time_us == 0) {
          timing->ready_time_us = now_us;
        }

        if (now_us - timing->ready_time_us < 250000) {
          return 0;
        }

        timing->playout_started = true;

        ESP_LOGI(TAG,
                 "Startup no-anchor fallback: buffered=%d",
                 buffered_frames);
      }
    }
  }

  sync_mode_t sync_mode = SYNC_MODE_NONE;

  if (ptp_clock_is_locked()) {
    sync_mode = SYNC_MODE_PTP;
  } else if (ntp_clock_is_locked()) {
    sync_mode = SYNC_MODE_NTP;
  }

  for (int attempt = 0; attempt < 8; attempt++) {
    size_t item_size = 0;
    void *item = NULL;
    bool from_pending = false;

    if (timing->pending_valid) {
      item_size = timing->pending_frame_len;

      if (item_size < sizeof(audio_frame_header_t)) {
        timing->pending_valid = false;
        timing->pending_frame_len = 0;
        continue;
      }

      item = timing->pending_frame;
      from_pending = true;
    } else {
      if (!audio_buffer_take(buffer, &item, &item_size, 0)) {
        if (stats) {
          stats->buffer_underruns++;
        }
        return 0;
      }

      buffered_frames = audio_buffer_get_frame_count(buffer);

      if (item_size < sizeof(audio_frame_header_t)) {
        audio_buffer_return(buffer, item);
        continue;
      }
    }

    audio_frame_header_t *hdr = (audio_frame_header_t *)item;
    size_t frame_samples = hdr->samples_per_channel;
    size_t channels = hdr->channels ? hdr->channels : format->channels;
    int16_t *pcm = (int16_t *)(hdr + 1);

    if (channels <= 0) {
      channels = 2;
    }

    if (frame_samples == 0 || channels == 0) {
      if (from_pending) {
        timing->pending_valid = false;
        timing->pending_frame_len = 0;
      } else {
        audio_buffer_return(buffer, item);
      }
      continue;
    }

    size_t expected_bytes =
        sizeof(*hdr) + frame_samples * channels * sizeof(int16_t);

    if (item_size < expected_bytes) {
      if (from_pending) {
        timing->pending_valid = false;
        timing->pending_frame_len = 0;
      } else {
        audio_buffer_return(buffer, item);
      }
      continue;
    }

    if (frame_samples > samples) {
      frame_samples = samples;
    }

    /*
     * Deferred FLUSHBUFFERED handling.
     */
    if (timing->deferred_flush_pending) {
      if ((int32_t)(hdr->rtp_timestamp - timing->flush_until_ts) >= 0) {
        ESP_LOGI(TAG,
                 "Deferred flush triggered at ts=%" PRIu32
                 " until=%" PRIu32,
                 hdr->rtp_timestamp,
                 timing->flush_until_ts);

        if (from_pending) {
          timing->pending_valid = false;
          timing->pending_frame_len = 0;
        } else {
          audio_buffer_return(buffer, item);
        }

        audio_buffer_flush(buffer);

        timing->deferred_flush_pending = false;
        timing->playout_started = false;
        timing->ready_time_us = 0;
        timing->consecutive_early_frames = 0;
        timing->consecutive_late_frames = 0;
        timing->post_flush = true;
        timing->post_flush_start_us = 0;

        return 0;
      }
    }

    /*
     * Timing gate.
     *
     * Important startup change:
     * Once playout_started is true, do not keep returning silence forever
     * just because the first anchor makes the frame look early. A healthy
     * buffer should be allowed to play.
     */
    if (timing->anchor_valid && format->sample_rate > 0) {
      int64_t early_us = 0;

      if (compute_early_us(timing,
                           format,
                           hdr->rtp_timestamp,
                           sync_mode,
                           &early_us)) {

        if (timing->post_flush) {
          if (timing->post_flush_start_us == 0) {
            timing->post_flush_start_us = esp_timer_get_time();
          }

          int64_t flush_elapsed =
              esp_timer_get_time() - timing->post_flush_start_us;

          if (early_us > POST_FLUSH_STALE_THRESHOLD_US) {
            ESP_LOGW(TAG,
                     "post_flush: bulk flush %d stale frames (%lld s early)",
                     audio_buffer_get_frame_count(buffer),
                     early_us / 1000000LL);

            if (from_pending) {
              timing->pending_valid = false;
              timing->pending_frame_len = 0;
            } else {
              audio_buffer_return(buffer, item);
            }

            audio_buffer_flush(buffer);

            timing->playout_started = false;
            timing->ready_time_us = 0;
            timing->consecutive_early_frames = 0;
            timing->consecutive_late_frames = 0;

            return 0;
          }

          if ((early_us >= -TIMING_THRESHOLD_US &&
               early_us <= TIMING_THRESHOLD_US) ||
              flush_elapsed >= POST_FLUSH_TIMEOUT_US) {
            ESP_LOGI(TAG,
                     "post_flush done: early=%lld ms elapsed=%lld ms",
                     early_us / 1000LL,
                     flush_elapsed / 1000LL);

            timing->post_flush = false;
            timing->post_flush_start_us = 0;
          }

          timing->consecutive_early_frames = 0;
          timing->consecutive_late_frames = 0;

          /*
           * Fall through and play.
           */
        } else if (early_us > TIMING_THRESHOLD_US) {
          timing->consecutive_early_frames++;

          /*
           * Startup/large-buffer bypass.
           *
           * If the decoded queue is already deep, playing silence makes the
           * startup stutter worse. Play the frame and allow the anchor to
           * settle instead.
           */
          if (buffered_frames >= 128) {
            static uint32_t early_bypass_log = 0;
            early_bypass_log++;

            if ((early_bypass_log % 100) == 1) {
              ESP_LOGI(TAG,
                       "Early bypass: early=%lld ms buffered=%d consecutive=%d",
                       early_us / 1000LL,
                       buffered_frames,
                       timing->consecutive_early_frames);
            }

            timing->consecutive_early_frames = 0;
            timing->consecutive_late_frames = 0;

            /*
             * Fall through and play.
             */
          } else if (timing->consecutive_early_frames > 100) {
            ESP_LOGW(TAG,
                     "Invalidating stuck anchor: consecutive=%d early=%lld ms",
                     timing->consecutive_early_frames,
                     early_us / 1000LL);

            timing->anchor_valid = false;
            timing->consecutive_early_frames = 0;

            /*
             * Fall through and play.
             */
          } else {
            /*
             * Normal early-frame path: keep one pending frame and return
             * silence. This is now only used while buffer depth is shallow.
             */
            if (!from_pending &&
                timing->pending_frame &&
                item_size <= timing->pending_frame_capacity) {
              memcpy(timing->pending_frame, item, item_size);
              timing->pending_frame_len = item_size;
              timing->pending_valid = true;
              audio_buffer_return(buffer, item);
            }

            memset(out, 0, samples * channels * sizeof(int16_t));
            return samples;
          }
        } else if (early_us < -TIMING_THRESHOLD_US) {
          timing->consecutive_early_frames = 0;
          timing->consecutive_late_frames++;

          if (-early_us > BULK_FLUSH_LATE_THRESHOLD_US) { 
            ESP_LOGW(TAG,
                     "Bulk flush: frame %lld ms late consecutive=%d flushing=%d",
                     -early_us / 1000LL,
                     timing->consecutive_late_frames,
                     audio_buffer_get_frame_count(buffer));

            if (from_pending) {
              timing->pending_valid = false;
              timing->pending_frame_len = 0;
            } else {
              audio_buffer_return(buffer, item);
            }

            audio_buffer_flush(buffer);

            timing->playout_started = false;
            timing->ready_time_us = 0;
            timing->consecutive_late_frames = 0;

            if (stats) {
              stats->late_frames++;
            }

            return 0;
          }

          if (timing->consecutive_late_frames == 1) {
            ESP_LOGW(TAG,
                     "Dropping late frame(s): %lld ms",
                     -early_us / 1000LL);
          }

          if (stats) {
            stats->late_frames++;
          }

          if (from_pending) {
            timing->pending_valid = false;
            timing->pending_frame_len = 0;
          } else {
            audio_buffer_return(buffer, item);
          }

          continue;
        }
      }
    }

    /*
     * Frame is accepted.
     */
    timing->consecutive_early_frames = 0;
    timing->consecutive_late_frames = 0;

    memcpy(out, pcm, frame_samples * channels * sizeof(int16_t));

    if (from_pending) {
      timing->pending_valid = false;
      timing->pending_frame_len = 0;
    } else {
      audio_buffer_return(buffer, item);
    }

    timing->playout_started = true;

    return frame_samples;
  }

  return 0;
}
