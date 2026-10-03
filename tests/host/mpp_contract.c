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

static unsigned char *fixture(const char *root, const char *name, uint32_t *length)
{
    char path[1024]; snprintf(path,sizeof(path),"%s/%s",root,name);
    FILE *f=fopen(path,"rb"); assert(f); assert(fseek(f,0,SEEK_END)==0);
    long n=ftell(f); assert(n>=0 && n<8*1024*1024); rewind(f);
    unsigned char *data=malloc(n ? (size_t)n : 1); assert(data);
    assert(fread(data,1,(size_t)n,f)==(size_t)n); fclose(f); *length=(uint32_t)n; return data;
}
static void open_mpp(lv_image_decoder_dsc_t *d, const void *src, const lv_image_decoder_args_t *args)
{
    assert(lv_image_decoder_open(d,src,args)==LV_RESULT_OK);
    assert(d->decoder==g_aic_mpp_decoder && d->decoded && d->user_data);
}
static void memory_cache_contract(const char *root)
{
    lv_image_decoder_t *decoder;
    lv_image_dsc_t image={0}, other;
    lv_image_decoder_dsc_t a,b,c;
    lv_image_decoder_args_t args={0};
    lv_image_header_t header;
    uint32_t n; unsigned char *data;
    const char *names[]={"aic_801x479.jpg","basn2c08.png","basn6a08.png","s33n3p04.png"};
    request_stride=request_height=0;
    assert(lv_aic_mpp_decoder_init(&decoder)==LV_AIC_OK);
    lv_aic_mpp_cache_set_limit(4*1024*1024);
    for(unsigned i=0;i<4;i++) {
        data=fixture(root,names[i],&n);
        image.header.magic=LV_IMAGE_HEADER_MAGIC; image.header.cf=LV_COLOR_FORMAT_RAW;
        image.header.w=1; image.header.h=1; image.data=data; image.data_size=n;
        assert(lv_image_decoder_get_info(&image,&header)==LV_RESULT_OK);
        assert(header.w>1 && header.h>1); /* encoded geometry wins */
        open_mpp(&a,&image,&args); int before=decodes;
        open_mpp(&b,&image,&args); assert(decodes==before && a.decoded==b.decoded);
        lv_image_decoder_close(&a); assert(live_cma>0);
        lv_aic_mpp_cache_drop(&image); assert(lv_aic_mpp_cache_stats()->entries==1);
        open_mpp(&c,&image,&args); assert(decodes==before+1 && b.decoded!=c.decoded);
        assert(lv_aic_mpp_cache_stats()->entries==2);
        lv_image_decoder_close(&b); assert(lv_aic_mpp_cache_stats()->entries==1);
        lv_image_decoder_close(&c); lv_aic_mpp_cache_drop(&image);
        assert(live_cma==0 && lv_aic_mpp_cache_stats()->bytes==0); free(data);
    }
    data=fixture(root,"basn6a08.png",&n); image.data=data; image.data_size=n;
    image.header.cf=LV_COLOR_FORMAT_RAW_ALPHA; other=image;
    open_mpp(&a,&image,&args); const void *cached=a.decoded;
    uint32_t cost=lv_aic_mpp_cache_stats()->bytes;
    lv_image_decoder_close(&a);
    args.no_cache=true; int before=decodes; open_mpp(&a,&image,&args);
    assert(decodes==before+1 && a.decoded!=cached && lv_aic_mpp_cache_stats()->entries==1);
    lv_image_decoder_close(&a); args.no_cache=false;
    args.premultiply=true; open_mpp(&a,&image,&args);
    assert(a.decoded!=cached && (a.decoded->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED));
    lv_image_decoder_close(&a); args.premultiply=false;
    args.stride_align=true; open_mpp(&a,&image,&args); assert(a.decoded!=cached);
    lv_image_decoder_close(&a); args.stride_align=false;
    args.use_indexed=true; open_mpp(&a,&image,&args); assert(a.decoded!=cached);
    lv_image_decoder_close(&a); args.use_indexed=false;
    args.flush_cache=true; before=decodes; open_mpp(&a,&image,&args);
    assert(a.decoded==cached && decodes==before); lv_image_decoder_close(&a); args.flush_cache=false;
    assert(lv_aic_mpp_cache_stats()->entries==4);
    lv_aic_mpp_cache_drop(NULL); assert(live_cma==0);

    /* One-entry byte budget, LRU eviction and pinned-reader preservation. */
    lv_aic_mpp_cache_set_limit(cost);
    open_mpp(&a,&image,&args); lv_image_decoder_close(&a);
    uint32_t evicted=lv_aic_mpp_cache_stats()->evictions;
    open_mpp(&b,&other,&args); assert(lv_aic_mpp_cache_stats()->evictions==evicted+1);
    lv_image_decoder_close(&b); open_mpp(&a,&image,&args);
    open_mpp(&b,&other,&args); assert(lv_aic_mpp_cache_stats()->entries==1);
    assert(a.decoded!=b.decoded); lv_image_decoder_close(&b);
    lv_aic_mpp_cache_set_limit(0); assert(lv_aic_mpp_cache_stats()->bytes==cost);
    lv_image_decoder_close(&a); assert(live_cma==0 && lv_aic_mpp_cache_stats()->bytes==0);
    before=decodes; open_mpp(&a,&image,&args); lv_image_decoder_close(&a);
    open_mpp(&a,&image,&args); lv_image_decoder_close(&a);
    assert(decodes==before+2 && lv_aic_mpp_cache_stats()->entries==0);

    lv_aic_mpp_cache_set_limit(1); /* Valid decode, too large to retain. */
    open_mpp(&a,&image,&args); assert(lv_aic_mpp_cache_stats()->entries==0);
    lv_image_decoder_close(&a); assert(live_cma==0);

    /* Cache population must not retain failed decodes. */
    lv_aic_mpp_cache_set_limit(4*1024*1024); fail_decode=1;
    assert(lv_image_decoder_open(&a,&image,&args)==LV_RESULT_INVALID);
    assert(live_cma==0 && lv_aic_mpp_cache_stats()->entries==0); fail_decode=0;
    fail_alloc=1; assert(lv_image_decoder_open(&a,&image,&args)==LV_RESULT_INVALID);
    assert(live_cma==0); fail_alloc=0;
    open_mpp(&a,&image,&args); lv_image_decoder_close(&a);
    evicted=lv_aic_mpp_cache_stats()->evictions; fail_next_alloc=1;
    open_mpp(&b,&other,&args); assert(lv_aic_mpp_cache_stats()->evictions==evicted+1);
    lv_image_decoder_close(&b);
    lv_aic_mpp_cma_stats_reset(); assert(lv_aic_mpp_cma_stats()->current_cma_bytes==live_cma);
    lv_aic_mpp_cache_drop(NULL); assert(live_cma==0 && lv_aic_mpp_cma_stats()->current_cma_bytes==0);

    /* Entry-count cap applies even when the byte budget is generous. */
    lv_image_dsc_t many[17];
    for(unsigned i=0;i<17;i++) {
        many[i]=image; open_mpp(&a,&many[i],&args); lv_image_decoder_close(&a);
    }
    assert(lv_aic_mpp_cache_stats()->entries==16);
    before=decodes; open_mpp(&a,&many[16],&args); lv_image_decoder_close(&a); assert(decodes==before);
    open_mpp(&a,&many[1],&args); lv_image_decoder_close(&a); assert(decodes==before);
    open_mpp(&a,&many[0],&args); lv_image_decoder_close(&a); assert(decodes==before+1);
    open_mpp(&a,&many[1],&args); lv_image_decoder_close(&a); assert(decodes==before+1);
    open_mpp(&a,&many[2],&args); lv_image_decoder_close(&a); assert(decodes==before+2);
    lv_aic_mpp_cache_drop(NULL); assert(live_cma==0);

    /* The stream never reads past the declared payload, including EOF seeks. */
    lv_aic_mpp_stream_t stream={0}; unsigned char bytes[4]={0}; uint32_t done=99;
    assert(lv_aic_mpp_stream_open_memory(&stream,data,2)==LV_FS_RES_OK);
    assert(lv_aic_mpp_stream_read(&stream,bytes,4,&done)==LV_FS_RES_OK && done==2);
    assert(lv_aic_mpp_stream_seek(&stream,3)==LV_FS_RES_INV_PARAM);
    assert(lv_aic_mpp_stream_read(&stream,bytes,1,&done)==LV_FS_RES_OK && done==0);
    lv_aic_mpp_stream_close(&stream);
    assert(lv_aic_mpp_stream_read(&stream,bytes,1,&done)==LV_FS_RES_INV_PARAM);
    lv_image_decoder_dsc_t direct={0}; direct.src=&image; direct.src_type=LV_IMAGE_SRC_VARIABLE;
    for(uint32_t size=0;size<32;size++) { image.data_size=size; assert(lv_aic_mpp_info_cb(NULL,&direct,&header)==LV_RESULT_INVALID); }
    image.data_size=8*1024*1024+1; assert(lv_aic_mpp_info_cb(NULL,&direct,&header)==LV_RESULT_INVALID);
    image.data_size=n; image.header.flags=LV_IMAGE_FLAGS_COMPRESSED;
    assert(lv_aic_mpp_info_cb(NULL,&direct,&header)==LV_RESULT_INVALID); image.header.flags=0;
    image.header.cf=LV_COLOR_FORMAT_RGB888; assert(lv_aic_mpp_info_cb(NULL,&direct,&header)==LV_RESULT_INVALID);
    image.header.cf=LV_COLOR_FORMAT_RAW_ALPHA;
    image.data=NULL; assert(lv_aic_mpp_info_cb(NULL,&direct,&header)==LV_RESULT_INVALID); image.data=data;

    /* CRC failure in a memory packet must be caught before engine decode. */
    uint32_t bad_n; unsigned char *bad=fixture(root,"bad_crc_rgb.png",&bad_n);
    other=image; other.data=bad; other.data_size=bad_n; before=decodes;
    assert(lv_image_decoder_open(&a,&other,&args)==LV_RESULT_INVALID);
    assert(decodes==before && live_cma==0); free(bad);

    /* File keys are owned string copies, not caller path addresses. */
    char path[1024], copy[1024]; snprintf(path,sizeof(path),"L:%s/basn2c08.png",root);
    strcpy(copy,path); open_mpp(&a,path,&args); lv_image_decoder_close(&a); before=decodes;
    path[0]='X'; open_mpp(&a,copy,&args); assert(decodes==before); lv_image_decoder_close(&a);
    lv_aic_mpp_cache_drop(copy); assert(live_cma==0);

    /* Teardown must not delete a decoder that still has readers. */
    open_mpp(&a,&image,&args); lv_aic_mpp_decoder_deinit(decoder);
    assert(g_aic_mpp_decoder==decoder); lv_image_decoder_close(&a);
    lv_aic_mpp_decoder_deinit(decoder);
    assert(g_aic_mpp_decoder==NULL && live_cma==0 && lv_aic_mpp_cache_stats()->entries==0);
    assert(lv_aic_mpp_decoder_init(&decoder)==LV_AIC_OK);
    open_mpp(&a,&image,&args); lv_image_decoder_close(&a); lv_aic_mpp_decoder_deinit(decoder);
    assert(live_cma==0); free(data);
}

