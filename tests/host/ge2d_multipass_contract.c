/* SPDX-License-Identifier: Apache-2.0
 * Real executor; descriptor-driven CPU GE model, NOT hardware acceptance. */
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define ulong uintptr_t
#include "../../draw/ge2d/lv_draw_aic_ge2d.c"
#include "../../draw/ge2d/lv_draw_aic_ge2d_image.c"
#include "../../draw/ge2d/lv_draw_aic_ge2d_fill.c"

enum { N = 96, SCRATCH_SLOTS = 5 };
static uint8_t source[64*256];
static uint32_t output[N*N], whole[N*N], staged_target_before[N*N];
static void *allocated[SCRATCH_SLOTS], *bases[SCRATCH_SLOTS];
static size_t sizes[SCRATCH_SLOTS];
static unsigned allocation_calls, frees, event, fail_event, fail_alloc, copies, scales, rotations;
static bool fail_address,alpha_background;
static unsigned alpha_invalidates,alpha_target_cleans;
static uint32_t background(unsigned i)
{
    const uint32_t values[]={0x00102030,0x40205090,0x80306020,0xff103050};
    return alpha_background?values[(i%N)/24]:0xff102030;
}
static const uint32_t colors[] = {0x00ff00ff,0x40d04020,0x8020b050,0xff9050c0};

void *aicos_malloc_align(unsigned int type,size_t bytes,size_t align)
{
    assert(type == MEM_CMA && align == 64);
    if(++allocation_calls == fail_alloc) return NULL;
    for(unsigned i=0;i<SCRATCH_SLOTS;i++) if(!allocated[i]) {
        bases[i] = malloc(bytes+63); assert(bases[i]);
        allocated[i] = (void *)(((uintptr_t)bases[i]+63)&~(uintptr_t)63);
        sizes[i] = bytes; memset(allocated[i],0xa5,bytes); return allocated[i];
    }
    assert(false); return NULL;
}
void aicos_free_align(unsigned int type,void *p)
{
    assert(type == MEM_CMA);
    for(unsigned i=0;i<SCRATCH_SLOTS;i++) if(p==allocated[i]) {
        free(bases[i]); bases[i]=allocated[i]=NULL; frees++; return;
    }
    assert(false);
}
static uint8_t *address(uint32_t a)
{
    if(a==(uint32_t)(uintptr_t)source) return source;
    if(a==(uint32_t)(uintptr_t)output) return (uint8_t *)output;
    for(unsigned i=0;i<SCRATCH_SLOTS;i++) if(allocated[i] && a==(uint32_t)(uintptr_t)allocated[i]) return allocated[i];
    assert(false); return NULL;
}
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{
    if(!b || !b->data) return false;
    if(b->data==source || b->data==(uint8_t *)output) return true;
    for(unsigned i=0;i<SCRATCH_SLOTS;i++) if(b->data==allocated[i]) return !fail_address && b->data_size<=sizes[i];
    return false;
}
bool lv_draw_aic_ge2d_dst_format_supported(lv_color_format_t cf) { return lv_aic_pixel_format_is_ge2d_dst(cf); }
void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *b,const lv_area_t *a)
{
    assert(b && a);
    if(b->data==(uint8_t *)output && b->header.cf==LV_COLOR_FORMAT_ARGB8888) {
        assert(alpha_active && alpha_invalidates==alpha_target_cleans+1);
        alpha_target_cleans++;
    }
}
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b,const lv_area_t *a)
{
    assert(b && a);
    if(b->data==(uint8_t *)output && b->header.cf==LV_COLOR_FORMAT_ARGB8888)
        memcpy(staged_target_before,output,sizeof output);
}
int lv_draw_aic_ge2d_yuv(lv_draw_task_t *t) { (void)t; return 0; }
bool lv_draw_aic_ge2d_yuv_faulted(void) { return false; }
struct mpp_ge *mpp_ge_open(void) { return (void *)(uintptr_t)1; }
void mpp_ge_close(struct mpp_ge *g) { (void)g; }
int mpp_ge_fillrect(struct mpp_ge *g,struct ge_fillrect *f) { (void)g;(void)f;assert(false);return -1; }
int mpp_ge_emit(struct mpp_ge *g) { (void)g;return ++event==fail_event?-1:0; }
int mpp_ge_sync(struct mpp_ge *g) { (void)g;return ++event==fail_event?-1:0; }

static uint32_t read_pixel(const struct mpp_buf *b,int x,int y)
{
    if(x<0 || y<0 || x>=b->size.width || y>=b->size.height) return 0;
    unsigned bpp=b->format==MPP_FMT_RGB_888?3:b->format==MPP_FMT_RGB_565?2:4;
    uint8_t *p=address(b->phy_addr[0])+y*b->stride[0]+x*bpp;
    if(bpp==2) {
        unsigned v=p[0]+256*p[1],r=(v>>11)*255/31,g=((v>>5)&63)*255/63,bl=(v&31)*255/31;
        return 0xff000000|(r<<16)|(g<<8)|bl;
    }
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|
           ((uint32_t)(bpp==3||b->format==MPP_FMT_XRGB_8888?255:p[3])<<24);
}
static void write_pixel(const struct mpp_buf *b,int x,int y,uint32_t v)
{
    assert(x>=0 && y>=0 && x<b->size.width && y<b->size.height);
    memcpy(address(b->phy_addr[0])+y*b->stride[0]+x*4,&v,4);
}
static uint32_t filtered(const struct mpp_buf *b,double x,double y)
{
    int ix=(int)floor(x),iy=(int)floor(y); double fx=x-ix,fy=y-iy;
    uint32_t out=0, p[4]={read_pixel(b,ix,iy),read_pixel(b,ix+1,iy),read_pixel(b,ix,iy+1),read_pixel(b,ix+1,iy+1)};
    for(unsigned k=0;k<4;k++) {
        double n=((p[0]>>(k*8))&255)*(1-fx)*(1-fy)+((p[1]>>(k*8))&255)*fx*(1-fy)+
                 ((p[2]>>(k*8))&255)*(1-fx)*fy+((p[3]>>(k*8))&255)*fx*fy;
        out|=(uint32_t)lround(n)<<(k*8);
    }
    return out;
}
int mpp_ge_bitblt(struct mpp_ge *g,struct ge_bitblt *b)
{
    (void)g; if(alpha_active) assert(!memcmp(staged_target_before,output,sizeof output));
    if(++event==fail_event) return -1;
    bool scale=b->scale_phase.scale_phase_en;
    bool direct=b->dst_buf.phy_addr[0]==(uint32_t)(uintptr_t)output;
    bool staged=alpha_surface.data && b->dst_buf.phy_addr[0]==(uint32_t)(uintptr_t)alpha_surface.data;
    struct mpp_rect r=b->dst_buf.crop_en?b->dst_buf.crop:(struct mpp_rect){0,0,b->dst_buf.size.width,b->dst_buf.size.height};
    if(scale) {
        scales++; if(!direct) assert(!b->ctrl.alpha_en && !b->src_buf.flags && !b->dst_buf.flags);
        assert(b->scale_phase.scaler_en && b->scale_phase.channel_num==1);
    } else if(!direct && !staged) {
        copies++; assert(r.x==2 && r.y==2 && r.width==32 && r.height==32);
        assert(b->src_buf.flags==0);
        if(b->ctrl.alpha_en) assert(b->ctrl.alpha_rules==GE_PD_SRC && b->dst_buf.flags==MPP_BUF_IS_PREMULTIPLY);
    } else copies++;
    for(int y=0;y<r.height;y++) for(int x=0;x<r.width;x++) {
        uint32_t v;
        if(scale) {
            int u=x,vv=y;
            switch(MPP_ROTATION_GET(b->ctrl.flags)) {
            case MPP_ROTATION_90: u=y;vv=r.width-1-x;break;
            case MPP_ROTATION_180: u=r.width-1-x;vv=r.height-1-y;break;
            case MPP_ROTATION_270: u=r.height-1-y;vv=x;break;
            default:break;
            }
            v=filtered(&b->src_buf,b->src_buf.crop.x+(b->scale_phase.h_phase_16[0]+(int64_t)u*b->scale_phase.dx_16[0])/65536.0,
                       b->src_buf.crop.y+(b->scale_phase.v_phase_16[0]+(int64_t)vv*b->scale_phase.dy_16[0])/65536.0);

        }
        else {
            int u=x,vv=y;
            switch(MPP_ROTATION_GET(b->ctrl.flags)) {
            case MPP_ROTATION_90:u=y;vv=r.width-1-x;break;
            case MPP_ROTATION_180:u=r.width-1-x;vv=r.height-1-y;break;
            case MPP_ROTATION_270:u=r.height-1-y;vv=x;break;
            default:break;
            }
            v=read_pixel(&b->src_buf,u+(b->src_buf.crop_en?b->src_buf.crop.x:0),vv+(b->src_buf.crop_en?b->src_buf.crop.y:0));
            if(b->dst_buf.flags & MPP_BUF_IS_PREMULTIPLY) {
                unsigned a=v>>24;uint32_t p=a<<24;
                for(unsigned k=0;k<3;k++) p|=((((v>>(k*8))&255)*a+127)/255)<<(k*8);
                v=p;
            }
        }
            if(direct) {
                uint32_t bg=read_pixel(&b->dst_buf,r.x+x,r.y+y),out=0xff000000;
                unsigned opa=b->ctrl.alpha_en?b->ctrl.src_global_alpha:255;
                unsigned alpha=((v>>24)*opa+127)/255;
                bool premult=b->src_buf.flags & MPP_BUF_IS_PREMULTIPLY;
                for(unsigned k=0;k<3;k++) {
                    unsigned c=(((v>>(k*8))&255)*(premult?opa:alpha)+((bg>>(k*8))&255)*(255-alpha)+127)/255;
                    out|=LV_MIN(c,255U)<<(k*8);
                }
                v=out;
            }
        write_pixel(&b->dst_buf,r.x+x,r.y+y,v);
    }
    return 0;
}
int mpp_ge_rotate(struct mpp_ge *g,struct ge_rotation *r)
{
    (void)g;if(alpha_active) assert(!memcmp(staged_target_before,output,sizeof output));
    if(++event==fail_event) return -1;
    rotations++;
    bool staged=alpha_surface.data && r->dst_buf.phy_addr[0]==(uint32_t)(uintptr_t)alpha_surface.data;
    if(staged) assert(!r->ctrl.alpha_en && !r->src_buf.flags);
    else assert(r->src_buf.flags & MPP_BUF_IS_PREMULTIPLY);
    if(!staged) assert(r->ctrl.alpha_en && r->ctrl.src_alpha_mode==(r->ctrl.src_global_alpha==255?0U:2U));
    if(transform_scaled.data && r->src_buf.phy_addr[0]==(uint32_t)(uintptr_t)transform_scaled.data)
    for(int y=0;y<r->src_buf.size.height;y++) for(int x=0;x<r->src_buf.size.width;x++)
        if(x<2 || y<2 || x>=r->src_buf.size.width-2 || y>=r->src_buf.size.height-2)
            assert(read_pixel(&r->src_buf,x,y)==0);
    double c=r->angle_cos/4096.0,s=r->angle_sin/4096.0;
    for(int y=0;y<r->dst_buf.crop.height;y++) for(int x=0;x<r->dst_buf.crop.width;x++) {
        double dx=x-r->dst_rot_center.x,dy=y-r->dst_rot_center.y;
        uint32_t p=filtered(&r->src_buf,c*dx+s*dy+r->src_rot_center.x,-s*dx+c*dy+r->src_rot_center.y);
        int xx=r->dst_buf.crop.x+x,yy=r->dst_buf.crop.y+y;
        uint32_t bg=read_pixel(&r->dst_buf,xx,yy),out=0xff000000;
        unsigned opa=r->ctrl.src_global_alpha,a=((p>>24)*opa+127)/255;
        for(unsigned k=0;k<3;k++) {
            unsigned v=(((p>>(k*8))&255)*opa+((bg>>(k*8))&255)*(255-a)+127)/255;
            out|=LV_MIN(v,255U)<<(k*8);
        }
        write_pixel(&r->dst_buf,xx,yy,staged?p:out);
    }
    return 0;
}
void aicos_dcache_invalid_range(unsigned long *p,unsigned long size)
{ assert((void *)p==alpha_surface.data && size==alpha_surface.data_size);
  assert(!memcmp(staged_target_before,output,sizeof output));alpha_invalidates++; }
