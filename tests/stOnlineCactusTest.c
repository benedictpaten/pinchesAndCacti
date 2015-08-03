// Testing without pinch callbacks, which is risky. Ensure that all
// splits are made *before* pinches and never as a result of a pinch.
#include "CuTest.h"
#include "sonLib.h"
#include "stOnlineCactus.h"
#include "stPinchGraphs.h"
#include <stdlib.h>

void pinchSplitThread(stPinchThread *thread, int64_t pos, void **oldEdge,
                      void **newEdge1, void **newEdge2) {
    stPinchSegment *oldSegment = stPinchThread_getSegment(thread, pos);
    if (oldSegment == stPinchThread_getSegment(thread, pos + 1)) {
        st_errAbort("attempting to split an already split thread, test is"
                    " broken");
    }
    stPinchBlock *oldBlock = stPinchSegment_getBlock(oldSegment);
    stPinchThread_split(thread, pos);
    stPinchSegment *newSegment1 = stPinchThread_getSegment(thread, pos);
    stPinchSegment *newSegment2 = stPinchThread_getSegment(thread, pos + 1);
    stPinchBlock *newBlock1 = stPinchSegment_getBlock(newSegment1);
    stPinchBlock *newBlock2 = stPinchSegment_getBlock(newSegment2);
    if (oldBlock == NULL) {
        assert(stPinchSegment_getBlock(newSegment1) == NULL);
        assert(stPinchSegment_getBlock(newSegment2) == NULL);
        *oldEdge = oldSegment;
        *newEdge1 = newSegment1;
        *newEdge2 = newSegment2;
    } else {
        *oldEdge = oldBlock;
        *newEdge1 = newBlock1;
        *newEdge2 = newBlock2;
    }
}

// Get the set of "blocks" (really segments) for an empty pinch graph
// with a fixed set of threads.
stSet *getThreadBlocks(stPinchThreadSet *threadSet) {
    stSet *ret = stSet_construct();

    stPinchThreadSetIt threadIt = stPinchThreadSet_getIt(threadSet);
    stPinchThread *thread;
    while ((thread = stPinchThreadSetIt_getNext(&threadSet)) != NULL) {
        stSet_insert(ret, stPinchThread_getFirst(thread));
    }
}

void testStOnlineCactus_pinch(CuTest *testCase) {
    stPinchThreadSet *threadSet = stPinchThreadSet_construct();
    stSet *threadBlocks = getThreadBlocks(threadSet);
    stOnlineCactus *cactus = stOnlineCactus_construct(threadBlocks, pinchAdjacent);
    stOnlineCactus_print(cactus);
}

void testStOnlineCactus_empty(CuTest *testCase) {
    stSet *blocks = stSet_construct();
    stSet_insert(blocks, (void *) 1);
    stSet_insert(blocks, (void *) 2);
    stSet_insert(blocks, (void *) 3);
    stSet_insert(blocks, (void *) 4);
    stOnlineCactus *cactus = stOnlineCactus_construct(blocks, NULL);
    printf("empty cactus\n");
    stOnlineCactus_print(cactus);
}

CuSuite* stOnlineCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_empty);
    return suite;
}