static void jpeg_header_boundaries(void)
{
    uint8_t gray[] = {0xff, 0xd8, 0xff, 0xc0, 0, 11,
                     8, 0, 16, 0, 24, 1, 1, 0x11, 0};
    lv_aic_mpp_stream_t stream = {0};
    int w, h, components;
    assert(lv_aic_mpp_stream_open_memory(&stream, gray, sizeof(gray)) == LV_FS_RES_OK);
    assert(lv_aic_mpp_parse_jpeg_header(&stream, &w, &h, &components) == LV_RESULT_OK);
    assert(w == 24 && h == 16 && components == 1 && stream.cursor == sizeof(gray));
    lv_aic_mpp_stream_close(&stream);
    for (unsigned length = 6; length < sizeof(gray); length++) {
        assert(lv_aic_mpp_stream_open_memory(&stream, gray, length) == LV_FS_RES_OK);
        assert(lv_aic_mpp_parse_jpeg_header(&stream, &w, &h, &components) == LV_RESULT_INVALID);
        lv_aic_mpp_stream_close(&stream);
    }
    gray[5] = 17; /* RGB length with one component is malformed. */
    assert(lv_aic_mpp_stream_open_memory(&stream, gray, sizeof(gray)) == LV_FS_RES_OK);
    assert(lv_aic_mpp_parse_jpeg_header(&stream, &w, &h, &components) == LV_RESULT_INVALID);
    lv_aic_mpp_stream_close(&stream);
}

