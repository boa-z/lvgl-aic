# SPDX-License-Identifier: Apache-2.0
"""Compile actual normal/CMDQ alpha helpers; no hardware pixel claim."""
import re
import runpy
import subprocess
import sys
import tempfile
from pathlib import Path
sdk=Path(sys.argv[1]);cc=sys.argv[2]
root=Path(__file__).resolve().parents[2]
stage=runpy.run_path(str(root/'tools/sdk/stage_sw_premult.py'))
source=(root.parent/'lvgl/src/draw/sw/lv_draw_sw_img.c').read_text(encoding='utf-8')
try:
    stage['corrected'](source+'\n/* changed upstream */\n')
except ValueError:
    pass
else:
    raise AssertionError('upstream source drift was accepted')

def function(text,name):
    match=re.search(r'(?:static )?(?:void|int) '+name+r'\([^;]*?\)\s*\{',text)
    assert match,name
    start=match.start();cursor=match.end();depth=1
    while depth:
        depth+=(text[cursor]=='{')-(text[cursor]=='}');cursor+=1
    return text[start:cursor]

prefix=r"""
#include <assert.h>
#include <string.h>
#include "aic_drv_ge.h"
struct ge_data {
    unsigned src_alpha_coef,dst_alpha_coef,src_premul_en,src_de_premul_en,dst_de_premul_en,out_premul_en;
};
#define aic_ge_data ge_data
"""
suffix=r"""
int main(void) {
    for(unsigned opacity=0;opacity<=255;opacity++) {
        struct ge_data data;
        memset(&data,0xa5,sizeof data);
        struct ge_ctrl ctrl={.alpha_en=1,.alpha_rules=GE_PD_NONE,
            .src_alpha_mode=opacity==255?0:2,.src_global_alpha=opacity};
        set_alpha_rules(&data,ctrl.alpha_rules);
        assert(set_premuliply(&data,MPP_FMT_ARGB_8888,MPP_FMT_ARGB_8888,1,0,0,&ctrl)==0);
        assert(data.src_premul_en==0 && data.dst_de_premul_en==0 && data.out_premul_en==0);
        assert(data.dst_alpha_coef==3);
        if(opacity==255) assert(data.src_alpha_coef==1 && data.src_de_premul_en==0);
        else assert(data.src_alpha_coef==2 && data.src_de_premul_en==1);
        /* Omitting the source flag must give a different datapath. */
        set_alpha_rules(&data,ctrl.alpha_rules);
        assert(set_premuliply(&data,MPP_FMT_ARGB_8888,MPP_FMT_ARGB_8888,0,0,0,&ctrl)==0);
        assert(data.src_de_premul_en==0 && data.src_premul_en==(opacity==255));
    }
    /* Multipass preparation: SRC into a premultiplied destination converts
     * straight channels exactly once; the following raw scale converts none. */
    struct ge_data data={0};
    struct ge_ctrl ctrl={.alpha_en=1,.alpha_rules=GE_PD_SRC,.src_alpha_mode=0,.src_global_alpha=255};
    set_alpha_rules(&data,ctrl.alpha_rules);
    assert(set_premuliply(&data,MPP_FMT_ARGB_8888,MPP_FMT_ARGB_8888,0,1,0,&ctrl)==0);
    assert(data.src_alpha_coef==1 && data.dst_alpha_coef==0);
    assert(data.src_premul_en==0 && data.src_de_premul_en==0 && data.out_premul_en==1);
    ctrl.alpha_en=0;
    assert(set_premuliply(&data,MPP_FMT_ARGB_8888,MPP_FMT_ARGB_8888,0,0,0,&ctrl)==0);
    assert(data.src_premul_en==0 && data.src_de_premul_en==0 && data.out_premul_en==0);
    /* Straight-ARGB staging: preserve raw channels/alpha for the native
     * final blend, including sources whose stored bytes are premultiplied. */
    ctrl.alpha_en=0;ctrl.src_alpha_mode=0;ctrl.src_global_alpha=255;
    ctrl.alpha_rules=GE_PD_NONE;
    set_alpha_rules(&data,ctrl.alpha_rules);
    assert(set_premuliply(&data,MPP_FMT_ARGB_8888,MPP_FMT_ARGB_8888,0,0,0,&ctrl)==0);
    assert(!data.src_premul_en && !data.src_de_premul_en && !data.dst_de_premul_en && !data.out_premul_en);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='aic-premult-') as temporary:
    for name,relative in [('cmdq','packages/artinchip/mpp/ge/cmdq_ops.c'),
                          ('normal','bsp/artinchip/hal/ge/hal_ge_normal.c')]:
        original=(sdk/relative).read_text(encoding='utf-8')
        path=Path(temporary)/(name+'.c');binary=path.with_suffix('.exe')
        path.write_text(prefix+function(original,'set_alpha_rules')+'\n'+function(original,'set_premuliply')+suffix)
        subprocess.run([cc,'-std=c99','-Wall','-Wextra','-Werror',
            '-I'+str(sdk/'bsp/artinchip/include/drv'),'-I'+str(sdk/'bsp/artinchip/include/uapi'),
            str(path),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
        assert (sdk/relative).read_text(encoding='utf-8')==original
print('PASS real SDK normal/CMDQ premultiplied control states at 256 opacities; SW source drift rejected')
