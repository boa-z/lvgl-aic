/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_fake_fs.h"
#include "lv_aic_fake_image.h"
#include "lvgl.h"
#include <string.h>

/* LVGL 9.6 opens FILE sources before decoder info callbacks. Wrap only the
 * application's existing L drive; all real handles retain its original ABI.
 * Owner-thread lifecycle matches decoder init/deinit, no SDK/global FS patch. */
static lv_fs_drv_t *wrapped;
static lv_fs_drv_t original;
static unsigned readers;
static char fake_handle;

static void *fake_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    char full[48] = "L:";
    size_t n = 0;
    lv_aic_fake_image_t image;
    while (n < sizeof(full) - 3 && path[n]) n++;
    if (n < sizeof(full) - 3) {
        memcpy(full + 2, path, n + 1);
        if (lv_aic_fake_image_parse(full, &image)) {
            if (mode != LV_FS_MODE_RD) return NULL;
            readers++;
            return &fake_handle;
        }
    }
    return original.open_cb(drv, path, mode);
}
static lv_fs_res_t fake_close(lv_fs_drv_t *drv, void *file)
{
    if (file == &fake_handle) {
        if (!readers) return LV_FS_RES_INV_PARAM;
        readers--;
        return LV_FS_RES_OK;
    }
    return original.close_cb(drv, file);
}
static lv_fs_res_t fake_read(lv_fs_drv_t *drv, void *file, void *buf, uint32_t count, uint32_t *read)
{
    if (file == &fake_handle) { *read = 0; return LV_FS_RES_OK; }
    return original.read_cb(drv, file, buf, count, read);
}
static lv_fs_res_t fake_write(lv_fs_drv_t *drv, void *file, const void *buf, uint32_t count, uint32_t *written)
{
    if (file == &fake_handle) { *written = 0; return LV_FS_RES_DENIED; }
    return original.write_cb ? original.write_cb(drv, file, buf, count, written) : LV_FS_RES_NOT_IMP;
}
static lv_fs_res_t fake_seek(lv_fs_drv_t *drv, void *file, uint32_t pos, lv_fs_whence_t whence)
{
    if (file == &fake_handle) return pos == 0 ? LV_FS_RES_OK : LV_FS_RES_INV_PARAM;
    return original.seek_cb(drv, file, pos, whence);
}
static lv_fs_res_t fake_tell(lv_fs_drv_t *drv, void *file, uint32_t *pos)
{
    if (file == &fake_handle) { *pos = 0; return LV_FS_RES_OK; }
    return original.tell_cb(drv, file, pos);
}
bool lv_aic_fake_fs_install(void)
{
    if (wrapped) return false;
    lv_fs_drv_t *drv = lv_fs_get_drv('L');
    if (!drv || !drv->open_cb || !drv->close_cb || !drv->read_cb ||
        !drv->seek_cb || !drv->tell_cb || drv->cache_size == LV_FS_CACHE_FROM_BUFFER) return false;
    original = *drv;
    wrapped = drv;
    drv->open_cb = fake_open; drv->close_cb = fake_close;
    drv->read_cb = fake_read; drv->write_cb = fake_write;
    drv->seek_cb = fake_seek; drv->tell_cb = fake_tell;
    return true;
}
bool lv_aic_fake_fs_idle(void) { return readers == 0; }
void lv_aic_fake_fs_restore(void)
{
    if (wrapped && !readers) {
        wrapped->open_cb = original.open_cb; wrapped->close_cb = original.close_cb;
        wrapped->read_cb = original.read_cb; wrapped->write_cb = original.write_cb;
        wrapped->seek_cb = original.seek_cb; wrapped->tell_cb = original.tell_cb;
        wrapped = NULL;
    }
}