static void reset(void)
{
    for(unsigned i=0;i<SCRATCH_SLOTS;i++) assert(!allocated[i]);
    allocation_calls=frees=event=fail_event=fail_alloc=copies=scales=rotations=0;fail_address=false;
    alpha_invalidates=alpha_target_cleans=0;
    for(unsigned i=0;i<N*N;i++) output[i]=background(i);
}
static void geometry(void)
{
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.rotation=175;
    unsigned checked=0;
    for(int s=16;s<=4096;s+=17) for(int pivot=-64;pivot<=64;pivot+=7) {
        d.scale_x=s;d.scale_y=512;d.pivot=(lv_point_t){pivot,-pivot};
        lv_aic_ge2d_transform_plan_t p;
        if(!lv_aic_ge2d_transform_plan(32,32,&d,UINT32_MAX,&p)) continue;
        for(int u=2;u<p.scaled_w-2;u++) {
            double got=p.crop_x+(p.phase_x+(int64_t)(u-2)*p.step_x)/65536.0-2;
            double expected=d.pivot.x+(u-p.pivot.x)*256.0/s;
            assert(fabs(got-expected)<0.15);
            assert(got>=-2 && got<=33);
        }
        assert(!lv_aic_ge2d_transform_plan(32,32,&d,p.padded_bytes+p.scaled_bytes-1,&p));checked++;
    }
    assert(checked>4000);
}
static void recolor_contract(void);
static void mask_contract(void);
int main(void)
{
    lv_init();g_ge2d_dev=mpp_ge_open();g_ge2d_ready=true;geometry();
    lv_draw_buf_t src,dst;lv_layer_t layer={0},child={0};lv_draw_task_t task={0};
    assert(lv_draw_buf_init(&dst,N,N,LV_COLOR_FORMAT_XRGB8888,N*4,output,sizeof output)==LV_RESULT_OK);
    layer.draw_buf=&dst;layer.color_format=LV_COLOR_FORMAT_XRGB8888;layer.buf_area=(lv_area_t){90,190,90+N-1,190+N-1};
    task.target_layer=&layer;task.area=(lv_area_t){120,220,151,251};task.clip_area=layer.buf_area;
    const int ratios[][2]={{128,384},{384,192},{512,512},{255,384},{264,384},{281,192}};
    unsigned scenes=0,pixels=0;
    lv_draw_image_dsc_t d;
    for(unsigned kind=0;kind<6;kind++) for(unsigned ratio=0;ratio<sizeof ratios/sizeof ratios[0];ratio++)
    for(unsigned angle=0;angle<3;angle++) for(unsigned is_layer=0;is_layer<2;is_layer++)
    for(unsigned opacity=0;opacity<2;opacity++) {
        bool premult=kind==2 || kind==3, has_alpha=kind>=1 && kind<=3;
        const lv_color_format_t formats[]={LV_COLOR_FORMAT_RGB888,LV_COLOR_FORMAT_ARGB8888,
            LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,LV_COLOR_FORMAT_ARGB8888,LV_COLOR_FORMAT_XRGB8888,LV_COLOR_FORMAT_RGB565};
        lv_color_format_t cf=formats[kind];
        assert(lv_draw_buf_init(&src,32,32,cf,128,source,sizeof source)==LV_RESULT_OK);
        if(kind==3) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
        for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++) {
            uint32_t v=colors[(x>=16)+2*(y>=16)];unsigned alpha=has_alpha?v>>24:255,bpp=kind==0?3:kind==5?2:4;
            if(kind==5) {
                uint16_t rgb=(((v>>16)&255)>>3)<<11|(((v>>8)&255)>>2)<<5|(v&255)>>3;
                memcpy(source+y*128+x*2,&rgb,2);continue;
            }
            for(unsigned k=0;k<3;k++) source[y*128+x*bpp+k]=premult?(((v>>(8*k))&255)*alpha+127)/255:(v>>(8*k))&255;
            if(kind) source[y*128+x*4+3]=alpha;
        }
        lv_draw_image_dsc_init(&d);d.header=src.header;d.pivot=(lv_point_t){7,9};d.rotation=angle==0?175:angle==1?451:3333;
        d.scale_x=ratios[ratio][0];d.scale_y=ratios[ratio][1];d.opa=opacity?128:255;
        child.draw_buf=&src;d.src=is_layer?(void *)&child:(void *)&src;
        task.type=is_layer?LV_DRAW_TASK_TYPE_LAYER:LV_DRAW_TASK_TYPE_IMAGE;task.draw_dsc=&d;
        reset();lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK && outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        assert(copies==1 && scales==(ratio>=4?2U:1U) && rotations==1 && frees==2 && allocation_calls==2);
        memcpy(whole,output,sizeof output);
        double r=d.rotation*3.141592653589793/1800.0,c=cos(r),s=sin(r);unsigned interior=0;
        for(int y=0;y<N;y++) for(int x=0;x<N;x++) {
            double dx=90+x-120-d.pivot.x,dy=190+y-220-d.pivot.y;
            double ix=(c*dx+s*dy)*256/d.scale_x+d.pivot.x,iy=(-s*dx+c*dy)*256/d.scale_y+d.pivot.y;
            if(((ix>=4&&ix<=11)||(ix>=20&&ix<=27)) && ((iy>=4&&iy<=11)||(iy>=20&&iy<=27))) {
                uint32_t color=colors[(ix>=16)+2*(iy>=16)];unsigned alpha=has_alpha?color>>24:255;
                if(kind==5) {
                    unsigned red=(((color>>16)&255)>>3)*255/31;
                    unsigned green=(((color>>8)&255)>>2)*255/63,blue=((color&255)>>3)*255/31;
                    color=(red<<16)|(green<<8)|blue;
                }
                alpha=(alpha*d.opa+127)/255;
                for(unsigned k=0;k<3;k++) {
                    int expected=(((color>>(k*8))&255)*alpha+((0xff102030U>>(k*8))&255)*(255-alpha)+127)/255;
                    assert(abs((int)((output[y*N+x]>>(k*8))&255)-expected)<=2);
                }
                interior++;
            }
        }
        assert(interior>90);pixels+=interior;
        reset();
        for(int i=0;i<2;i++) {
            task.clip_area=(lv_area_t){90+i*(N/2),190,90+(i+1)*(N/2)-1,190+N-1};
            assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK && outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        }
        assert(!memcmp(output,whole,sizeof output));task.clip_area=layer.buf_area;
        lv_image_cache_drop(&src);scenes++;
    }
    /* Nine cells share ONE preparation, preserve clipping, and produce the
     * same pixels when the refresh divides the middle column of tiles. */
    for(unsigned tile_zoom=0;tile_zoom<2;tile_zoom++) {
    task.type=LV_DRAW_TASK_TYPE_IMAGE;d.src=&src;d.rotation=175;
    d.scale_x=tile_zoom?264:512;d.scale_y=512;d.tile=true;task.area=d.image_area=layer.buf_area;
    lv_draw_aic_ge2d_outcome_t tiled_outcome;
    reset();
    assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
    assert(copies==1 && scales==(tile_zoom?2U:1U) && rotations==9 && allocation_calls==2 && frees==2);
    memcpy(whole,output,sizeof output);reset();
    for(int i=0;i<2;i++) {
        task.clip_area=(lv_area_t){90+i*48,190,90+(i+1)*48-1,285};
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
    }
    assert(!memcmp(whole,output,sizeof output));
    /* The last one-pixel cell must decline during the FIRST geometry pass. */
    reset();task.clip_area=(lv_area_t){90,190,154,285};
    assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
    assert(!event && !allocation_calls);
    task.clip_area=layer.buf_area;d.tile=false;task.area=(lv_area_t){120,220,151,251};
    lv_image_cache_drop(&src);
    }
    /* Full task preparation failures must replay in native software only
     * before any command. Uncertain command/emit/sync completion retains both
     * scratch allocations and decoder, and blocks every subsequent request. */
    task.type=LV_DRAW_TASK_TYPE_IMAGE;d.src=&src;d.rotation=175;d.scale_x=d.scale_y=512;
    for(unsigned failure=1;failure<=3;failure++) {
        reset();if(failure<=2) fail_alloc=failure;else fail_address=true;
        lv_draw_aic_ge2d_outcome_t result;
        assert(lv_draw_aic_ge2d_image(&task,&result)==LV_RESULT_OK && result==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
        assert(event==0 && !allocated[0] && !allocated[1]);lv_image_cache_drop(&src);
    }
    d.scale_x=264;
    for(unsigned failure=1;failure<=12;failure++) {
        reset();fail_event=failure;lv_draw_aic_ge2d_outcome_t result;
        assert(lv_draw_aic_ge2d_image(&task,&result)==LV_RESULT_INVALID);
        assert(decoder_quarantined && allocated[0] && allocated[1] && !frees && event==failure);
        assert(lv_draw_aic_ge2d_image(&task,&result)==LV_RESULT_INVALID && event==failure);
        /* Test-only drain: this CPU model has no outstanding DMA. */
        lv_image_decoder_close(&image_decoder);decoder_quarantined=false;
        s_blit_failed=false;transform_release();lv_image_cache_drop(&src);
    }
    /* Ordinary RGB scaling: inverse-coordinate ramps, both alpha encodings,
     * all orthogonal placements and split refresh across the stripe boundary. */
    unsigned direct_scenes=0,direct_pixels=0;
    task.type=LV_DRAW_TASK_TYPE_IMAGE;task.area=(lv_area_t){106,222,169,253};
    for(unsigned kind=0;kind<3;kind++) for(unsigned ratio=0;ratio<3;ratio++)
    for(unsigned angle=0;angle<4;angle++) {
        const unsigned zooms[]={257,264,281};
        bool premult=kind==2;
        assert(lv_draw_buf_init(&src,64,32,premult?LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
               kind?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_XRGB8888,256,source,sizeof source)==LV_RESULT_OK);
        for(unsigned y=0;y<32;y++) for(unsigned x=0;x<64;x++) {
            unsigned a=kind?128:255,c[3]={40+x+y,30+5*y,20+2*x};
            for(unsigned k=0;k<3;k++) source[y*256+x*4+k]=premult?(c[k]*a+127)/255:c[k];
            source[y*256+x*4+3]=a;
        }
        lv_draw_image_dsc_init(&d);d.src=&src;d.header=src.header;
        d.pivot=(lv_point_t){32,16};d.scale_x=zooms[ratio];d.scale_y=384;d.rotation=angle*900;d.opa=128;
        /* Transform the interior source rectangle independently; ceil/floor
         * discard exterior fractional samples, preserving a >31 scaler width. */
        const int cs[]={1,0,-1,0},sn[]={0,1,0,-1};
        double minx=1e9,maxx=-1e9,miny=1e9,maxy=-1e9;
        for(unsigned j=0;j<4;j++) {
            double u=((j&1)?61:2)-32,v=((j&2)?29:2)-16;
            u*=d.scale_x/256.0;v*=d.scale_y/256.0;
            double xx=138+cs[angle]*u-sn[angle]*v,yy=238+sn[angle]*u+cs[angle]*v;
            minx=fmin(minx,xx);maxx=fmax(maxx,xx);miny=fmin(miny,yy);maxy=fmax(maxy,yy);
        }
        lv_area_t clip={(int)ceil(minx),(int)ceil(miny),(int)floor(maxx),(int)floor(maxy)};
        task.clip_area=clip;reset();lv_draw_aic_ge2d_outcome_t result;
        assert(lv_draw_aic_ge2d_image(&task,&result)==LV_RESULT_OK && result==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        assert(scales>=2 && !copies && !rotations && !allocation_calls);
        memcpy(whole,output,sizeof output);
        for(int y=0;y<N;y++) for(int x=0;x<N;x++) {
            int xx=x+90,yy=y+190;
            if(xx<clip.x1 || xx>clip.x2 || yy<clip.y1 || yy>clip.y2) { assert(output[y*N+x]==0xff102030);continue; }
            double u=(cs[angle]*(xx-138)+sn[angle]*(yy-238))*256.0/d.scale_x+32;
            double v=(-sn[angle]*(xx-138)+cs[angle]*(yy-238))*256.0/d.scale_y+16;
            double colors[3]={40+u+v,30+5*v,20+2*u};unsigned alpha=((kind?128:255)*128+127)/255;
            for(unsigned k=0;k<3;k++) {
                int expected=(int)lround((colors[k]*alpha+((0xff102030U>>(k*8))&255)*(255-alpha))/255.0);
                assert(abs((int)((output[y*N+x]>>(k*8))&255)-expected)<=2);
            }
            assert(output[y*N+x]>>24==255);direct_pixels++;
        }
        reset();
        for(unsigned half=0;half<2;half++) {
            task.clip_area=clip;
            if(half) task.clip_area.x1=(clip.x1+clip.x2)/2+1;
            else task.clip_area.x2=(clip.x1+clip.x2)/2;
            assert(lv_draw_aic_ge2d_image(&task,&result)==LV_RESULT_OK && result==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        }
        assert(!memcmp(output,whole,sizeof output));lv_image_cache_drop(&src);direct_scenes++;
        /* A failed later submission follows a completed target strip. Never
         * replay this translucent request through native software. */
        task.clip_area=clip;reset();fail_event=4;
        assert(lv_draw_aic_ge2d_image(&task,&result)==LV_RESULT_INVALID);
        assert(event==4 && scales==1 && decoder_quarantined && !allocation_calls);
        bool changed=false;for(unsigned i=0;i<N*N;i++) if(output[i]!=0xff102030) changed=true;
        assert(changed && memcmp(output,whole,sizeof output));
        assert(lv_draw_aic_ge2d_image(&task,&result)==LV_RESULT_INVALID && event==4);
        lv_image_decoder_close(&image_decoder);decoder_quarantined=false;s_blit_failed=false;
        lv_image_cache_drop(&src);
    }
    recolor_contract();mask_contract();
    reset();lv_deinit();
    printf("PASS %u direct striped scenes, %u independent ramp pixels, exact partial refresh and late-strip quarantine\n",direct_scenes,direct_pixels);
    printf("PASS %u multipass IMAGE/LAYER scenes, %u independent interior pixels, partial refresh, allocation/address failure and 12 DMA failure points\n",scenes,pixels);
    return 0;
}

/* Native SW is the independent pixel oracle. The model above interprets only
 * submitted GE descriptors; it never sees the original recolor request. */
static void recolor_contract(void)
{
    static uint32_t reference[N*N];
    static uint8_t original[sizeof source];
    const lv_color_format_t formats[]={LV_COLOR_FORMAT_RGB565,LV_COLOR_FORMAT_RGB888,
        LV_COLOR_FORMAT_XRGB8888,LV_COLOR_FORMAT_ARGB8888,
        LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,LV_COLOR_FORMAT_ARGB8888};
    const unsigned mixes[]={0,4,64,128,192,253,255};
    lv_draw_buf_t src,dst,ref;lv_layer_t layer={0},reference_layer={0},child={0};
    assert(lv_draw_buf_init(&dst,N,N,LV_COLOR_FORMAT_XRGB8888,N*4,output,sizeof output)==LV_RESULT_OK);
    assert(lv_draw_buf_init(&ref,N,N,LV_COLOR_FORMAT_XRGB8888,N*4,reference,sizeof reference)==LV_RESULT_OK);
    layer.draw_buf=&dst;layer.color_format=LV_COLOR_FORMAT_XRGB8888;layer.buf_area=(lv_area_t){90,190,90+N-1,190+N-1};
    reference_layer=layer;reference_layer.draw_buf=&ref;
    lv_draw_task_t task={0};task.target_layer=&layer;task.clip_area=layer.buf_area;
    task.area=(lv_area_t){120,220,151,251};
    unsigned scenes=0,checked=0,worst=0,alpha_native=0,alpha_flat=0,alpha_ramp=0;
    for(unsigned destination=0;destination<2;destination++)
    for(unsigned pattern=0;pattern<2;pattern++)
    for(unsigned format=0;format<6;format++) for(unsigned tint=0;tint<7;tint++)
    for(unsigned transform=0;transform<(destination?6U:4U);transform++) for(unsigned opacity=0;opacity<2;opacity++) {
        alpha_background=destination!=0;
        dst.header.cf=ref.header.cf=destination?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_XRGB8888;
        layer.color_format=reference_layer.color_format=dst.header.cf;
        assert(lv_draw_buf_init(&src,32,32,formats[format],128,source,sizeof source)==LV_RESULT_OK);
        bool premult=format>=4;if(format==5) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
        unsigned bpp=lv_color_format_get_size(formats[format]);memset(source,0xa5,sizeof source);
        for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++) {
            uint32_t color=pattern ? ((uint32_t)(16+7*x)<<24)|((32+6*y)<<16)|((20+3*x+3*y)<<8)|(8+7*x) : colors[(x>=16)+2*(y>=16)];
            unsigned alpha=format>=3?color>>24:255;
            uint8_t *p=source+y*128+x*bpp;
            if(bpp==2) {
                uint16_t value=(((color>>16)&255)>>3)<<11|(((color>>8)&255)>>2)<<5|(color&255)>>3;
                memcpy(p,&value,2);
            } else {
                for(unsigned k=0;k<3;k++) p[k]=premult?(((color>>(k*8))&255)*alpha+127)/255:(color>>(k*8))&255;
                if(bpp==4) p[3]=alpha;
            }
        }
        memcpy(original,source,sizeof source);
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&src;d.header=src.header;
        d.recolor=lv_color_make(220,45,150);d.recolor_opa=mixes[tint];d.opa=opacity?128:255;
        d.pivot=(lv_point_t){7,9};
        if(transform) { d.scale_x=384;d.scale_y=320;d.rotation=transform==2?333:0; }
        if(transform==3) { d.scale_x=d.scale_y=512;d.rotation=900;d.pivot=(lv_point_t){16,16}; }
        if(transform>=4) { d.scale_x=d.scale_y=256;d.rotation=transform==4?333:900;d.pivot=(lv_point_t){16,16}; }
        child.draw_buf=&src;child.buf_area=task.area;child.color_format=formats[format];
        bool is_layer=(tint&1)!=0;if(is_layer) d.src=&child;
        task.type=is_layer?LV_DRAW_TASK_TYPE_LAYER:LV_DRAW_TASK_TYPE_IMAGE;task.draw_dsc=&d;
        task.clip_area=transform==1 || transform==3 || transform==5?task.area:layer.buf_area;
        reset();lv_draw_aic_ge2d_outcome_t outcome;
        lv_result_t status=lv_draw_aic_ge2d_image(&task,&outcome);
        if(status!=LV_RESULT_OK || outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE)
            fprintf(stderr,"recolor route format=%u mix=%u transform=%u opacity=%u status=%d outcome=%d allocations=%u events=%u\n",format,mixes[tint],transform,d.opa,status,outcome,allocation_calls,event);
        assert(status==LV_RESULT_OK && outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        assert(allocation_calls==(transform==2?2U:0U)+(tint?1U:0U)+destination && frees==allocation_calls);
        assert(alpha_invalidates==destination && alpha_target_cleans==destination);
        assert(!memcmp(original,source,sizeof source));
        for(unsigned i=0;i<N*N;i++) reference[i]=background(i);
        lv_draw_task_t sw=task;sw.target_layer=&reference_layer;
        if(is_layer) lv_draw_sw_layer(&sw,&d,&sw.area);else lv_draw_sw_image(&sw,&d,&sw.area);
        double r=d.rotation*3.141592653589793/1800.0,c=cos(r),sn=sin(r);unsigned pixels=0;
        for(int y=0;y<N;y++) for(int x=0;x<N;x++) {
            double dx=x+90-120-d.pivot.x,dy=y+190-220-d.pivot.y;
            double ix=(c*dx+sn*dy)*256/d.scale_x+d.pivot.x,iy=(-sn*dx+c*dy)*256/d.scale_y+d.pivot.y;
            bool interior=pattern ? ix>=4&&ix<=27&&iy>=4&&iy<=27 :
                ((ix>=4&&ix<=11)||(ix>=20&&ix<=27)) && ((iy>=4&&iy<=11)||(iy>=20&&iy<=27));
            if(!transform || interior) for(unsigned k=0;k<(destination?4U:3U);k++) {
                unsigned error=abs((int)((output[y*N+x]>>(k*8))&255)-(int)((reference[y*N+x]>>(k*8))&255));
                if(error>worst) worst=error;
                if(destination) {
                    unsigned *peak=!transform?&alpha_native:pattern?&alpha_ramp:&alpha_flat;
                    if(error>*peak) *peak=error;
                }
                /* RGB565 quantizes recolor before filtering; native SW recolors
                 * after filtering. The ramp budget includes one RGB565 step. */
                unsigned tolerance=pattern && transform ? (destination?16:10) : destination && transform ? 5 : 3;
                if(k==3) tolerance=3;
                if(error>tolerance) { fprintf(stderr,"recolor destination=%u pattern=%u format=%u mix=%u transform=%u opacity=%u xy=%d,%d channel=%u error=%u\n",destination,pattern,format,mixes[tint],transform,d.opa,x,y,k,error);assert(error<=tolerance); }
                pixels++;
            }
        }
        assert(pixels>90);checked+=pixels;scenes++;
        memcpy(whole,output,sizeof output);reset();
        lv_area_t full_clip=task.clip_area;int midpoint=(full_clip.x1+full_clip.x2)/2;
        for(unsigned half=0;half<2;half++) {
            task.clip_area=full_clip;
            if(half) task.clip_area.x1=midpoint+1;else task.clip_area.x2=midpoint;
            assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK && outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        }
        assert(!memcmp(whole,output,sizeof output));task.clip_area=layer.buf_area;
        lv_image_cache_drop(&src);
    }
    printf("PASS straight-ARGB native max=%u, transformed flat max=%u, ramp max=%u (RGBA comparisons)\n",alpha_native,alpha_flat,alpha_ramp);
    alpha_background=false;dst.header.cf=ref.header.cf=LV_COLOR_FORMAT_XRGB8888;
    layer.color_format=reference_layer.color_format=LV_COLOR_FORMAT_XRGB8888;
    /* Preparation failures preserve the source and draw only via software. */
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&src;d.header=src.header;
    d.recolor=lv_color_make(220,45,150);d.recolor_opa=128;d.rotation=333;d.scale_x=264;d.scale_y=384;
    task.type=LV_DRAW_TASK_TYPE_IMAGE;task.draw_dsc=&d;
    for(unsigned failure=1;failure<=4;failure++) {
        reset();if(failure<=3) fail_alloc=failure;else fail_address=true;
        lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK && outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
        assert(!event && !allocated[0] && !allocated[1] && !allocated[2]);
        assert(!memcmp(original,source,sizeof source));lv_image_cache_drop(&src);
    }
    for(unsigned failure=1;failure<=12;failure++) {
        reset();fail_event=failure;lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID);
        assert(decoder_quarantined && allocated[0] && allocated[1] && allocated[2] && !frees && event==failure);
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID && event==failure);
        assert(!memcmp(original,source,sizeof source));
        /* Synchronous CPU-only mock drain. Real uncertain DMA stays retained. */
        lv_image_decoder_close(&image_decoder);decoder_quarantined=false;s_blit_failed=false;transform_release();
        lv_image_cache_drop(&src);
    }
    /* Recolored tiles share one source map; all geometry precedes target DMA. */
    d.rotation=175;d.scale_x=d.scale_y=512;d.tile=true;
    task.area=d.image_area=layer.buf_area;task.clip_area=layer.buf_area;
    reset();lv_draw_aic_ge2d_outcome_t tiled_outcome;
    assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
    assert(allocation_calls==3 && frees==3 && rotations==9);
    memcpy(whole,output,sizeof output);reset();
    for(unsigned half=0;half<2;half++) {
        task.clip_area=(lv_area_t){90+half*48,190,137+half*48,285};
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
    }
    assert(!memcmp(whole,output,sizeof output));
    reset();task.clip_area=(lv_area_t){90,190,154,285};
    assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
    assert(!event && allocation_calls==1 && frees==1);
    lv_image_cache_drop(&src);
    d.tile=false;task.area=(lv_area_t){120,220,151,251};task.clip_area=task.area;
    d.rotation=0;
    /* Copy/scale failures retain the sole recolor buffer, never replay pixels. */
    for(unsigned zoom=0;zoom<2;zoom++) for(unsigned failure=1;failure<=3;failure++) {
        d.scale_x=d.scale_y=zoom?384:256;
        reset();fail_event=failure;
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_INVALID);
        assert(decoder_quarantined && allocated[0] && !allocated[1] && !allocated[2] && !frees && event==failure);
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_INVALID && event==failure);
        lv_image_decoder_close(&image_decoder);decoder_quarantined=false;s_blit_failed=false;transform_release();
        lv_image_cache_drop(&src);
    }
    /* Direct calls use native software for effects the dispatcher rejects;
     * recolor plus a key is now the bounded two-stage GE preparation. */
    d.scale_x=d.scale_y=256;
    lv_image_colorkey_t key={.low=lv_color_make(1,2,3),.high=lv_color_make(1,2,3)};
    for(unsigned effect=0;effect<3;effect++) {
        d.colorkey=effect==0?&key:NULL;d.clip_radius=effect==1?6:0;
        d.blend_mode=effect==2?LV_BLEND_MODE_ADDITIVE:LV_BLEND_MODE_NORMAL;
        reset();
        if(effect==0) {
            assert(lv_draw_aic_ge2d_dsc_is_plain(&d));
            assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK &&
                   tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && allocation_calls>=2);
            lv_image_cache_drop(&src);
            continue;
        }
        assert(!lv_draw_aic_ge2d_dsc_is_plain(&d));
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
        assert(!event && !allocation_calls);
        for(unsigned i=0;i<N*N;i++) reference[i]=background(i);
        lv_draw_task_t sw=task;sw.target_layer=&reference_layer;lv_draw_sw_image(&sw,&d,&sw.area);
        assert(!memcmp(output,reference,sizeof output));lv_image_cache_drop(&src);
    }
    d.colorkey=NULL;d.clip_radius=0;d.blend_mode=LV_BLEND_MODE_NORMAL;
    /* ARGB staging is transactional: destination is never touched by GE and
     * the CPU blend is delayed until every tile/stripe completes. */
    alpha_background=true;dst.header.cf=ref.header.cf=LV_COLOR_FORMAT_ARGB8888;
    layer.color_format=reference_layer.color_format=LV_COLOR_FORMAT_ARGB8888;
    d.rotation=333;d.scale_x=264;d.scale_y=384;task.clip_area=layer.buf_area;
    for(unsigned failure=1;failure<=5;failure++) {
        reset();if(failure<=4) fail_alloc=failure;else fail_address=true;
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
        assert(!event && !allocated[0] && !allocated[1] && !allocated[2] && !allocated[3]);
        for(unsigned i=0;i<N*N;i++) reference[i]=background(i);
        lv_draw_task_t sw=task;sw.target_layer=&reference_layer;lv_draw_sw_image(&sw,&d,&sw.area);
        assert(!memcmp(output,reference,sizeof output));lv_image_cache_drop(&src);
    }
    for(unsigned failure=1;failure<=12;failure++) {
        reset();fail_event=failure;
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_INVALID);
        assert(decoder_quarantined && allocated[0] && allocated[1] && allocated[2] && allocated[3] && !frees);
        for(unsigned i=0;i<N*N;i++) assert(output[i]==background(i));
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_INVALID && event==failure);
        lv_image_decoder_close(&image_decoder);decoder_quarantined=false;s_blit_failed=false;transform_release();
        lv_image_cache_drop(&src);
    }
    d.rotation=0;d.recolor_opa=0;task.clip_area=task.area;
    for(unsigned zoom=0;zoom<2;zoom++) for(unsigned failure=1;failure<=3;failure++) {
        d.scale_x=d.scale_y=zoom?384:256;reset();fail_event=failure;
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_INVALID);
        assert(decoder_quarantined && allocated[0] && !allocated[1] && !frees && event==failure);
        for(unsigned i=0;i<N*N;i++) assert(output[i]==background(i));
        lv_image_decoder_close(&image_decoder);decoder_quarantined=false;s_blit_failed=false;transform_release();
        lv_image_cache_drop(&src);
    }
    d.recolor_opa=128;
    d.rotation=175;d.scale_x=d.scale_y=512;d.tile=true;
    task.area=d.image_area=task.clip_area=layer.buf_area;
    reset();assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
    assert(allocation_calls==4 && frees==4 && rotations==9);memcpy(whole,output,sizeof output);reset();
    for(unsigned half=0;half<2;half++) {
        task.clip_area=(lv_area_t){90+half*48,190,137+half*48,285};
        assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
    }
    assert(!memcmp(output,whole,sizeof output));lv_image_cache_drop(&src);
    reset();task.clip_area=(lv_area_t){90,190,154,285};
    assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_OK && tiled_outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
    assert(!event && allocation_calls==2 && frees==2);lv_image_cache_drop(&src);
    reset();task.clip_area=layer.buf_area;fail_event=13; /* third tile submission */
    assert(lv_draw_aic_ge2d_image(&task,&tiled_outcome)==LV_RESULT_INVALID);
    assert(decoder_quarantined && !frees && event==13);
    for(unsigned i=0;i<N*N;i++) assert(output[i]==background(i));
    lv_image_decoder_close(&image_decoder);decoder_quarantined=false;s_blit_failed=false;transform_release();
    lv_image_cache_drop(&src);
    reset();lv_layer_t oversized=layer,temporary;lv_draw_task_t huge=task,copy;
    lv_draw_buf_t advertised=dst;advertised.header.w=advertised.header.h=2048;
    advertised.header.stride=8192;advertised.data_size=16777216;
    oversized.draw_buf=&advertised;oversized.buf_area=(lv_area_t){0,0,2047,2047};
    huge.target_layer=&oversized;huge.clip_area=oversized.buf_area;
    assert(!alpha_prepare(&huge,&huge.clip_area,&temporary,&copy) && !allocation_calls);
    printf("PASS ARGB staged target unchanged until sync, 4 allocation failures, address/budget rejection, 19 DMA failures, tiled sharing and exact split refresh\n");
    alpha_background=false;dst.header.cf=ref.header.cf=LV_COLOR_FORMAT_XRGB8888;
    layer.color_format=reference_layer.color_format=LV_COLOR_FORMAT_XRGB8888;
    d.tile=false;d.rotation=0;d.scale_x=d.scale_y=256;
    task.area=(lv_area_t){120,220,151,251};task.clip_area=task.area;
    /* The generated bridge rejects invalid footprints without touching output,
     * and preserves destination padding when handling a transparent tint. */
    reset();src.header.w=31;assert(recolor_prepare(&src,&d));
    for(unsigned y=0;y<src.header.h;y++) {
        for(unsigned x=src.header.w*4;x<recolored.header.stride;x++) assert(recolored.data[y*recolored.header.stride+x]==0xa5);
    }
    lv_draw_buf_t bad=recolored;bad.data_size=1;
    memset(recolored.data,0xa5,recolored.data_size);
    assert(!lv_aic_sw_recolor_copy(&src,&bad,d.recolor,128));
    for(unsigned i=0;i<recolored.data_size;i++) assert(recolored.data[i]==0xa5);
    bad=recolored;bad.header.cf=LV_COLOR_FORMAT_RGB888;
    assert(!lv_aic_sw_recolor_copy(&src,&bad,d.recolor,128));
    assert(lv_aic_sw_recolor_copy(&src,&recolored,d.recolor,LV_OPA_MIN));
    for(unsigned y=0;y<src.header.h;y++) assert(!memcmp(src.data+y*src.header.stride,recolored.data+y*recolored.header.stride,src.header.w*4));
    transform_release();src.header.w=32;
    reset();lv_draw_buf_t invalid=src;invalid.header.stride=4;
    assert(!recolor_prepare(&invalid,&d) && !allocation_calls);
    invalid=src;invalid.header.w=4096;invalid.header.h=128;invalid.header.stride=16384;invalid.data_size=2097152;
    assert(!recolor_prepare(&invalid,&d) && !allocation_calls);
    printf("PASS recolor %u IMAGE/LAYER scenes, %u native SW channel comparisons (max %u), immutable sources, exact split refresh, tiled sharing, effect fallback, allocation/address and 18 DMA failures\n",scenes,checked,worst);
}


