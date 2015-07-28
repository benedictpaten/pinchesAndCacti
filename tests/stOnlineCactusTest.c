#include "CuTest.h"
#include "sonLib.h"
#include "stOnlineCactus.h"
#include <stdlib.h>

void testStOnlineCactus_empty(CuTest *testCase) {
    stConnectivity *connectivity = stConnectivity_construct();
    stSet *blocks = stSet_construct();
    stSet_insert(blocks, (void *) 1);
    stSet_insert(blocks, (void *) 2);
    stSet_insert(blocks, (void *) 3);
    stSet_insert(blocks, (void *) 4);
    stOnlineCactus *cactus = stOnlineCactus_construct(connectivity, blocks);
    printf("empty cactus\n");
    stOnlineCactus_print(cactus);
    printf("Splitting 2 into 5 and 6\n");
    stOnlineCactus_splitEdgeHorizontally(cactus, (void *) 2, (void *) 5, (void *) 6);
    stOnlineCactus_print(cactus);
    printf("Merging that back together\n");
    stOnlineCactus_mergeAdjacentEdges(cactus, (void *) 5, (void *) 6, (void *) 2);
    stOnlineCactus_print(cactus);
}

CuSuite* stOnlineCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_empty);
    return suite;
}
