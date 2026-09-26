/* Exercises the production decoder with the real SDK MPP ABI and a fake engine.
 * This proves allocation/cleanup contracts, not hardware decode correctness. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../image/mpp/lv_aic_mpp_decoder.c"

static size_t live_cma;
static int fail_alloc, fail_decode;
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
    void *p = malloc(size);
    if (p) live_cma += size;
    return p;
}
void aicos_free_align(unsigned int type, void *ptr) {
    (void)type; free(ptr); live_cma = 0;
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
    d->frame.buf.size.width=816; d->frame.buf.size.height=request_height;
    int result=d->allocator->ops->alloc_frame_buffer(d->allocator, &d->frame,
        request_stride, request_height, d->config.pix_fmt);
    if (result) return result;
    assert(d->frame.buf.size.width == 816);
    assert(d->frame.buf.stride[0] == (unsigned int)request_stride);
    assert(d->frame.buf.phy_addr[0] != 0);
    assert(live_cma >= (size_t)request_stride * request_height);
    return fail_decode ? -1 : 0;
}
int mpp_decoder_get_frame(struct mpp_decoder *d, struct mpp_frame *f) {*f=d->frame;return 0;}
int mpp_decoder_put_frame(struct mpp_decoder *d, struct mpp_frame *f) {(void)d;(void)f;return 0;}
int main(int argc, char **argv) {
    assert(argc == 2);
    lv_init();
    char path[1024];
    const struct { const char *name; bool accepted; } cases[] = {
        {"aic_801x479.jpg", true}, {"basn2c08.png", true},
        {"basn6a08.png", true}, {"s33n3p04.png", true},
        {"testimgp.jpg", false}, {"testimgari.jpg", false},
        {"empty.jpg", false}, {"empty.png", false}, {"basn0g08.png", false}
    };
    for (unsigned i=0; i<sizeof(cases)/sizeof(cases[0]); i++) {
        snprintf(path,sizeof(path),"L:%s/%s",argv[1],cases[i].name);
        lv_image_decoder_dsc_t dsc={0};
        lv_image_header_t header={0};
        dsc.src=path; dsc.src_type=LV_IMAGE_SRC_FILE;
        assert((lv_aic_mpp_info_cb(NULL,&dsc,&header)==LV_RESULT_OK)==cases[i].accepted);
    }
    snprintf(path,sizeof(path),"L:%s/aic_801x479.jpg",argv[1]);
    for (int mode=0; mode<4; mode++) {
        fail_alloc=mode==1; fail_decode=mode==2;
        request_stride=mode==3 ? 32 : 2448;
        lv_image_decoder_dsc_t dsc={0};
        lv_aic_mpp_session_t *session=NULL;
        lv_result_t result=lv_aic_mpp_decode_file(path, MPP_CODEC_VIDEO_DECODER_MJPEG,
            MPP_FMT_RGB_888, LV_COLOR_FORMAT_RGB888, 801, 479, false, &dsc, &session);
        if (mode==0) {
            assert(result==LV_RESULT_OK);
            assert(dsc.decoded->header.stride==2448);
            assert(dsc.decoded->header.w==801 && dsc.decoded->header.h==479);
            assert(session->cma_size==2448U*480U);
            lv_aic_mpp_close_cb(NULL,&dsc);
        } else assert(result==LV_RESULT_INVALID);
        assert(live_cma==0);
    }

    /* PNG chunk CRC gate. The SDK MPP PNG decoder skips every chunk CRC and the
     * zlib Adler-32, so a file with corrupt-but-still-inflatable IDAT would
     * otherwise open successfully. The wrapper must reject it before the bytes
     * reach the engine. Exercised against the real fixtures, no engine needed. */
    {
        const struct { const char *name; bool valid; } crc_cases[] = {
            {"basn2c08.png", true}, {"basn6a08.png", true}, {"basn3p08.png", true},
            {"z09n2c08.png", true}, {"basi2c08.png", true},
            {"s33n3p04.png", true}, {"s37n3p04.png", true},
            {"bad_crc_rgb.png", false}, {"xcsn0g01.png", false},
            {"xhdn0g08.png", false}
        };
        unsigned char buf[64*1024];
        for (unsigned i=0; i<sizeof(crc_cases)/sizeof(crc_cases[0]); i++) {
            snprintf(path,sizeof(path),"L:%s/%s",argv[1],crc_cases[i].name);
            FILE *f=fopen(path+2,"rb");
            assert(f);
            size_t n=fread(buf,1,sizeof(buf),f);
            fclose(f);
            assert(n>0 && n<sizeof(buf));
            bool got=lv_aic_mpp_png_chunks_valid(buf,(uint32_t)n);
            if (got!=crc_cases[i].valid)
                printf("CRC verdict mismatch %s expected=%d got=%d\n",
                       crc_cases[i].name,crc_cases[i].valid,got);
            assert(got==crc_cases[i].valid);
            if (crc_cases[i].valid) {
                /* Leave bytes beyond the declared length intact: accepting any
                 * prefix proves an over-read even without an address sanitizer. */
                for (uint32_t length=0; length<(uint32_t)n; length++)
                    assert(!lv_aic_mpp_png_chunks_valid(buf, length));
                buf[0] ^= 1;
                assert(!lv_aic_mpp_png_chunks_valid(buf, (uint32_t)n));
            }
        }
    }

    lv_deinit();
    puts("PASS: MPP allocator ABI, padded JPEG stride/height, failure cleanup, PNG CRC gate");
    return 0;
}
