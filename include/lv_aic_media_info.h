/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_MEDIA_INFO_H
#define LV_AIC_MEDIA_INFO_H
#include <stdint.h>
/* SDK-neutral spelling of the current av_media_info value layout. Size/offsets
 * are checked against real SDK headers by the media playback adapter. Zero
 * duration/file_size means unknown (or empty), as in the SDK. APNG reports
 * dimensions/file bytes, no audio, duration=0 and general seek_able=0; zero-time
 * replay is still supported. All fields are copied as one coherent snapshot. */
typedef struct {
    int64_t file_size,duration;
    uint8_t has_video,has_audio,seek_able;
    struct { int32_t width,height; } video_stream;
    struct { int32_t nb_channel,bits_per_sample,sample_rate; } audio_stream;
} lv_aic_media_info_t;
#endif
