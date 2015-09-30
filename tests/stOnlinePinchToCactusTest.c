#include "CuTest.h"
#include "sonLib.h"
#include "stOnlineCactus.h"
#include "stPinchGraphs.h"
#include <stdlib.h>

#if 0

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
    stPinchThreadSet_setEndCreationCallback(threadSet, (void (*)(void *, stPinchSegmentCap *)) stOnlineCactus_createEnd, cactus);
    stPinchThreadSet_setBlockCreationCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *, stPinchBlock *)) stOnlineCactus_addEdge, cactus);
    stPinchThreadSet_setBlockDeletionCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *, stPinchBlock *)) stOnlineCactus_deleteEdge, cactus);
    stPinchThreadSet_setEndMergeCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *)) stOnlineCactus_netMerge, cactus);
    stPinchThreadSet_setEndCleaveCallback(threadSet, (bool (*)(void *, stPinchSegmentCap *, stSet *)) stOnlineCactus_netCleave, cactus);
}

static void teardown(void) {
    stPinchThreadSet_destruct(threadSet);
    stOnlineCactus_destruct(cactus);
}

// Check that every block exists in the cactus forest.
static void simpleSanityChecks(CuTest *testCase) {
    stPinchThreadSetBlockIt it = stPinchThreadSet_getBlockIt(threadSet);
    stPinchBlock *block;
    while ((block = stPinchThreadSetBlockIt_getNext(&it)) != NULL) {
        CuAssertTrue(testCase, stOnlineCactus_getEdge(cactus, block) != NULL);
    }
}

static void testStOnlineCactus_simpleBlockDelete(CuTest *testCase) {
    setup();
    stPinchBlock *block = stPinchBlock_construct2(stPinchThread_getSegment(thread1, 1));
    stPinchBlock_construct2(stPinchThread_getSegment(thread2, 1));
    stPinchBlock_destruct(block);
    stOnlineCactus_print(cactus);
    stOnlineCactus_check(cactus);
    simpleSanityChecks(testCase);
    teardown();
}

static void testStOnlineCactus_edgeMerge(CuTest *testCase) {
    setup();
    stPinchThread_pinch(thread1, thread2, 0, 0, 50, 1);
    stOnlineCactus_print(cactus);
    simpleSanityChecks(testCase);
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
    simpleSanityChecks(testCase);
    teardown();
}

static void testStOnlineCactus_blockCreation(CuTest *testCase) {
    setup();
    stPinchBlock_construct2(stPinchThread_getSegment(thread1, 1));
    stPinchBlock_construct2(stPinchThread_getSegment(thread2, 1));
    stOnlineCactus_print(cactus);
    stOnlineCactus_check(cactus);
    simpleSanityChecks(testCase);
    teardown();
}

static void testStOnlineCactus_endCleave(CuTest *testCase) {
    setup();
    stPinchThread_pinch(thread1, thread2, 50, 50, 30, 1);
    stOnlineCactus_print(cactus);
    stPinchUndo *undo = stPinchThread_prepareUndo(thread1, thread2, 0, 0, 50, 0);
    stPinchThread_pinch(thread1, thread2, 0, 0, 50, 0);
    stOnlineCactus_print(cactus);
    stPinchThreadSet_undoPinch(threadSet, undo);
    stOnlineCactus_print(cactus);
    stOnlineCactus_check(cactus);
    stPinchUndo_destruct(undo);
    teardown();
}

static void testStOnlinePinchToCactus_random(CuTest *testCase) {
    for (int64_t i = 0; i < 100; i++) {
        printf("Random test %" PRIi64"\n", i);
        threadSet = stPinchThreadSet_getRandomEmptyGraph();
        connectivity = stPinchThreadSet_getAdjacencyConnectivity(threadSet);
        cactus = stOnlineCactus_construct(
            connectivity,
            (void *(*)(void *, bool)) stPinchBlock_getRepresentativeSegmentCap,
            (void *(*)(void *)) stPinchSegmentCap_getBlock);
        stPinchThreadSet_setEndCreationCallback(threadSet, (void (*)(void *, stPinchSegmentCap *)) stOnlineCactus_createEnd, cactus);
        stPinchThreadSet_setBlockCreationCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *, stPinchBlock *)) stOnlineCactus_addEdge, cactus);
        stPinchThreadSet_setBlockDeletionCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *, stPinchBlock *)) stOnlineCactus_deleteEdge, cactus);
        stPinchThreadSet_setEndMergeCallback(threadSet, (void (*)(void *, stPinchSegmentCap *, stPinchSegmentCap *)) stOnlineCactus_netMerge, cactus);
        stPinchThreadSet_setEndCleaveCallback(threadSet, (bool (*)(void *, stPinchSegmentCap *, stSet *)) stOnlineCactus_netCleave, cactus);

        double threshold = st_random();
        if (threshold < 0.01) {
            threshold = 0.01; // Just to prevent the test from
            // exploding every so often.
        }
        while (st_random() > threshold) {
            stPinch pinch = stPinchThreadSet_getRandomPinch(threadSet);
            stPinchUndo *undo = stPinchThread_prepareUndo(stPinchThreadSet_getThread(threadSet, pinch.name1),
                                                          stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                                                          pinch.strand);
            stPinchThread_pinch(stPinchThreadSet_getThread(threadSet, pinch.name1),
                                stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                                pinch.strand);

            stOnlineCactus_print(cactus);
            stOnlineCactus_check(cactus);
            simpleSanityChecks(testCase);

            if (st_random() > 0.5) {
                int64_t offset = 0;
                while (offset < pinch.length) {
                    int64_t undoLength = st_randomInt64(0, pinch.length - offset + 1);
                    stPinchThreadSet_partiallyUndoPinch(threadSet, undo, offset, undoLength);
                    offset += undoLength;
                    stOnlineCactus_print(cactus);
                    stOnlineCactus_check(cactus);
                    simpleSanityChecks(testCase);
                }
            }
            stOnlineCactus_print(cactus);
            stOnlineCactus_check(cactus);
            simpleSanityChecks(testCase);
            stPinchUndo_destruct(undo);
        }

        stPinchThreadSet_destruct(threadSet);
        stOnlineCactus_destruct(cactus);
    }
}

CuSuite* stOnlinePinchToCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_blockCreation);
    SUITE_ADD_TEST(suite, testStOnlineCactus_simpleBlockDelete);
    SUITE_ADD_TEST(suite, testStOnlineCactus_edgeSplit);
    SUITE_ADD_TEST(suite, testStOnlineCactus_edgeMerge);
    SUITE_ADD_TEST(suite, testStOnlineCactus_endCleave);
    SUITE_ADD_TEST(suite, testStOnlinePinchToCactus_random);
    return suite;
}
#endif