static void aicp_header_contract(void)
{
    uint8_t bytes[] = {'A','I','C','P',0xff,0xd8,0xff,0xc1,0,17,
                      8,0,16,0,24,3,1,0x11,0,2,0x11,0,3,0x11,0};
    lv_image_dsc_t image = {0};
    image.header.magic = LV_IMAGE_HEADER_MAGIC;
    image.header.cf = LV_COLOR_FORMAT_RAW;
    image.data = bytes; image.data_size = sizeof(bytes);
    lv_image_decoder_dsc_t dsc = {0};
    lv_image_header_t header = {0};
    dsc.src = &image;
#ifdef AIC_MPP_AICP_DEC_ENABLE
    assert(lv_aic_mpp_info_cb(NULL, &dsc, &header) == LV_RESULT_OK);
    assert(header.w == 24 && header.h == 16 && header.cf == LV_COLOR_FORMAT_RGB888);
    lv_aic_mpp_stream_t stream = {0};
    enum mpp_codec_type codec;
    assert(lv_aic_mpp_source_open(&image, &stream, &codec) == LV_RESULT_OK);
    assert(codec == MPP_CODEC_VIDEO_DECODER_AICP && stream.cursor == 0);
    assert(stream.size == sizeof(bytes)); /* Prefix stays in decoder packet. */
    lv_aic_mpp_stream_close(&stream);
    bytes[15] = 4; /* Invalid length for four components. */
    assert(lv_aic_mpp_info_cb(NULL, &dsc, &header) == LV_RESULT_INVALID);
    bytes[15] = 3;
    image.data_size--;
    assert(lv_aic_mpp_info_cb(NULL, &dsc, &header) == LV_RESULT_INVALID);
    image.data_size++;
    bytes[0] = 'X';
#endif
    assert(lv_aic_mpp_info_cb(NULL, &dsc, &header) == LV_RESULT_INVALID);
}

