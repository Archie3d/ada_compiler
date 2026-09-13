/* Instrument allocation only in this test's runtime copy. This verifies actual
   frees, including ownership transfer failure, without a production debug API. */
#include <stdlib.h>
#include <stdio.h>
static int liveAllocations;
static int failNext;
static void* testMalloc(size_t size)
{
    if (failNext) {
        failNext = 0;
        return NULL;
    }
    void* result = malloc(size);
    if (result != NULL) {
        ++liveAllocations;
    }
    return result;
}
static void* testCalloc(size_t count, size_t size)
{
    void* result = calloc(count, size);
    if (result != NULL) {
        ++liveAllocations;
    }
    return result;
}
static void testFree(void* pointer)
{
    if (pointer != NULL) {
        --liveAllocations;
    }
    free(pointer);
}
#define malloc testMalloc
#define calloc testCalloc
#define free testFree
#include "../runtime/adart.c"
#undef malloc
#undef calloc
#undef free
int __ada_exception;
const char* __ada_exception_name;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "line %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
    void* owner = NULL;
    char* outer = __ada_array_local(&owner, 1, 20, 1);
    CHECK(outer != NULL && liveAllocations == 2);
    outer[0] = 'x';
    void* mark = owner;
    for (int i = 0; i < 1000; ++i) {
        CHECK(__ada_array_local(&owner, 5, 4, 1) != NULL);
        CHECK(__ada_array_local(&owner, 1, 100, 8) != NULL);
        __ada_array_rewind(&owner, mark);
        CHECK(owner == mark && liveAllocations == 2 && outer[0] == 'x');
    }
    void* transfer = testMalloc(100);
    __ada_array_adopt(&owner, transfer);
    CHECK(liveAllocations == 4);
    __ada_array_rewind(&owner, mark);
    CHECK(liveAllocations == 2);
    transfer = testMalloc(100);
    failNext = 1;
    __ada_array_adopt(&owner, transfer);
    CHECK(__ada_exception == ADA_STORAGE_ERROR && liveAllocations == 2 && owner == mark);
    __ada_array_release(&owner);
    CHECK(owner == NULL && liveAllocations == 0);
    __ada_array_release(&owner);
    CHECK(liveAllocations == 0);
    return 0;
}
