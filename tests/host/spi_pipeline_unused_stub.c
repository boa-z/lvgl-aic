/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_pipeline.h"
#include <assert.h>
lv_aic_spi_pipeline_t *lv_aic_spi_pipeline_create(lv_aic_spi_session_t *s,uint32_t stack,uint32_t priority)
{ (void)s;(void)stack;(void)priority;assert(!"unexpected pipeline create");return NULL; }
lv_aic_spi_result_t lv_aic_spi_pipeline_submit(lv_aic_spi_pipeline_t *p,
    const lv_aic_spi_rgb565_frame_t *f,unsigned degrees,void *cookie)
{ (void)p;(void)f;(void)degrees;(void)cookie;assert(!"unexpected pipeline submit");return LV_AIC_SPI_INVALID; }
bool lv_aic_spi_pipeline_take(lv_aic_spi_pipeline_t *p,lv_aic_spi_event_t *event)
{ (void)p;(void)event;assert(!"unexpected pipeline take");return false; }
void lv_aic_spi_pipeline_stop(lv_aic_spi_pipeline_t *p)
{ (void)p;assert(!"unexpected pipeline stop"); }
bool lv_aic_spi_pipeline_close(lv_aic_spi_pipeline_t *p)
{ (void)p;assert(!"unexpected pipeline close");return false; }
