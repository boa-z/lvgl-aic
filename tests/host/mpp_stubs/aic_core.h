#pragma once
#include <stdint.h>
/* Match SDK aic_common.h command encoding for real framebuffer UAPI tests. */
#ifndef _IOR
#define _IOR(x,y,z) (((x)<<8)|(y))
#endif
#ifndef _IOW
#define _IOW(x,y,z) (((x)<<8)|(y))
#endif
#ifndef _IOWR
#define _IOWR(x,y,z) (((x)<<8)|(y))
#endif
