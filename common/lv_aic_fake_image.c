/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_fake_image.h"
#include <string.h>

static bool dimension(const char **cursor, uint16_t *value, char separator)
{
    const char *p = *cursor;
    uint32_t n = 0;
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 4) return false;
        n = n * 10 + (unsigned)(*p++ - '0');
    }
    if (!digits || !n || n > 4096 || *p != separator) return false;
    *cursor = p + 1;
    *value = (uint16_t)n;
    return true;
}

bool lv_aic_fake_image_parse(const char *path, lv_aic_fake_image_t *image)
{
    lv_aic_fake_image_t parsed = {0};
    if (!path || !image) return false;
    if (!((*path >= 'A' && *path <= 'Z') || (*path >= 'a' && *path <= 'z'))) return false;
    path++;
    if (*path++ != ':') return false;
    if (*path++ != '/') return false;
    if (!dimension(&path, &parsed.width, 'x') ||
        !dimension(&path, &parsed.height, '_') ||
        (uint32_t)parsed.width * parsed.height > 8U * 1024U * 1024U) return false;
    if (*path != '0' && *path != '1') return false;
    parsed.blend = *path++ == '1';
    if (*path++ != '_') return false;
    for (unsigned i = 0; i < 8; i++) {
        unsigned digit;
        char c = *path++;
        if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (unsigned)(c - 'a') + 10;
        else if (c >= 'A' && c <= 'F') digit = (unsigned)(c - 'A') + 10;
        else return false;
        parsed.argb = (parsed.argb << 4) | digit;
    }
    if (strcmp(path, ".fake") != 0) return false;
    *image = parsed;
    return true;
}
