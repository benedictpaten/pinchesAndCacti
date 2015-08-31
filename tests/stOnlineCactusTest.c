#include "CuTest.h"
#include "sonLib.h"
#include "stOnlineCactus.h"
#include "stPinchGraphs.h"
#include <stdlib.h>

stPinchBlock *stPinchSegmentCap_getBlock(stPinchSegmentCap *cap) {
    return stPinchSegment_getBlock(stPinchSegmentCap_getSegment(cap));
}

stPinchThreadSet *threadSet;
stPinchThread *thread1;
stPinchThread *thread2;
stPinchThread *thread3;
stConnectivity *connectivity;
stOnlineCactus *cactus;

static void setup(void) {
    threadSet = stPinchThreadSet_construct();
    thread1 = stPinchThreadSet_addThread(threadSet, 1, 0, 100);
    thread2 = stPinchThreadSet_addThread(threadSet, 2, 0, 200);
    thread3 = stPinchThreadSet_addThread(threadSet, 3, 0, 100);
    connectivity = stPinchThreadSet_getAdjacencyConnectivity(threadSet);
    cactus = stOnlineCactus_construct(
        connectivity,
        (void *(*)(void *, bool)) stPinchBlock_getRepresentativeSegmentCap,
        (void *(*)(void *)) stPinchSegmentCap_getBlock);
    stPinchThreadSet_setAdjComponentCreationCallback(threadSet, (void (*)(void *, stPinchSegmentCap *)) stOnlineCactus_createEnd, cactus);
    stPinchThreadSet_setBlockCreationCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *, stPinchBlock *)) stOnlineCactus_addEdge, cactus);
    stPinchThreadSet_setBlockDeletionCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *, stPinchBlock *)) stOnlineCactus_deleteEdge, cactus);
    stPinchThreadSet_setEndMergeCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *)) stOnlineCactus_netMerge, cactus);
}

static void teardown(void) {
    stPinchThreadSet_destruct(threadSet);
    // stOnlineCactus_destruct(cactus);
}

static void testStOnlineCactus_simpleBlockDelete(CuTest *testCase) {
    setup();
    stPinchBlock *block = stPinchBlock_construct2(stPinchThread_getSegment(thread1, 1));
    stPinchBlock_construct2(stPinchThread_getSegment(thread2, 1));
    stPinchBlock_destruct(block);
    stOnlineCactus_print(cactus);
    stOnlineCactus_check(cactus);
    teardown();
}

static void testStOnlineCactus_edgeMerge(CuTest *testCase) {
    setup();
    stPinchThread_pinch(thread1, thread2, 0, 0, 50, 1);
    stOnlineCactus_print(cactus);
    teardown();
}

static void testStOnlineCactus_edgeSplit(CuTest *testCase) {
    setup();
    stPinchBlock_construct2(stPinchThread_getSegment(thread1, 1));
    stPinchBlock_construct2(stPinchThread_getSegment(thread2, 1));
    // Split the two blocks (roughly) in half.
    stPinchThread_split(thread1, 50);
    stPinchThread_split(thread2, 100);
    // This split shouldn't be registered as it doesn't affect a
    // block.
    stPinchThread_split(thread3, 50);
    stOnlineCactus_print(cactus);
    stOnlineCactus_check(cactus);

    teardown();
}

static void testStOnlineCactus_blockCreation(CuTest *testCase) {
    setup();
    stPinchBlock_construct2(stPinchThread_getSegment(thread1, 1));
    stPinchBlock_construct2(stPinchThread_getSegment(thread2, 1));
    stOnlineCactus_print(cactus);
    stOnlineCactus_check(cactus);
    teardown();
}

CuSuite* stOnlineCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_blockCreation);
    SUITE_ADD_TEST(suite, testStOnlineCactus_simpleBlockDelete);
    SUITE_ADD_TEST(suite, testStOnlineCactus_edgeSplit);
    SUITE_ADD_TEST(suite, testStOnlineCactus_edgeMerge);
    return suite;
}
