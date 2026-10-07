#include "mpp_engine_fixture.h"

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
    /* 32 px RGBA rows (128 B) already satisfy LV_DRAW_BUF_STRIDE_ALIGN: the
     * SW unit's stride_align=true request shares the GE2D unit's raw buffer
     * instead of caching a second copy. */
    args.stride_align=true; before=decodes; open_mpp(&a,&image,&args);
    assert(a.decoded==cached && decodes==before);
    lv_image_decoder_close(&a); args.stride_align=false;
    args.use_indexed=true; open_mpp(&a,&image,&args); assert(a.decoded!=cached);
    lv_image_decoder_close(&a); args.use_indexed=false;
    args.flush_cache=true; before=decodes; open_mpp(&a,&image,&args);
    assert(a.decoded==cached && decodes==before); lv_image_decoder_close(&a); args.flush_cache=false;
    assert(lv_aic_mpp_cache_stats()->entries==3);
    {
        /* An aligned request reuses the raw buffer exactly when its actual
         * stride already equals the aligned stride; otherwise it must decode
         * a separate (post-processed, aligned) copy. */
        lv_image_dsc_t odd=image; uint32_t odd_n;
        unsigned char *odd_data=fixture(root,"s33n3p04.png",&odd_n);
        odd.data=odd_data; odd.data_size=odd_n; odd.header.cf=LV_COLOR_FORMAT_RAW;
        open_mpp(&a,&odd,&args); const void *raw=a.decoded;
        bool aligned=a.decoded->header.stride ==
            lv_draw_buf_width_to_stride(a.decoded->header.w, a.decoded->header.cf);
        lv_image_decoder_close(&a);
        args.stride_align=true; before=decodes; open_mpp(&b,&odd,&args);
        if(aligned) assert(decodes==before && b.decoded==raw);
        else assert(decodes==before+1 && b.decoded!=raw &&
                    b.decoded->header.stride ==
                        lv_draw_buf_width_to_stride(b.decoded->header.w, b.decoded->header.cf));
        lv_image_decoder_close(&b); args.stride_align=false;
        lv_aic_mpp_cache_drop(&odd); free(odd_data);
        assert(lv_aic_mpp_cache_stats()->entries==3);
    }
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

#ifdef AIC_MPP_AICP_DEC_ENABLE
static void aicp_fixture_contract(const char *root)
{
    const char *names[] = {"flower.aicp", "bird.aicp"};
    lv_image_decoder_t *decoder;
    request_stride = request_height = 0;
    assert(lv_aic_mpp_decoder_init(&decoder) == LV_AIC_OK);
    lv_aic_mpp_cache_set_limit(4 * 1024 * 1024);
    for (unsigned i = 0; i < 2; i++) {
        uint32_t size;
        uint8_t *bytes = fixture(root, names[i], &size);
        lv_image_dsc_t image = {0};
        image.header.magic = LV_IMAGE_HEADER_MAGIC;
        image.header.cf = LV_COLOR_FORMAT_RAW;
        image.data = bytes; image.data_size = size;
        char path[1024];
        snprintf(path, sizeof(path), "L:%s/%s", root, names[i]);
        for (int memory = 0; memory < 2; memory++) {
            const void *src = memory ? (const void *)&image : (const void *)path;
            lv_image_header_t header;
            lv_image_decoder_dsc_t a, b;
            lv_image_decoder_args_t args = {0};
#ifndef AIC_VE_DRV_V31
            if (i == 0) {
                lv_image_decoder_dsc_t rejected = {0};
                rejected.src = src;
                assert(lv_aic_mpp_info_cb(NULL, &rejected, &header) == LV_RESULT_INVALID);
                continue;
            }
#endif
            assert(lv_image_decoder_get_info(src, &header) == LV_RESULT_OK);
            assert(header.cf == (i == 0 ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_RGB888));
            assert(header.w > 0 && header.h > 0);
            open_mpp(&a, src, &args);
            int before = decodes;
            open_mpp(&b, src, &args);
            assert(decodes == before && a.decoded == b.decoded);
            lv_aic_mpp_cache_drop(src);
            lv_image_decoder_close(&a);
            assert(live_cma > 0);
            lv_image_decoder_close(&b);
            assert(live_cma == 0);
            fail_decode = 1;
            assert(lv_image_decoder_open(&a, src, &args) == LV_RESULT_INVALID);
            fail_decode = 0;
            assert(live_cma == 0);
        }
        free(bytes);
    }
    lv_aic_mpp_decoder_deinit(decoder);
}
#endif

static void bmp_put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

