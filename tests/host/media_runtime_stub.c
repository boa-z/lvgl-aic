/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <pthread.h>
static unsigned refs;
static pthread_t owner;
int ve_open_device(void)
{
    assert(!refs);owner=pthread_self();refs++;return 0;
}
void ve_close_device(void)
{ assert(refs==1 && pthread_equal(owner,pthread_self()));refs--; }
unsigned test_media_runtime_refs(void) { return refs; }