/* The model receives only GE descriptors. Native SW independently masks its
 * own source copy, while GE must leave the shared original untouched. */
static void mask_contract(void)
{
    static uint32_t reference[N*N];
    static uint8_t original[sizeof source], reference_source[sizeof source];
    static uint8_t mask_pixels[48*48], original_mask[sizeof mask_pixels], image_masked_reference[32*128];
    lv_draw_buf_t src,dst,ref,ref_src;
    lv_layer_t layer={0},reference_layer={0},child={0},reference_child={0};
    assert(lv_draw_buf_init(&dst,N,N,LV_COLOR_FORMAT_XRGB8888,N*4,output,sizeof output)==LV_RESULT_OK);
    assert(lv_draw_buf_init(&ref,N,N,LV_COLOR_FORMAT_XRGB8888,N*4,reference,sizeof reference)==LV_RESULT_OK);
    layer.draw_buf=&dst;layer.color_format=LV_COLOR_FORMAT_XRGB8888;layer.buf_area=(lv_area_t){90,190,90+N-1,190+N-1};
    reference_layer=layer;reference_layer.draw_buf=&ref;
    lv_draw_task_t task={0};task.type=LV_DRAW_TASK_TYPE_LAYER;task.target_layer=&layer;
    task.area=(lv_area_t){120,220,151,251};task.clip_area=layer.buf_area;
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&child;task.draw_dsc=&d;
    lv_image_dsc_t mask={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A8,
        .w=32,.h=32,.stride=48},.data=mask_pixels,.data_size=sizeof mask_pixels};
    unsigned scenes=0,checked=0,worst=0;
    for(unsigned destination=0;destination<2;destination++)
    for(unsigned encoding=0;encoding<3;encoding++)
    for(unsigned luminance=0;luminance<2;luminance++)
    for(unsigned shape=0;shape<3;shape++)
    for(unsigned transform=0;transform<4;transform++)
    for(unsigned opacity=0;opacity<2;opacity++)
    for(unsigned tint=0;tint<2;tint++) {
        alpha_background=destination!=0;
        dst.header.cf=ref.header.cf=destination?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_XRGB8888;
        layer.color_format=reference_layer.color_format=dst.header.cf;
        lv_color_format_t cf=encoding==1?LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:LV_COLOR_FORMAT_ARGB8888;
        assert(lv_draw_buf_init(&src,32,32,cf,128,source,sizeof source)==LV_RESULT_OK);
        if(encoding==2) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
        memset(source,0xa5,sizeof source);
        for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++) {
            unsigned a=64+5*x;
            uint8_t *pixel=source+y*128+x*4;
            const unsigned rgb[]={32+4*x,40+3*y,96+2*x};
            for(unsigned k=0;k<3;k++) pixel[k]=encoding?(rgb[k]*a+127)/255:rgb[k];
            pixel[3]=a;
        }
        memcpy(original,source,sizeof source);memcpy(reference_source,source,sizeof source);
        mask.header.cf=luminance?LV_COLOR_FORMAT_L8:LV_COLOR_FORMAT_A8;
        mask.header.w=shape==0?32:shape==1?20:40;
        mask.header.h=shape==0?32:shape==1?24:40;
        memset(mask_pixels,0xa5,sizeof mask_pixels);
        for(unsigned y=0;y<mask.header.h;y++) for(unsigned x=0;x<mask.header.w;x++)
            mask_pixels[y*48+x]=x<4?0:x>=(unsigned)mask.header.w-4?255:32+3*x+2*y;
        memcpy(original_mask,mask_pixels,sizeof mask_pixels);
        lv_draw_image_dsc_init(&d);d.src=&child;d.header=src.header;
        d.bitmap_mask_src=&mask;d.image_area=shape==1?(lv_area_t){116,218,155,253}:task.area;
        d.opa=opacity?128:255;d.recolor=lv_color_make(180,60,120);d.recolor_opa=tint?96:0;
        d.pivot=(lv_point_t){16,16};
        if(transform) { d.scale_x=384;d.scale_y=320; }
        if(transform==2) { d.rotation=900;d.scale_x=d.scale_y=512; }
        if(transform==3) d.rotation=333;
        child.draw_buf=&src;child.buf_area=task.area;child.color_format=cf;
        ref_src=src;ref_src.data=reference_source;reference_child=child;reference_child.draw_buf=&ref_src;
        task.clip_area=layer.buf_area;reset();lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_accepts_layer(&task));
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
        if(outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE)
            fprintf(stderr,"mask route dest=%u encoding=%u luma=%u shape=%u transform=%u tint=%u outcome=%u\n",
                destination,encoding,luminance,shape,transform,tint,outcome);
        assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        assert(allocation_calls==1+tint+destination+(transform==3?2U:0U) && frees==allocation_calls);
        assert(!memcmp(source,original,sizeof source) && !memcmp(mask_pixels,original_mask,sizeof mask_pixels));
        for(unsigned i=0;i<N*N;i++) reference[i]=background(i);
        lv_draw_task_t sw=task;sw.target_layer=&reference_layer;
        lv_draw_image_dsc_t sw_d=d;sw_d.src=&reference_child;sw.draw_dsc=&sw_d;
        lv_draw_sw_layer(&sw,&sw_d,&sw.area);
        double radians=d.rotation*3.141592653589793/1800.0,c=cos(radians),sn=sin(radians);
        unsigned samples=0;
        for(int y=0;y<N;y++) for(int x=0;x<N;x++) {
            double dx=x+90-120-16,dy=y+190-220-16;
            double ix=(c*dx+sn*dy)*256/d.scale_x+16,iy=(-sn*dx+c*dy)*256/d.scale_y+16;
            /* Smooth interior of the source and centered mask: avoid the
             * known native/GE filter difference at hard mask/source edges. */
            bool interior=ix>=12 && ix<=19 && iy>=12 && iy<=19;
            if(!transform || interior) for(unsigned channel=0;channel<(destination?4U:3U);channel++) {
                unsigned error=abs((int)((output[y*N+x]>>(channel*8))&255)-(int)((reference[y*N+x]>>(channel*8))&255));
                unsigned tolerance=transform?8:3;
                if(error>tolerance) fprintf(stderr,"mask pixel dest=%u enc=%u luma=%u shape=%u transform=%u tint=%u opacity=%u xy=%d,%d ch=%u error=%u\n",
                    destination,encoding,luminance,shape,transform,tint,opacity,x,y,channel,error);
                assert(error<=tolerance);worst=LV_MAX(worst,error);samples++;
            }
        }
        assert(samples>90);checked+=samples;scenes++;
        memcpy(whole,output,sizeof output);reset();
        for(unsigned half=0;half<2;half++) {
            task.clip_area=layer.buf_area;
            if(half) task.clip_area.x1=138;else task.clip_area.x2=137;
            assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
            assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE || outcome==LV_DRAW_AIC_GE2D_OUTCOME_NOTHING);
        }
        assert(!memcmp(whole,output,sizeof output));
        assert(!memcmp(source,original,sizeof source));
        lv_image_cache_drop(&src);lv_image_cache_drop(&ref_src);lv_image_cache_drop(&mask);
    }
    /* Ordinary IMAGE masks now share the bounded native preparation path;
     * rounded clips and non-normal blends remain software. Color key plus
     * recolor is intentionally tested below as two bounded preparations. */
    {
        lv_draw_buf_t combo_src;
        lv_draw_image_dsc_t combo_d;
        lv_image_colorkey_t combo_key={.low=lv_color_make(255,0,255),
                                       .high=lv_color_make(255,0,255)};
        uint8_t original_combo[8], *p=source;
        memset(source,0,128);
        /* XRGB8888 is stored B,G,R,X by the AIC/LVGL bridge. */
        p[0]=255; p[1]=0; p[2]=255; p[3]=255;
        p[4]=0; p[5]=255; p[6]=0; p[7]=255;
        memcpy(original_combo,source,sizeof original_combo);
        assert(lv_draw_buf_init(&combo_src,2,1,LV_COLOR_FORMAT_XRGB8888,
                                64,source,128)==LV_RESULT_OK);
        lv_draw_image_dsc_init(&combo_d); combo_d.src=&combo_src;
        combo_d.header=combo_src.header; combo_d.colorkey=&combo_key;
        combo_d.recolor=lv_color_make(200,10,50); combo_d.recolor_opa=255;
        reset();
        assert(colorkey_needs_prepare(&combo_src,&combo_d));
        assert(colorkey_prepare(&combo_src,&combo_d));
        combo_d.colorkey=NULL;
        assert(recolor_prepare(&keyed,&combo_d));
        assert(((uint32_t *)keyed.data)[0]>>24==0);
        assert(((uint32_t *)keyed.data)[1]>>24==255);
        assert((((uint32_t *)recolored.data)[1]&0xffffffU)==
               (lv_color_to_u32(combo_d.recolor)&0xffffffU));
        transform_release();
        assert(!memcmp(source,original_combo,sizeof original_combo));
    }
    {
        /* IMAGE preparation order is mask -> key -> recolor.  Keep this
         * contract separate from the transformed LAYER matrix so a future
         * optimization cannot accidentally key the recolored pixels. */
        lv_draw_buf_t combo_src;
        lv_draw_image_dsc_t combo_d;
        lv_image_dsc_t combo_mask;
        lv_image_colorkey_t combo_key={.low=lv_color_make(255,0,255),
                                       .high=lv_color_make(255,0,255)};
        uint8_t combo_pixels[128], combo_mask_pixels[64], combo_original[8];
        memset(combo_pixels,0xa5,sizeof combo_pixels);
        memset(combo_mask_pixels,0,sizeof combo_mask_pixels);
        combo_pixels[0]=255; combo_pixels[1]=0; combo_pixels[2]=255; combo_pixels[3]=255;
        combo_pixels[4]=0; combo_pixels[5]=255; combo_pixels[6]=0; combo_pixels[7]=255;
        combo_mask_pixels[0]=128; combo_mask_pixels[1]=255;
        memcpy(combo_original,combo_pixels,sizeof combo_original);
        assert(lv_draw_buf_init(&combo_src,2,1,LV_COLOR_FORMAT_ARGB8888,
                                64,combo_pixels,sizeof combo_pixels)==LV_RESULT_OK);
        memset(&combo_mask,0,sizeof combo_mask);
        combo_mask.header.magic=LV_IMAGE_HEADER_MAGIC;
        combo_mask.header.cf=LV_COLOR_FORMAT_A8;
        combo_mask.header.w=2; combo_mask.header.h=1; combo_mask.header.stride=64;
        combo_mask.data=combo_mask_pixels; combo_mask.data_size=sizeof combo_mask_pixels;
        lv_draw_image_dsc_init(&combo_d);
        combo_d.src=&combo_src; combo_d.header=combo_src.header;
        combo_d.bitmap_mask_src=&combo_mask;
        combo_d.image_area=(lv_area_t){0,0,1,0};
        combo_d.colorkey=&combo_key;
        combo_d.recolor=lv_color_make(200,10,50); combo_d.recolor_opa=255;
        reset();
        assert(image_mask_prepare(&combo_src,&combo_d,&combo_d.image_area)>0);
        assert(colorkey_needs_prepare(&masked,&combo_d));
        assert(colorkey_prepare(&masked,&combo_d));
        combo_d.colorkey=NULL;
        assert(recolor_prepare(&keyed,&combo_d));
        assert(((uint32_t *)recolored.data)[0]>>24==0);
        assert(((uint32_t *)recolored.data)[1]>>24>=254);
        assert(!memcmp(combo_pixels,combo_original,sizeof combo_original));
        assert(!memcmp(combo_mask_pixels, (uint8_t[64]){128,255,0,0,0}, 5));
        transform_release();
    }
    static const lv_color_format_t image_mask_formats[] = {
        LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888,
        LV_COLOR_FORMAT_XRGB8888, LV_COLOR_FORMAT_ARGB8888,
        LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,
    };
    alpha_background=false;
    dst.header.cf=ref.header.cf=LV_COLOR_FORMAT_XRGB8888;
    layer.color_format=reference_layer.color_format=LV_COLOR_FORMAT_XRGB8888;
    for(unsigned format=0;format<sizeof image_mask_formats/sizeof image_mask_formats[0];format++) {
        lv_color_format_t cf=image_mask_formats[format];
        unsigned bpp=lv_color_format_get_size(cf); bool alpha=cf>=LV_COLOR_FORMAT_ARGB8888;
        assert(lv_draw_buf_init(&src,32,32,cf,128,source,sizeof source)==LV_RESULT_OK);
        if(cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
        for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++) {
            unsigned a=alpha?64+5*x:255, r=32+4*x, g=40+3*y, b=96+2*x;
            uint8_t *p=source+y*128+x*bpp;
            if(cf==LV_COLOR_FORMAT_RGB565) { uint16_t v=(uint16_t)((r>>3)<<11)|((uint16_t)(g>>2)<<5)|(b>>3); memcpy(p,&v,2); }
            else { p[0]=alpha&&cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED?(b*a+127)/255:b;
                   p[1]=alpha&&cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED?(g*a+127)/255:g;
                   p[2]=alpha&&cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED?(r*a+127)/255:r;
                   if(bpp==4) p[3]=a; }
        }
        memcpy(original,source,sizeof source); memcpy(reference_source,source,sizeof source);
        lv_draw_image_dsc_init(&d); d.src=&src; d.header=src.header; d.bitmap_mask_src=&mask;
        d.image_area=task.area; d.opa=128; d.pivot=(lv_point_t){16,16};
        task.type=LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc=&d; task.clip_area=layer.buf_area;
        reset(); lv_draw_aic_ge2d_outcome_t image_outcome;
        assert(lv_draw_aic_ge2d_accepts_image(&task));
        assert(lv_draw_aic_ge2d_image(&task,&image_outcome)==LV_RESULT_OK &&
               image_outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        assert(allocation_calls==1 && frees==1 && !memcmp(source,original,sizeof source));
        for(unsigned i=0;i<N*N;i++) reference[i]=background(i);
        lv_draw_buf_t reference_image;
        assert(lv_draw_buf_init(&reference_image,32,32,
                                cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED?
                                LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:LV_COLOR_FORMAT_ARGB8888,128,
                                image_masked_reference,sizeof image_masked_reference)==LV_RESULT_OK);
        if(cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) reference_image.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
        assert(lv_aic_sw_image_mask_copy(&d,&task.area,&src,&reference_image)==1);
        lv_draw_image_dsc_t sw_d=d; sw_d.src=&reference_image; sw_d.bitmap_mask_src=NULL;
        lv_draw_task_t sw=task; sw.target_layer=&reference_layer; sw.draw_dsc=&sw_d;
        lv_draw_sw_image(&sw,&sw_d,&sw.area);
        for(int y=0;y<N;y++) for(int x=0;x<N;x++)
            for(unsigned channel=0;channel<3;channel++) {
                unsigned error=(unsigned)abs((int)((output[y*N+x]>>(channel*8))&255)-
                                             (int)((reference[y*N+x]>>(channel*8))&255));
                assert(error<=(format==4?8U:3U));
            }
        lv_image_cache_drop(&src); lv_image_cache_drop(&mask);
    }
    /* A mask must not bypass the existing unsupported-effect boundary. */
    alpha_background=true;
    dst.header.cf=ref.header.cf=LV_COLOR_FORMAT_ARGB8888;
    layer.color_format=reference_layer.color_format=LV_COLOR_FORMAT_ARGB8888;
    d.src=&child; d.header=src.header; d.recolor_opa=128;
    d.rotation=333; d.scale_x=264; d.scale_y=384; d.opa=128;
    child.draw_buf=&src; child.buf_area=task.area; child.color_format=src.header.cf;
    task.type=LV_DRAW_TASK_TYPE_LAYER;
    lv_draw_image_dsc_t saved=d;
    lv_image_colorkey_t key={.low=lv_color_make(1,2,3),.high=lv_color_make(1,2,3)};
    for(unsigned effect=0;effect<3;effect++) {
        d=saved;
        if(effect==0) d.clip_radius=4;
        if(effect==1) d.blend_mode=LV_BLEND_MODE_ADDITIVE;
        if(effect==2) { d.colorkey=&key; d.recolor_opa=128; d.bitmap_mask_src=NULL; }
        bool accepted=lv_draw_aic_ge2d_accepts_layer(&task);
        assert(effect==2 ? accepted : !accepted);
        memcpy(source,original,sizeof source);memcpy(reference_source,original,sizeof source);
        task.clip_area=layer.buf_area;reset();lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
        if(effect==2) {
            assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && allocation_calls>=2);
            assert(!memcmp(source,original,sizeof source));
            lv_image_cache_drop(&src);lv_image_cache_drop(&ref_src);lv_image_cache_drop(&mask);
            continue;
        }
        assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE && !event && !allocation_calls);
        for(unsigned i=0;i<N*N;i++) reference[i]=background(i);
        lv_draw_task_t sw=task;sw.target_layer=&reference_layer;
        lv_draw_image_dsc_t sw_d=d;sw_d.src=(const void *)&reference_child;
        sw.draw_dsc=&sw_d;
        lv_draw_sw_layer(&sw,&sw_d,&sw.area);
        assert(!memcmp(output,reference,sizeof output));
        lv_image_cache_drop(&src);lv_image_cache_drop(&ref_src);lv_image_cache_drop(&mask);
    }
    d=saved;
    /* Maximal preparation: alpha target + mask + recolor + two transforms. */
    task.clip_area=layer.buf_area;
    for(unsigned failure=1;failure<=6;failure++) {
        memcpy(source,original,sizeof source);memcpy(reference_source,original,sizeof source);
        reset();if(failure<=5) fail_alloc=failure;else fail_address=true;
        lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
        assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE && !event);
        for(unsigned i=0;i<SCRATCH_SLOTS;i++) assert(!allocated[i]);
        for(unsigned i=0;i<N*N;i++) reference[i]=background(i);
        lv_draw_task_t sw=task;sw.target_layer=&reference_layer;
        lv_draw_image_dsc_t sw_d=d;sw_d.src=&reference_child;sw.draw_dsc=&sw_d;
        lv_draw_sw_layer(&sw,&sw_d,&sw.area);
        assert(!memcmp(output,reference,sizeof output));
        lv_image_cache_drop(&src);lv_image_cache_drop(&ref_src);lv_image_cache_drop(&mask);
    }
    for(unsigned failure=1;failure<=9;failure++) {
        memcpy(source,original,sizeof source);reset();fail_event=failure;
        lv_draw_aic_ge2d_outcome_t outcome;
        lv_result_t status=lv_draw_aic_ge2d_image(&task,&outcome);
        if(status!=LV_RESULT_INVALID) fprintf(stderr,"mask fault=%u event=%u allocations=%u frees=%u outcome=%u\n",failure,event,allocation_calls,frees,outcome);
        assert(status==LV_RESULT_INVALID);
        assert(decoder_quarantined && masked.data && event==failure && !frees);
        for(unsigned i=0;i<SCRATCH_SLOTS;i++) assert(allocated[i]);
        assert(!memcmp(source,original,sizeof source));
        for(unsigned i=0;i<N*N;i++) assert(output[i]==background(i));
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID && event==failure);
        /* Only the synchronous CPU model can prove that this DMA has drained. */
        lv_image_decoder_close(&image_decoder);decoder_quarantined=false;s_blit_failed=false;
        transform_release();lv_image_cache_drop(&src);lv_image_cache_drop(&mask);
    }
    /* A mask wholly outside this partial layer is NOTHING, not SW or DMA. */
    reset();d.image_area=(lv_area_t){0,0,39,39};lv_draw_aic_ge2d_outcome_t outcome;
    assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
    assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_NOTHING && !event && frees==allocation_calls);
    assert(!memcmp(source,original,sizeof source));
    lv_image_cache_drop(&src);lv_image_cache_drop(&mask);d.image_area=task.area;
    /* Invalid, wrong-format, and unresolved masks follow native unmasked policy. */
    for(unsigned invalid=0;invalid<3;invalid++) {
        reset();mask.data_size=invalid==0?1:sizeof mask_pixels;
        mask.header.cf=invalid==1?LV_COLOR_FORMAT_RGB565:LV_COLOR_FORMAT_A8;
        d.bitmap_mask_src=invalid==2?(const void *)"Z:/missing-layer-mask.bin":&mask;
        assert(mask_prepare(&src,&d,&task.area)==1);
        for(unsigned y=0;y<32;y++) assert(!memcmp(masked.data+y*masked.header.stride,source+y*128,128));
        assert(!event);transform_release();lv_image_cache_drop(&mask);
    }
    d.bitmap_mask_src=&mask;mask.data_size=sizeof mask_pixels;mask.header.cf=LV_COLOR_FORMAT_A8;
    /* Bounds, unsupported source storage, budget and scratch address preflight. */
    reset();lv_draw_buf_t bad=src;bad.data_size=1;
    assert(mask_prepare(&bad,&d,&task.area)==-1 && !allocation_calls);
    bad=src;bad.header.cf=LV_COLOR_FORMAT_RGB565;
    assert(mask_prepare(&bad,&d,&task.area)==-1 && !allocation_calls);
    bad=src;bad.header.w=4096;bad.header.h=4096;bad.header.stride=16384;bad.data_size=UINT32_MAX;
    assert(mask_prepare(&bad,&d,&task.area)==-1 && !allocation_calls);
    fail_address=true;assert(mask_prepare(&src,&d,&task.area)==-1 && allocation_calls==frees);
    reset();assert(mask_prepare(&src,&d,&task.area)==1);
    lv_draw_buf_t bad_dst=masked;bad_dst.data_size=1;
    assert(lv_aic_sw_layer_mask_copy(&d,&task.area,&src,&bad_dst)==-1);
    assert(lv_aic_sw_layer_mask_copy(&d,&task.area,&src,&src)==-1);
    lv_area_t wrong=task.area;wrong.x2--;
    assert(lv_aic_sw_layer_mask_copy(&d,&wrong,&src,&masked)==-1);
    transform_release();lv_image_cache_drop(&mask);
    alpha_background=false;
    printf("PASS masked LAYER %u scenes, %u native SW channel comparisons (max %u), immutable source/mask, exact split refresh, 6 preparation failures and 9 DMA faults\n",scenes,checked,worst);
}
