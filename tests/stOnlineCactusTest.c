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
    stPinchThreadSet_setBlockCreationCallback(threadSet, (void (*)(void *, stPinchBlock *)) stOnlineCactus_createEdge, cactus);
    stPinchThreadSet_setBlockSplitCallback(threadSet, (void (*)(void *, stPinchBlock *, stPinchSegmentCap *, stPinchSegmentCap *, stPinchBlock *, stPinchBlock *)) stOnlineCactus_splitEdgeHorizontally, cactus);
}

static void teardown(void) {
    stPinchThreadSet_destruct(threadSet);
    // stOnlineCactus_destruct(cactus);
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
    stCactusTree *tree = stOnlineCactus_getCactusTree(cactus);
    stCactusTreeIt *it = stCactusTree_getIt(tree);
    stCactusTree *child;
    size_t numChildren = 0;
    while((child = stCactusTreeIt_getNext(it)) != NULL) {
        numChildren++;
        // Ensure that the children are chains.
        CuAssertTrue(testCase, stCactusTree_type(child) == CHAIN);
        // Ensure that these chain nodes have one child each.
        size_t numGrandChildren = 0;
        stCactusTreeIt *childIt = stCactusTree_getIt(child);
        stCactusTree *grandChild;
        while ((grandChild = stCactusTreeIt_getNext(childIt)) != NULL) {
            numGrandChildren++;
            stCactusTreeIt *grandChildIt = stCactusTree_getIt(grandChild);
            CuAssertTrue(testCase, stCactusTreeIt_getNext(grandChildIt) == NULL);
        }
        CuAssertIntEquals(testCase, 1, numGrandChildren);
        stCactusTreeIt_destruct(childIt);
    }
    CuAssertIntEquals(testCase, 2, numChildren);
    stCactusTreeIt_destruct(it);
    teardown();
}

static void testStOnlineCactus_edgeCreation(CuTest *testCase) {
    setup();
    stPinchBlock_construct2(stPinchThread_getSegment(thread1, 1));
    stPinchBlock_construct2(stPinchThread_getSegment(thread2, 1));
    stOnlineCactus_print(cactus);
    stCactusTree *tree = stOnlineCactus_getCactusTree(cactus);
    CuAssertTrue(testCase, stCactusTree_type(tree) == NODE);
    stCactusTreeIt *it = stCactusTree_getIt(tree);
    stCactusTree *child;
    size_t numChildren = 0;
    while((child = stCactusTreeIt_getNext(it)) != NULL) {
        numChildren++;
        // Ensure that the children are chains.
        CuAssertTrue(testCase, stCactusTree_type(child) == CHAIN);
        // Ensure that these chain nodes have no children.
        stCactusTreeIt *childIt = stCactusTree_getIt(child);
        CuAssertTrue(testCase, stCactusTreeIt_getNext(childIt) == NULL);
        stCactusTreeIt_destruct(childIt);
    }
    CuAssertIntEquals(testCase, 2, numChildren);
    stCactusTreeIt_destruct(it);
    teardown();
}

// Trivial test to ensure an empty cactus graph works properly (has a
// single node).
static void testStOnlineCactus_empty(CuTest *testCase) {
    setup();
    stCactusTree *tree = stOnlineCactus_getCactusTree(cactus);
    CuAssertTrue(testCase, stCactusTree_type(tree) == NODE);

    // The root should have no children in this empty graph.
    stCactusTreeIt *it = stCactusTree_getIt(tree);
    CuAssertTrue(testCase, stCactusTreeIt_getNext(it) == NULL);
    stCactusTreeIt_destruct(it);
    // stOnlineCactus_destruct(cactus);
    teardown();
}

CuSuite* stOnlineCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_empty);
    SUITE_ADD_TEST(suite, testStOnlineCactus_edgeCreation);
    SUITE_ADD_TEST(suite, testStOnlineCactus_edgeSplit);
    return suite;
}