static void bmp_pixel_contract(void)
{
    lv_image_decoder_t *decoder;
    assert(lv_aic_mpp_decoder_init(&decoder) == LV_AIC_OK);
    lv_aic_mpp_cache_set_limit(4096);
    for (int mode = 0; mode < 3; mode++) {
        uint8_t bytes[78] = {'B', 'M'};
        bmp_put32(bytes + 10, 70); bmp_put32(bytes + 14, 40);
        bmp_put32(bytes + 18, 2); bmp_put32(bytes + 22, (uint32_t)-2);
        bytes[26] = 1; bytes[28] = 16;
        if (mode) {
            bmp_put32(bytes + 30, 3);
            bmp_put32(bytes + 54, mode == 1 ? 0xf800 : 0x7c00);
            bmp_put32(bytes + 58, mode == 1 ? 0x7e0 : 0x3e0);
            bmp_put32(bytes + 62, 31);
        }
        uint16_t input[] = {mode == 1 ? 0xf800 : 0x7c00,
                            mode == 1 ? 0x7e0 : 0x3e0, 31, 0xffff};
        uint16_t expected[] = {0xf800, 0x7e0, 31, 0xffff};
        for (unsigned i = 0; i < 4; i++) {
            bytes[70 + 2*i] = (uint8_t)input[i];
            bytes[71 + 2*i] = (uint8_t)(input[i] >> 8);
        }
        lv_image_dsc_t image = {0};
        image.header.magic = LV_IMAGE_HEADER_MAGIC; image.header.cf = LV_COLOR_FORMAT_RAW;
        image.data = bytes; image.data_size = sizeof(bytes);
        lv_image_decoder_dsc_t d;
        lv_image_decoder_args_t args = {0};
        args.no_cache = true;
        open_mpp(&d, &image, &args);
        assert(d.decoded->header.cf == LV_COLOR_FORMAT_RGB565);
        for (unsigned i = 0; i < 4; i++) {
            const uint8_t *p = d.decoded->data + (i / 2) * d.decoded->header.stride + (i % 2) * 2;
            assert(((uint16_t)p[0] | (uint16_t)p[1] << 8) == expected[i]);
        }
        lv_image_decoder_close(&d);
        assert(live_cma == 0);
    }
    for (int bpp = 24; bpp <= 32; bpp += 8) {
        for (int top = 0; top < 2; top++) {
            uint8_t bytes[94] = {'B', 'M'};
            bmp_put32(bytes + 10, 70); bmp_put32(bytes + 14, 40);
            bmp_put32(bytes + 18, 3); bmp_put32(bytes + 22, top ? (uint32_t)-2 : 2);
            bytes[26] = 1; bytes[28] = bpp;
            uint8_t expected[2][12] = {{0}};
            unsigned row_bytes = 3 * bpp / 8;
            for (unsigned y = 0; y < 2; y++) {
                for (unsigned x = 0; x < row_bytes; x++)
                    expected[y][x] = (uint8_t)(20 + y * 70 + x);
                memcpy(bytes + 70 + (top ? y : 1 - y) * 12, expected[y], row_bytes);
            }
            const char *filename = "bmp-contract.BMP";
            FILE *f = fopen(filename, "wb"); assert(f);
            assert(fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes)); fclose(f);
            lv_image_dsc_t image = {0};
            image.header.magic = LV_IMAGE_HEADER_MAGIC; image.header.cf = LV_COLOR_FORMAT_RAW;
            image.data = bytes; image.data_size = sizeof(bytes);
            for (int memory = 0; memory < 2; memory++) {
                const void *src = memory ? (const void *)&image : (const void *)"L:bmp-contract.BMP";
                lv_image_decoder_args_t args = {0};
                args.premultiply = false;
                lv_image_decoder_dsc_t a, b;
                int before = decodes;
                open_mpp(&a, src, &args);
                assert(decodes == before); /* Software BMP never invokes MPP codec. */
                for (unsigned y = 0; y < 2; y++)
                    assert(memcmp(a.decoded->data + y * a.decoded->header.stride,
                                  expected[y], row_bytes) == 0);
                open_mpp(&b, src, &args);
                assert(a.decoded == b.decoded);
                lv_aic_mpp_cache_drop(src);
                lv_image_decoder_close(&a);
                assert(live_cma > 0);
                lv_image_decoder_close(&b);
                assert(live_cma == 0);
                fail_alloc = 1;
                assert(lv_image_decoder_open(&a, src, &args) == LV_RESULT_INVALID);
                fail_alloc = 0;
                assert(live_cma == 0);
            }
            assert(remove(filename) == 0);
        }
    }
    lv_aic_mpp_decoder_deinit(decoder);
}

