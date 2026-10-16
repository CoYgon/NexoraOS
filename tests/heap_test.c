#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "heap.h"
int main(void) {
    heap_init();
    assert(heap_capacity() >= 1024u*1024u);
    assert(heap_used()==0);
    void *a=nexora_malloc(1), *b=nexora_calloc(64,4);
    assert(a && b && ((uintptr_t)a%16u)==0 && ((uintptr_t)b%16u)==0);
    for(unsigned i=0;i<256;i++)assert(((uint8_t*)b)[i]==0);
    ((uint8_t*)a)[0]=0x5a;
    a=nexora_realloc(a,128); assert(a && ((uint8_t*)a)[0]==0x5a);
    nexora_free(a); nexora_free(b); nexora_free(b); assert(heap_used()==0);
    void *whole=nexora_malloc(heap_capacity()/2); assert(whole); nexora_free(whole);
    puts("NexoraOS heap tests passed");
    return 0;
}
