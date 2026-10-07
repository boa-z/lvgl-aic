/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
/* Exercises the production decoder with the real SDK MPP ABI and a fake engine.
 * This proves allocation/cleanup contracts, not hardware decode correctness. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../image/mpp/lv_aic_mpp_decoder.c"

static size_t live_cma;
static int fail_alloc, fail_decode, fail_next_alloc, decodes;
static struct { void *ptr; size_t size; } allocations[64];
static int request_stride = 2448, request_height = 480;
struct mpp_decoder {
    struct frame_allocator *allocator;
    struct decode_config config;
    struct mpp_frame frame;
    void *packet;
};
void *aicos_malloc_align(unsigned int type, size_t size, size_t align) {
    (void)type; (void)align;
    if (fail_alloc) return NULL;
    if (fail_next_alloc > 0) { fail_next_alloc--; return NULL; }
    void *p = calloc(1,size);
    if (p) {
        unsigned i;
        for (i=0; i<64 && allocations[i].ptr; i++) {}
        assert(i<64); allocations[i].ptr=p; allocations[i].size=size; live_cma+=size;
    }
    return p;
}
void aicos_free_align(unsigned int type, void *ptr) {
    (void)type;
    unsigned i;
    for (i=0; i<64 && allocations[i].ptr!=ptr; i++) {}
    assert(i<64); live_cma-=allocations[i].size; allocations[i].ptr=NULL; free(ptr);
}
void aicos_dcache_clean_invalid_range(unsigned long *p, unsigned long n) {(void)p;(void)n;}
void aicos_dcache_invalid_range(unsigned long *p, unsigned long n) {(void)p;(void)n;}
struct mpp_decoder *mpp_decoder_create(enum mpp_codec_type t) {(void)t; return calloc(1, sizeof(struct mpp_decoder));}
void mpp_decoder_destory(struct mpp_decoder *d) {free(d->packet); free(d);}
int mpp_decoder_control(struct mpp_decoder *d, int cmd, void *p) {(void)cmd; d->allocator=p; return 0;}
int mpp_decoder_init(struct mpp_decoder *d, struct decode_config *c) {d->config=*c; return 0;}
int mpp_decoder_get_packet(struct mpp_decoder *d, struct mpp_packet *p, int size) {
    d->packet=malloc((size_t)size); p->data=d->packet; return p->data ? 0 : -1;
}
int mpp_decoder_put_packet(struct mpp_decoder *d, struct mpp_packet *p) {(void)d;(void)p;return 0;}
int mpp_decoder_decode(struct mpp_decoder *d) {
    lv_aic_mpp_session_t *session = ((lv_aic_mpp_ext_allocator_t *)d->allocator)->session;
    int stride = request_stride ? request_stride : (int)((session->width *
                 lv_aic_mpp_format_lvgl_bpp(session->color_format) + 15) & ~15U);
    int height = request_height ? request_height : (int)session->height;
    d->frame.buf.size.width=request_stride ? 816 : session->width;
    d->frame.buf.size.height=height;
    decodes++;
    int result=d->allocator->ops->alloc_frame_buffer(d->allocator, &d->frame,
        stride, height, d->config.pix_fmt);
    if (result) return result;
    assert(d->frame.buf.stride[0] == (unsigned int)stride);
    assert(d->frame.buf.phy_addr[0] != 0);
    assert(live_cma >= (size_t)stride * height);
    memset(session->allocation_base,128,session->cma_size);
    return fail_decode ? -1 : 0;
}
int mpp_decoder_get_frame(struct mpp_decoder *d, struct mpp_frame *f) {*f=d->frame;return 0;}
int mpp_decoder_put_frame(struct mpp_decoder *d, struct mpp_frame *f) {(void)d;(void)f;return 0;}

