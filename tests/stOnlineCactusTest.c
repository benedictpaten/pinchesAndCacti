#include "CuTest.h"
#include "sonLib.h"
#include "stOnlineCactus.h"
#include "stPinchGraphs.h"
#include <stdlib.h>

stPinchBlock *stPinchSegmentCap_getBlock(stPinchSegmentCap *cap) {
    return stPinchSegment_getBlock(stPinchSegmentCap_getSegment(cap));
}

// Trivial test to ensure an empty cactus graph works properly (has a
// single node).
void testStOnlineCactus_empty(CuTest *testCase) {
    stPinchThreadSet *threadSet = stPinchThreadSet_construct();
    stPinchThreadSet_addThread(threadSet, 1, 0, 100);
    stPinchThreadSet_addThread(threadSet, 2, 0, 200);
    stConnectivity *connectivity = stPinchThreadSet_getAdjacencyConnectivity(threadSet);
    stOnlineCactus *cactus = stOnlineCactus_construct(
        connectivity,
        (void *(*)(void *, bool)) stPinchBlock_getRepresentativeSegmentCap,
        (void *(*)(void *)) stPinchSegmentCap_getBlock);
    stCactusTree *tree = stOnlineCactus_getCactusTree(cactus);
    CuAssertTrue(testCase, stCactusTree_type(tree) == NODE);

    // The root should have no children in this empty graph.
    stCactusTreeIt *it = stCactusTree_getIt(tree);
    CuAssertTrue(testCase, stCactusTreeIt_getNext(it) == NULL);
    stCactusTreeIt_destruct(it);
    // stOnlineCactus_destruct(cactus);    
}

CuSuite* stOnlineCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_empty);
    return suite;
}