#include "lv_aic_video_window.h"
static void video_window_contract(void)
{
    lv_display_t *display=lv_display_create(320,240);
    assert(display);
    for(unsigned cycle=0;cycle<20;cycle++) {
        lv_obj_t *window=lv_aic_video_window_create(lv_screen_active());
        assert(window);
        lv_aic_video_window_set_color(window,lv_color_hex(0x123456));
        assert(lv_image_get_src(window)==NULL);
        lv_aic_video_window_set_size(window,32,24);
        assert(!strcmp(lv_image_get_src(window),"L:/32x24_0_00123456.fake"));
        lv_obj_update_layout(window);
        assert(lv_obj_get_width(window)==32 && lv_obj_get_height(window)==24);
        const void *source=lv_image_get_src(window);
        lv_aic_video_window_set_size(window,32,24);
        lv_aic_video_window_set_color(window,lv_color_hex(0x123456));
        assert(lv_image_get_src(window)==source);
        lv_aic_video_window_set_size(window,0,24);
        lv_aic_video_window_set_size(window,4097,24);
        lv_aic_video_window_set_size(window,4096,4096);
        assert(lv_image_get_src(window)==source);
        lv_aic_video_window_set_color(window,lv_color_hex(0xabcdef));
        lv_aic_video_window_set_size(window,48,16);
        lv_obj_update_layout(window);
        assert(lv_obj_get_width(window)==48 && lv_obj_get_height(window)==16);
        assert(!strcmp(lv_image_get_src(window),"L:/48x16_0_00abcdef.fake"));
        lv_obj_set_style_pad_all(window,0,0);
        assert(!strcmp(lv_image_get_src(window),"L:/48x16_0_00abcdef.fake"));
        lv_obj_delete(window);
    }
    lv_display_delete(display);
    assert(live_cma==0 && decodes==0);
}
int main(int argc, char **argv) {
    jpeg_header_boundaries();
    assert(argc == 2 || argc == 3);
    lv_init();
    {
        lv_image_decoder_dsc_t dsc = {0};
        lv_image_header_t header = {0};
        dsc.src = "L:/320x240_0_00123456.fake";
        assert(lv_aic_mpp_info_cb(NULL, &dsc, &header) == LV_RESULT_OK);
        assert(header.w == 320 && header.h == 240 && header.cf == LV_COLOR_FORMAT_ARGB8888);
        assert(live_cma == 0 && decodes == 0);
        dsc.header = header;
        assert(lv_aic_mpp_open_cb(NULL, &dsc) == LV_RESULT_INVALID);
        assert(live_cma == 0 && decodes == 0);
        lv_image_decoder_t *registered = NULL;
        lv_fs_drv_t *drive = lv_fs_get_drv('L');
        assert(drive);
        lv_fs_drv_t saved_drive = *drive;
        assert(lv_aic_mpp_decoder_init(&registered) == LV_AIC_OK);
        video_window_contract();
        assert(lv_image_decoder_get_info(dsc.src, &header) == LV_RESULT_OK);
        assert(header.w == 320 && header.h == 240 && header.cf == LV_COLOR_FORMAT_ARGB8888);
        lv_fs_file_t first, second, real;
        assert(lv_fs_open(&first, dsc.src, LV_FS_MODE_RD) == LV_FS_RES_OK);
        assert(lv_fs_open(&second, dsc.src, LV_FS_MODE_RD) == LV_FS_RES_OK);
        assert(!lv_aic_mpp_decoder_can_deinit());
        lv_aic_mpp_decoder_deinit(registered);
        assert(g_aic_mpp_decoder == registered);
        char bytes[4];
        uint32_t read = 99, position = 99;
        assert(lv_fs_read(&first, bytes, sizeof(bytes), &read) == LV_FS_RES_OK && read == 0);
        assert(lv_fs_seek(&first, 0, LV_FS_SEEK_END) == LV_FS_RES_OK);
        assert(lv_fs_tell(&first, &position) == LV_FS_RES_OK && position == 0);
        assert(lv_fs_write(&first, bytes, sizeof(bytes), &read) == LV_FS_RES_DENIED);
        assert(lv_fs_close(&first) == LV_FS_RES_OK && !lv_aic_mpp_decoder_can_deinit());
        assert(lv_fs_close(&second) == LV_FS_RES_OK && lv_aic_mpp_decoder_can_deinit());
        /* An ordinary real file still uses the original driver's ABI. */
        assert(lv_fs_open(&real, "L:" __FILE__, LV_FS_MODE_RD) == LV_FS_RES_OK);
        assert(lv_fs_read(&real, bytes, sizeof(bytes), &read) == LV_FS_RES_OK && read == 4);
        assert(memcmp(bytes, "#inc", 4) == 0);
        assert(lv_fs_tell(&real, &position) == LV_FS_RES_OK && position == 4);
        assert(lv_fs_seek(&real, 0, LV_FS_SEEK_SET) == LV_FS_RES_OK);
        assert(lv_fs_close(&real) == LV_FS_RES_OK);
        lv_aic_mpp_decoder_deinit(registered);
        assert(drive->open_cb == saved_drive.open_cb && drive->close_cb == saved_drive.close_cb);
        assert(drive->read_cb == saved_drive.read_cb && drive->write_cb == saved_drive.write_cb);
        assert(drive->seek_cb == saved_drive.seek_cb && drive->tell_cb == saved_drive.tell_cb);
    }
    aicp_header_contract();
    if (argc == 3 && strcmp(argv[2], "--bmp") == 0) {
        bmp_pixel_contract();
        lv_deinit();
        return 0;
    }
#ifdef AIC_MPP_AICP_DEC_ENABLE
    if (argc == 3 && strcmp(argv[2], "--aicp") == 0) {
        aicp_fixture_contract(argv[1]);
        lv_deinit();
        return 0;
    }
#endif
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
