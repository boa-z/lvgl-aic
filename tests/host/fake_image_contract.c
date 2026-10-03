/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lv_aic_fake_image.h"

int main(void)
{
    lv_aic_fake_image_t image;
    const char *valid = "L:/320x240_1_80402010.fake";
    assert(lv_aic_fake_image_parse(valid, &image));
    assert(image.width == 320 && image.height == 240 && image.blend && image.argb == 0x80402010);
    assert(lv_aic_fake_image_parse("s:/4096x2048_0_00000000.fake", &image));
    assert(image.width == 4096 && image.height == 2048 && !image.blend && !image.argb);
    assert(lv_aic_fake_image_parse("A:/1x1_1_FfAaBbCc.fake", &image));
    assert(image.argb == 0xffaabbcc);
    const char *invalid[] = {
        "", "L", "L:", "L:/", "1:/1x1_0_00000000.fake",
        "L:/0x1_0_00000000.fake", "L:/-1x1_0_00000000.fake",
        "L:/1x0_0_00000000.fake", "L:/4097x1_0_00000000.fake",
        "L:/4096x2049_0_00000000.fake", "L:/999999999999x1_0_00000000.fake",
        "L:/1X1_0_00000000.fake", "L:/1x1_2_00000000.fake",
        "L:/1x1_10_00000000.fake", "L:/1x1_0_0000000.fake",
        "L:/1x1_0_000000000.fake", "L:/1x1_0_0000000g.fake",
        "L:/1x1_0_00000000.fake/extra", "L:/1x1_0_00000000.fake.png"
    };
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        memset(&image, 0xa5, sizeof(image));
        unsigned char before[sizeof(image)];
        memcpy(before, &image, sizeof(image));
        assert(!lv_aic_fake_image_parse(invalid[i], &image));
        assert(memcmp(before, &image, sizeof(image)) == 0);
    }
    /* Every truncation, including inside the suffix, is rejected. */
    char truncated[64];
    for (size_t n = 0; n < strlen(valid); n++) {
        memcpy(truncated, valid, n); truncated[n] = 0;
        assert(!lv_aic_fake_image_parse(truncated, &image));
    }
    for (unsigned alpha = 0; alpha <= 255; alpha++) {
        snprintf(truncated, sizeof(truncated), "L:/32x32_0_%08x.fake", (alpha << 24) | 0x123456);
        assert(lv_aic_fake_image_parse(truncated, &image));
        assert(image.argb == ((alpha << 24) | 0x123456) && !image.blend);
    }
    assert(!lv_aic_fake_image_parse(NULL, &image));
    assert(!lv_aic_fake_image_parse(valid, NULL));
    return 0;
}
