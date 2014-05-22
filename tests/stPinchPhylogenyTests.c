#include <stdlib.h>
#include "CuTest.h"
#include "sonLib.h"
#include "stPinchPhylogeny.h"

/*
 * Tests that from random pinch graphs we get reliable set of contextual feature blocks.
 */
static void testStFeatureBlock_getContextualFeatureBlocks(CuTest *testCase) {

}

/*
 * Tests that for a random set of feature blocks we get set of correct feature columns.
 */
static void testStFeatureColumn_getFeatureColumns(CuTest *testCase) {

}

/*
 * Tests that for random sequence graphs the counts of substitutions is accurate.
 */
static void testStPinchPhylogeny_getMatrixFromSubstitutions(CuTest *testCase) {

}
/*
 * Tests that for random sequence graphs the counts of breakpoints is accurate.
 */
static void testStPinchPhylogeny_getMatrixFromBreakPoints(CuTest *testCase) {

}

/*
 * Tests that the symmetric distance matrix is calculated as expected.
 */
static void testStPinchPhylogeny_getSymmetricDistanceMatrix(CuTest *testCase) {

}

void testSimpleRemovePoorlySupportedPartitions(CuTest *testCase)
{
    int64_t i; 
    stTree *tree = stTree_parseNewickString("(((((0,1),(2,3)),4),5),(6,(7,8)));");
    stTree *result;
    stPhylogeny_addStPhylogenyInfo(tree);

    // Running on a tree with no support at all should create a star tree.
    result = removePoorlySupportedPartitions(tree, 1.0);
    CuAssertIntEquals(testCase, 9, stTree_getChildNumber(result));
    for(i = 0; i < stTree_getChildNumber(result); i++) {
        stTree *child = stTree_getChild(result, i);
        CuAssertIntEquals(testCase, 0, stTree_getChildNumber(child));
    }
}

CuSuite* stPinchPhylogenyTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testSimpleRemovePoorlySupportedPartitions);
    SUITE_ADD_TEST(suite, testStFeatureBlock_getContextualFeatureBlocks);
    SUITE_ADD_TEST(suite, testStFeatureColumn_getFeatureColumns);
    SUITE_ADD_TEST(suite, testStPinchPhylogeny_getMatrixFromSubstitutions);
    SUITE_ADD_TEST(suite, testStPinchPhylogeny_getMatrixFromBreakPoints);
    SUITE_ADD_TEST(suite, testStPinchPhylogeny_getSymmetricDistanceMatrix);
    return suite;
}
