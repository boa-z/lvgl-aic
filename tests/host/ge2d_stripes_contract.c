/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_draw_aic_ge2d_stripes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned events,fail_event;
int mpp_ge_bitblt(struct mpp_ge *ge,struct ge_bitblt *b)
{ assert(ge && b);return ++events==fail_event?-1:0; }
int mpp_ge_emit(struct mpp_ge *ge) { assert(ge);return ++events==fail_event?-1:0; }
int mpp_ge_sync(struct mpp_ge *ge) { assert(ge);return ++events==fail_event?-1:0; }

int main(void)
{
    const int widths[]={32,33,47,62,63,65,127,1024,4096};
    const int steps[]={59393,59400,60000,63488,65534,65535};
    const int phases[]={0,1,32768,65535};
    unsigned probes=0;
    struct ge_bitblt b={0};
    b.src_buf.buf_type=b.dst_buf.buf_type=MPP_PHY_ADDR;
    b.src_buf.format=b.dst_buf.format=MPP_FMT_ARGB_8888;
    b.src_buf.size=(struct mpp_size){8192,128};b.dst_buf.size=(struct mpp_size){8192,8192};
    b.src_buf.phy_addr[0]=0x40000000;b.dst_buf.phy_addr[0]=0x44000000;
    b.src_buf.stride[0]=b.dst_buf.stride[0]=8192*4;
    b.src_buf.crop_en=b.dst_buf.crop_en=1;
    b.src_buf.flags=MPP_BUF_IS_PREMULTIPLY;
    b.ctrl.alpha_en=1;b.ctrl.alpha_rules=GE_PD_NONE;b.ctrl.src_alpha_mode=2;b.ctrl.src_global_alpha=128;
    b.scale_phase.scale_phase_en=1;b.scale_phase.scaler_en=1;b.scale_phase.channel_num=1;
    b.scale_phase.dy_16[0]=32768;b.scale_phase.v_phase_16[0]=23000;
    for(unsigned rotation=0;rotation<4;rotation++)
    for(unsigned w=0;w<sizeof widths/sizeof widths[0];w++)
    for(unsigned s=0;s<sizeof steps/sizeof steps[0];s++)
    for(unsigned p=0;p<sizeof phases/sizeof phases[0];p++) {
        int width=widths[w],step=steps[s],phase=phases[p];
        b.ctrl.flags=rotation;b.scale_phase.dx_16[0]=step;b.scale_phase.h_phase_16[0]=phase;
        int64_t last=phase+(int64_t)(width-1)*step;
        b.src_buf.crop=(struct mpp_rect){13,7,(int)((last+65535)/65536+2),32};
        b.dst_buf.crop=(struct mpp_rect){29,37,rotation&1?48:width,rotation&1?width:48};
        struct ge_bitblt saved=b;
        unsigned count=0;assert(lv_aic_ge2d_stripe_count(&b,&count));
        assert(count>=2 && count<=133);
        bool seen[4096]={false};
        for(unsigned i=0;i<count;i++) {
            struct ge_bitblt c;assert(lv_aic_ge2d_stripe(&b,i,&c));
            assert(!memcmp(&b,&saved,sizeof b));
            assert(!memcmp(&c.ctrl,&b.ctrl,sizeof b.ctrl));
            assert(c.src_buf.flags==b.src_buf.flags && c.src_buf.phy_addr[0]==b.src_buf.phy_addr[0]);
            assert(c.dst_buf.stride[0]==b.dst_buf.stride[0] && c.dst_buf.phy_addr[0]==b.dst_buf.phy_addr[0]);
            int n=rotation&1?c.dst_buf.crop.height:c.dst_buf.crop.width;
            assert(n>=16 && n<=31 && c.src_buf.crop.width>=4);
            assert(c.src_buf.crop.x>=b.src_buf.crop.x &&
                   c.src_buf.crop.x+c.src_buf.crop.width<=b.src_buf.crop.x+b.src_buf.crop.width);
            assert(c.src_buf.crop.y==b.src_buf.crop.y && c.src_buf.crop.height==b.src_buf.crop.height);
            assert(c.scale_phase.dy_16[0]==b.scale_phase.dy_16[0] &&
                   c.scale_phase.v_phase_16[0]==b.scale_phase.v_phase_16[0]);
            for(int j=0;j<n;j++) {
                /* Physical destination order and original inverse map are
                 * independent of the production partition calculation. */
                bool reverse=rotation==MPP_ROTATION_180 || rotation==MPP_ROTATION_270;
                int physical=(rotation&1?c.dst_buf.crop.y-b.dst_buf.crop.y:c.dst_buf.crop.x-b.dst_buf.crop.x)+
                             (reverse?n-1-j:j);
                assert(physical>=0 && physical<width && !seen[physical]);seen[physical]=true;
                int source_index=reverse?width-1-physical:physical;
                int64_t expected=(int64_t)b.src_buf.crop.x*65536+phase+(int64_t)source_index*step;
                int64_t got=(int64_t)c.src_buf.crop.x*65536+c.scale_phase.h_phase_16[0]+(int64_t)j*step;
                assert(got==expected);
                assert(got<=((int64_t)c.src_buf.crop.x+c.src_buf.crop.width-1)*65536);
            }
        }
        for(int i=0;i<width;i++) assert(seen[i]);
        events=fail_event=0;
        assert(lv_aic_ge2d_stripe_run((void *)(uintptr_t)1,&b)==1 && events==3*count);
        assert(!memcmp(&b,&saved,sizeof b));probes++;
    }
    /* A later strip cannot fit: the public runner must send NOTHING. */
    b.src_buf.crop.width-=32;
    unsigned count=123;events=0;
    assert(!lv_aic_ge2d_stripe_count(&b,&count) && count==123);
    assert(lv_aic_ge2d_stripe_run((void *)(uintptr_t)1,&b)==0 && !events);
    b.src_buf.crop.width+=32;
    assert(lv_aic_ge2d_stripe_count(&b,&count));
    for(unsigned failure=1;failure<=count*3;failure++) {
        events=0;fail_event=failure;
        assert(lv_aic_ge2d_stripe_run((void *)(uintptr_t)1,&b)==-1 && events==failure);
    }
    assert(!lv_aic_ge2d_stripe_count(NULL,&count));
    assert(!lv_aic_ge2d_stripe_count(&b,NULL));
    struct ge_bitblt command;
    assert(!lv_aic_ge2d_stripe(&b,count,&command));
    b.scale_phase.dx_16[0]=65536;
    assert(lv_aic_ge2d_stripe_count(&b,&count) && count==1);
    assert(lv_aic_ge2d_stripe(&b,0,&command) && !memcmp(&command,&b,sizeof b));
    printf("PASS %u orthogonal strip plans: exact inverse phases, complete nonoverlapping coverage, all-command preflight and submission failures\n",probes);
    return 0;
}