int main(int argc, char **argv) {
    jpeg_header_boundaries();
    assert(argc == 2 || argc == 3);
    lv_init();
    aicp_header_contract();
    if (argc == 3) {
        assert(strcmp(argv[2],"--memory-cache") == 0);
        memory_cache_contract(argv[1]);
        lv_deinit();
        puts("PASS: memory sources, bounded cache, reference lifetime, invalidation and failure cleanup");
        return 0;
    }
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
        lv_result_t result=lv_aic_mpp_decode_source(path, MPP_CODEC_VIDEO_DECODER_MJPEG,
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

    /* CMA lifecycle counters. Modes 0 and 2 each allocate once and release once;
     * the failed-allocation and rejected-stride modes must not move the counters
     * at all, so the accounting stays symmetric. */
    {
        const lv_aic_mpp_cma_stats_t *cma = lv_aic_mpp_cma_stats();
        assert(cma->alloc_count == 2U);
        assert(cma->free_count == cma->alloc_count);
        assert(cma->current_cma_bytes == 0U);
        assert(cma->peak_cma_bytes == 2448U * 480U);
        lv_aic_mpp_cma_stats_reset();
        cma = lv_aic_mpp_cma_stats();
        assert(cma->alloc_count == 0U && cma->free_count == 0U);
        assert(cma->current_cma_bytes == 0U && cma->peak_cma_bytes == 0U);
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
    puts("PASS: MPP allocator ABI, padded JPEG stride/height, failure cleanup, CMA lifecycle counters, PNG CRC gate");
    return 0;
}
