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

enum { N = 96 };
static uint8_t source[64*256];
static uint32_t output[N*N], whole[N*N];
static void *allocated[2], *bases[2];
static size_t sizes[2];
static unsigned allocation_calls, frees, event, fail_event, fail_alloc, copies, scales, rotations;
static bool fail_address;
static const uint32_t colors[] = {0x00ff00ff,0x40d04020,0x8020b050,0xff9050c0};

void *aicos_malloc_align(unsigned int type,size_t bytes,size_t align)
{
    assert(type == MEM_CMA && align == 64);
    if(++allocation_calls == fail_alloc) return NULL;
    for(unsigned i=0;i<2;i++) if(!allocated[i]) {
        bases[i] = malloc(bytes+63); assert(bases[i]);
        allocated[i] = (void *)(((uintptr_t)bases[i]+63)&~(uintptr_t)63);
        sizes[i] = bytes; memset(allocated[i],0xa5,bytes); return allocated[i];
    }
    assert(false); return NULL;
}
void aicos_free_align(unsigned int type,void *p)
{
    assert(type == MEM_CMA);
    for(unsigned i=0;i<2;i++) if(p==allocated[i]) {
        free(bases[i]); bases[i]=allocated[i]=NULL; frees++; return;
    }
    assert(false);
}
static uint8_t *address(uint32_t a)
{
    if(a==(uint32_t)(uintptr_t)source) return source;
    if(a==(uint32_t)(uintptr_t)output) return (uint8_t *)output;
    for(unsigned i=0;i<2;i++) if(allocated[i] && a==(uint32_t)(uintptr_t)allocated[i]) return allocated[i];
    assert(false); return NULL;
}
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{
    if(!b || !b->data) return false;
    if(b->data==source || b->data==(uint8_t *)output) return true;
    for(unsigned i=0;i<2;i++) if(b->data==allocated[i]) return !fail_address && b->data_size<=sizes[i];
    return false;
}
bool lv_draw_aic_ge2d_dst_format_supported(lv_color_format_t cf) { return lv_aic_pixel_format_is_ge2d_dst(cf); }
void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *b,const lv_area_t *a) { assert(b && a); }
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b,const lv_area_t *a) { assert(b && a); }
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
    (void)g; if(++event==fail_event) return -1;
    bool scale=b->scale_phase.scale_phase_en;
    bool direct=b->dst_buf.phy_addr[0]==(uint32_t)(uintptr_t)output;
    struct mpp_rect r=b->dst_buf.crop_en?b->dst_buf.crop:(struct mpp_rect){0,0,b->dst_buf.size.width,b->dst_buf.size.height};
    if(scale) {
        scales++; if(!direct) assert(!b->ctrl.alpha_en && !b->src_buf.flags && !b->dst_buf.flags);
        assert(b->scale_phase.scaler_en && b->scale_phase.channel_num==1);
    } else {
        copies++; assert(r.x==2 && r.y==2 && r.width==32 && r.height==32);
        assert(b->src_buf.flags==0);
        if(b->ctrl.alpha_en) assert(b->ctrl.alpha_rules==GE_PD_SRC && b->dst_buf.flags==MPP_BUF_IS_PREMULTIPLY);
    }
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
        }
        else {
            v=read_pixel(&b->src_buf,x,y);
            if(b->dst_buf.flags & MPP_BUF_IS_PREMULTIPLY) {
                unsigned a=v>>24;uint32_t p=a<<24;
                for(unsigned k=0;k<3;k++) p|=((((v>>(k*8))&255)*a+127)/255)<<(k*8);
                v=p;
            }
        }
        write_pixel(&b->dst_buf,r.x+x,r.y+y,v);
    }
    return 0;
}
int mpp_ge_rotate(struct mpp_ge *g,struct ge_rotation *r)
{
    (void)g;if(++event==fail_event) return -1;rotations++;
    assert(r->src_buf.flags & MPP_BUF_IS_PREMULTIPLY);
    assert(r->ctrl.alpha_en && r->ctrl.src_alpha_mode==(r->ctrl.src_global_alpha==255?0U:2U));
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
        write_pixel(&r->dst_buf,xx,yy,out);
    }
    return 0;
}
static void reset(void)
{
    assert(!allocated[0] && !allocated[1]);
    allocation_calls=frees=event=fail_event=fail_alloc=copies=scales=rotations=0;fail_address=false;
    for(unsigned i=0;i<N*N;i++) output[i]=0xff102030;
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
int main(void)
{
    lv_init();g_ge2d_dev=mpp_ge_open();g_ge2d_ready=true;geometry();
    lv_draw_buf_t src,dst;lv_layer_t layer={0},child={0};lv_draw_task_t task={0};
    assert(lv_draw_buf_init(&dst,N,N,LV_COLOR_FORMAT_ARGB8888,N*4,output,sizeof output)==LV_RESULT_OK);
    layer.draw_buf=&dst;layer.color_format=LV_COLOR_FORMAT_ARGB8888;layer.buf_area=(lv_area_t){90,190,90+N-1,190+N-1};
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
    reset();lv_deinit();
    printf("PASS %u direct striped scenes, %u independent ramp pixels, exact partial refresh and late-strip quarantine\n",direct_scenes,direct_pixels);
    printf("PASS %u multipass IMAGE/LAYER scenes, %u independent interior pixels, partial refresh, allocation/address failure and 12 DMA failure points\n",scenes,pixels);
    return 0;
}
