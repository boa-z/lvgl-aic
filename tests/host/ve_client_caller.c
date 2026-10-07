/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdatomic.h>
#include <aic_osal.h>
extern int ve_get_client(void);
extern void test_ve_put_client(void);
extern atomic_int inside,completed;
/* Intentionally mirrors SDK's unchecked call, in a separate translation unit
 * so the host link also has to redirect its unresolved symbol reference. */
void test_ve_decode(void)
{
    (void)ve_get_client();
    assert(atomic_fetch_add(&inside,1)==0);
    aicos_msleep(2);
    assert(atomic_fetch_sub(&inside,1)==1);
    atomic_fetch_add(&completed,1);
    test_ve_put_client();
}
