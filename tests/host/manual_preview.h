/* SPDX-License-Identifier: Apache-2.0
 * Host-only software reference: never emulate GE or MPP success. */
#ifndef AIC_MANUAL_PREVIEW_H
#define AIC_MANUAL_PREVIEW_H
static const char *fixture_root;
static void *preview_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    if (mode != LV_FS_MODE_RD) return NULL;
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *source = !strcmp(name, "a.jpg") ? "aic_160x120.jpg" :
                         !strcmp(name, "b.png") ? "basn2c08.png" :
                         !strcmp(name, "c.png") ? "basn6a08.png" : NULL;
    if (!source) return NULL;
    char full[1024];
    int len = snprintf(full, sizeof(full), "%s/%s", fixture_root, source);
    if (len < 0 || (size_t)len >= sizeof(full)) return NULL;
    return fopen(full, "rb");
}
static lv_fs_res_t preview_close(lv_fs_drv_t *drv, void *file)
{
    (void)drv;
    return fclose(file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static lv_fs_res_t preview_read(lv_fs_drv_t *drv, void *file, void *buf, uint32_t size, uint32_t *read)
{
    (void)drv;
    *read = (uint32_t)fread(buf, 1, size, file);
    return ferror(file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static lv_fs_res_t preview_seek(lv_fs_drv_t *drv, void *file, uint32_t pos, lv_fs_whence_t where)
{
    (void)drv;
    int whence = where == LV_FS_SEEK_SET ? SEEK_SET : where == LV_FS_SEEK_CUR ? SEEK_CUR : SEEK_END;
    return fseek(file, (long)pos, whence) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static lv_fs_res_t preview_tell(lv_fs_drv_t *drv, void *file, uint32_t *pos)
{
    (void)drv;
    long offset = ftell(file);
    if (offset < 0) return LV_FS_RES_FS_ERR;
    *pos = (uint32_t)offset;
    return LV_FS_RES_OK;
}
static void preview_fs(const char *root)
{
    fixture_root = root;
    lv_fs_drv_t *drv = lv_fs_get_drv('L');
    assert(drv);
    drv->cache_size = 0;
    drv->open_cb = preview_open; drv->close_cb = preview_close;
    drv->read_cb = preview_read; drv->seek_cb = preview_seek; drv->tell_cb = preview_tell;
    const char *paths[] = {"L:/data/mpp_test/a.jpg", "L:/data/mpp_test/b.png", "L:/data/mpp_test/c.png"};
    for (unsigned i = 0; i < 3; i++) {
        lv_image_header_t header;
        assert(lv_image_decoder_get_info(paths[i], &header) == LV_RESULT_OK);
        assert(header.w == (i ? 32 : 160) && header.h == (i ? 32 : 120));
    }
}
static void preview_save(const char *directory, int page, const uint8_t *pixels)
{
    char path[1024];
    int len = snprintf(path, sizeof(path), "%s/page-%d.ppm", directory, page);
    assert(len > 0 && (size_t)len < sizeof(path));
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs("P6\n800 480\n255\n", file);
    for (unsigned i = 0; i < 800 * 480; i++) {
        unsigned pixel = pixels[2 * i] | (pixels[2 * i + 1] << 8);
        fputc(((pixel >> 11) & 31) * 255 / 31, file);
        fputc(((pixel >> 5) & 63) * 255 / 63, file);
        fputc((pixel & 31) * 255 / 31, file);
    }
    assert(!ferror(file));
    assert(!fclose(file));
}
#endif
