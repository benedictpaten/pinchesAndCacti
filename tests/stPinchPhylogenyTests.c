#include <stdlib.h>
#include "CuTest.h"
#include "sonLib.h"
#include "stPinchPhylogeny.h"

// Get the distance to a leaf from an internal node
static double distToLeaf(stTree *tree, int64_t leafIndex)
{
    int64_t i;
    stPhylogenyInfo *info = stTree_getClientData(tree);
    assert(info->leavesBelow[leafIndex]);
    if(stTree_getChildNumber(tree) == 0) {
        return 0.0;
    }
    for(i = 0; i < stTree_getChildNumber(tree); i++) {
        stTree *child = stTree_getChild(tree, i);
        stPhylogenyInfo *childInfo = stTree_getClientData(child);
        if(childInfo->leavesBelow[leafIndex]) {
            return stTree_getBranchLength(child) + distToLeaf(child, leafIndex);
        }
    }
    // We shouldn't've gotten here--none of the children have the
    // leaf under them, but this node does!
    return 0.0/0.0;
}

// Find the distance between leaves (given by their index in the
// distance matrix.)
static double distanceBetweenLeaves(stTree *tree, int64_t leaf1,
                                    int64_t leaf2)
{
    int64_t i;
    for(i = 0; i < stTree_getChildNumber(tree); i++) {
        stTree *child = stTree_getChild(tree, i);
        stPhylogenyInfo *childInfo = stTree_getClientData(child);
        if(childInfo->leavesBelow[leaf1] && childInfo->leavesBelow[leaf2]) {
            return distanceBetweenLeaves(child, leaf1, leaf2);
        }
    }
    // If we've gotten to this point, then this is the MRCA of the leaves.
    return distToLeaf(tree, leaf1) + distToLeaf(tree, leaf2);
}

static void staticNeighborJoinTest(CuTest *testCase) {
    int64_t i;
    // Simple static test, with index 1 very far away from everything,
    // 0 and 3 very close, and 2 closer to (0,3) than to 1.
    double distances[4][4] = {{0.0, 9.0, 3.0, 0.1},
                              {9.0, 0.0, 6.0, 8.9},
                              {3.0, 6.0, 0.0, 3.0},
                              {0.1, 8.9, 3.0, 0.0}};
    double *distanceMatrix[] = {distances[0], distances[1],
                                distances[2], distances[3]};
    stTree *tree = neighborJoin(distanceMatrix, 4);
    stPhylogenyInfo *info = stTree_getClientData(tree);

    // Check that leavesBelow is correct for the root (every leaf is
    // below the root)
    for(i = 0; i < 4; i++) {
        CuAssertIntEquals(testCase, info->leavesBelow[i], 1);
    }

    // We don't want to check the topology, since the root is
    // arbitrary (though currently halfway along the longest branch.)
    // So instead we check that the distances make sense. 0,3 should be the
    // closest together, etc.
    CuAssertTrue(testCase, distanceBetweenLeaves(tree, 0, 3) < distanceBetweenLeaves(tree, 0, 2));
    CuAssertTrue(testCase, distanceBetweenLeaves(tree, 0, 3) < distanceBetweenLeaves(tree, 0, 1));
    CuAssertTrue(testCase, distanceBetweenLeaves(tree, 0, 3) < distanceBetweenLeaves(tree, 3, 2));
    CuAssertTrue(testCase, distanceBetweenLeaves(tree, 0, 3) < distanceBetweenLeaves(tree, 3, 1));
    CuAssertTrue(testCase, distanceBetweenLeaves(tree, 0, 2) < distanceBetweenLeaves(tree, 0, 1));
    CuAssertTrue(testCase, distanceBetweenLeaves(tree, 0, 2) < distanceBetweenLeaves(tree, 2, 1));

    stPhylogenyInfo_destructOnTree(tree);
    stTree_destruct(tree);
}

CuSuite* stPinchPhylogenyTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, staticNeighborJoinTest);
    return suite;
}
