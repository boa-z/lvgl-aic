# SPDX-License-Identifier: Apache-2.0
"""Compile the actual SDK gradient builder before/after app correction."""
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("stage",root/"tools/sdk/stage_ge_cmdq.py")
stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
sdk=Path(sys.argv[1]);cc=sys.argv[2]
original=(sdk/stage.SOURCE).read_text(encoding="utf-8")
try:
    stage.corrected(original+"\n/* unexpected update */\n")
    raise AssertionError("changed SDK accepted")
except ValueError:
    pass
prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ge_reg.h"
struct cmd_queue { unsigned words[6]; } queue;
struct mpp_ge { void *dev_fd; } device;
static struct cmd_queue *to_cmdq(struct mpp_ge *ge) { (void)ge;return &queue; }
static unsigned *get_cmd_buf(struct cmd_queue *q,void *fd) { (void)fd;return q->words+1; }
static void update_cmd_group(struct cmd_queue *q,unsigned offset,unsigned count,unsigned end)
{ assert(q==&queue && offset==0x30 && count==4 && end==0); }
"""
suffix = r"""
int main(void) {
    const unsigned lengths[]={1,2,8,17,4096};
    for(unsigned i=0;i<5;i++) for(unsigned direction=0;direction<2;direction++)
    for(unsigned reverse=0;reverse<2;reverse++) {
        unsigned start=reverse?0xe090e098:0x40204060,end=reverse?0x40204060:0xe090e098;
        memset(queue.words,0xa5,sizeof(queue.words));
        update_gradient_cmd(&device,direction?11:lengths[i],direction?lengths[i]:11,start,end,direction);
        assert(queue.words[0]==0xa5a5a5a5U && queue.words[5]==0xa5a5a5a5U);
        for(unsigned channel=0;channel<4;channel++) {
            unsigned shift=24-channel*8;
            int64_t delta=(int)((end>>shift)&255)-(int)((start>>shift)&255);
            int64_t step=lengths[i]>1?delta*65536/(lengths[i]-1):0;
            assert(queue.words[channel+1]==((uint32_t)step&0x1ffffff));
        }
    }
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="aic-ge-") as tmp:
    tmp=Path(tmp)
    generated=Path(stage.generate(sdk,tmp/"patched.c"))
    for patched,text in ((False,original),(True,generated.read_text(encoding="utf-8"))):
        begin=text.index("static void update_gradient_cmd(" if patched else "void update_gradient_cmd(")
        end=text.index("static void update_rot1_cmd(",begin)
        source=tmp/("fixed.c" if patched else "original.c")
        source.write_text(prefix+text[begin:end]+suffix,encoding="utf-8")
        binary=source.with_suffix(".exe")
        subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror","-I"+str(sdk/"packages/artinchip/mpp/ge/include"),str(source),"-o",str(binary)],check=True)
        result=subprocess.run([str(binary)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        assert (result.returncode==0)==patched, result.stderr.decode(errors="replace")
    assert (sdk/stage.SOURCE).read_text(encoding="utf-8")==original
print("PASS original defect detected; corrected ARGB command words and source drift guard")
