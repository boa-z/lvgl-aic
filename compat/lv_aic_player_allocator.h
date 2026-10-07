/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_ALLOCATOR_H
#define LV_AIC_PLAYER_ALLOCATOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <frame_allocator.h>
typedef struct lv_aic_player_allocator lv_aic_player_allocator_t;
/* Owns up to 32 CMA frames, matching the SDK frame-manager limit. Budget is
 * total live allocation bytes, including cache-line tails and retired pins.
 * Decoder callbacks and acquire/release are mutex protected. Destroy only
 * after the SDK has stopped using the allocator; never from a UI callback. */
lv_aic_player_allocator_t *lv_aic_player_allocator_create(size_t budget);
struct frame_allocator *lv_aic_player_allocator_sdk(lv_aic_player_allocator_t *a);
bool lv_aic_player_allocator_destroy(lv_aic_player_allocator_t *a);
/* Pin allocator-owned planes with a validated bounded frame layout before using returned capacities. One
 * pin per frame; requires a live SDK frame lease. Release the pin before SDK
 * put_frame, after all CPU/GE readers finish. SDK free while pinned retires
 * the allocation, preventing premature CMA release. Outputs transactional. */
bool lv_aic_player_allocator_acquire(lv_aic_player_allocator_t *a,
    const struct mpp_frame *frame, size_t capacities[3], uint64_t *ticket);
bool lv_aic_player_allocator_release(lv_aic_player_allocator_t *a, uint64_t ticket);
#endif
