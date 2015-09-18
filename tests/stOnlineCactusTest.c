#include "CuTest.h"
#include "sonLib.h"
#include "stOnlineCactus.h"
#include "stOnlineCactus_private.h"
#include "stPinchGraphs.h"
#include "3_Absorb3edge2x.h"
#include <stdlib.h>

stOnlineCactus *cactus;
stConnectivity *connectivity;

stHash *nameToEnd;
stHash *nameToBlock;

stHash *blockToEnd1;
stHash *blockToEnd2;

stList *adjacencyList;

typedef struct _myBlock myBlock;
typedef struct _myEnd myEnd;

struct _myBlock {
    myEnd *end1;
    myEnd *end2;
    char *label;
};

struct _myEnd {
    myBlock *block;
    char *originalLabel;
};

static void *blockToEnd(void *block, bool orientation) {
    myBlock *myBlock = block;
    if (orientation) {
        return myBlock->end1;
    } else {
        return myBlock->end2;
    }
}

static void *endToBlock(void *end) {
    myEnd *myEnd = end;
    return myEnd->block;
}

static void myEnd_destruct(myEnd *end) {
    free(end->originalLabel);
    free(end);
}

static void myBlock_destruct(myBlock *block) {
    myEnd_destruct(block->end1);
    myEnd_destruct(block->end2);
    free(block->label);
    free(block);
}

static void setup() {
    connectivity = stConnectivity_construct();
    cactus = stOnlineCactus_construct(connectivity, blockToEnd, endToBlock);
    blockToEnd1 = stHash_construct3(NULL, NULL, NULL, NULL);
    blockToEnd2 = stHash_construct3(NULL, NULL, NULL, NULL);
    nameToEnd = stHash_construct3(stHash_stringKey, stHash_stringEqualKey, free, (void (*)(void *)) myEnd_destruct);
    nameToBlock = stHash_construct3(stHash_stringKey, stHash_stringEqualKey, NULL,
                                    (void (*)(void *)) myBlock_destruct);
}

static stCactusTree *getCactusTreeR_node(stTree *stNode, stCactusTree *parent, stCactusTree *leftSib,
                                         void *block) {
    cactusNodeType type = NET;
    stList *parts = stString_splitByString(stTree_getLabel(stNode), "_");
    if (strcmp(stList_get(parts, 0), "NET") == 0) {
        type = NET;
    } else if (strcmp(stList_get(parts, 0), "CHAIN") == 0) {
        type = CHAIN;
    } else {
        st_errAbort("unknown cactus node type!");
    }
    char *nodeLabel = stString_copy(stList_get(parts, 1));
    stList_destruct(parts);

    stCactusTree *tree = stCactusTree_construct(parent, NULL, type, block);

    myEnd *nullNode = calloc(1, sizeof(myEnd));
    nullNode->originalLabel = stString_copy(nodeLabel);
    stConnectivity_addNode(connectivity, nullNode);
    stHash_insert(cactus->endToNode, nullNode, tree);
    stHash_insert(nameToEnd, nodeLabel, nullNode);
    if (type == CHAIN) {
        tree->ends = stSet_construct();
    }
    stSet_insert(tree->ends, nullNode);

    stCactusTree *prevChild = NULL;
    for (int64_t i = 0; i < stTree_getChildNumber(stNode); i++) {
        stTree *blockEdge = stTree_getChild(stNode, i);
        stList *blockLabelParts = stString_splitByString(stTree_getLabel(blockEdge), "_");
        assert(strcmp(stList_get(blockLabelParts, 0), "BLOCK") == 0);
        char *blockLabel = stString_copy(stList_get(blockLabelParts, 1));
        // Create a "block" and two "ends".
        myBlock *block = calloc(1, sizeof(myBlock));
        myEnd *end1 = calloc(1, sizeof(myEnd));
        myEnd *end2 = calloc(1, sizeof(myEnd));
        block->end1 = end1;
        block->end2 = end2;
        block->label = blockLabel;
        end1->block = block;
        end1->originalLabel = stString_copy(nodeLabel);
        end2->block = block;
        stConnectivity_addNode(connectivity, end1);
        stConnectivity_addEdge(connectivity, end1, nullNode);
        stHash_insert(cactus->endToNode, end1, tree);
        stSet_insert(tree->ends, end1);

        assert(stTree_getChildNumber(blockEdge) == 1);
        stTree *childStNode = stTree_getChild(blockEdge, 0);
        stCactusTree *child = getCactusTreeR_node(childStNode, tree, prevChild, block);

        stSetIterator *it = stSet_getIterator(child->ends);
        myEnd *arbitraryEnd = stSet_getNext(it);
        stSet_destructIterator(it);
        end2->originalLabel = stString_copy(arbitraryEnd->originalLabel);
        stHash_insert(cactus->endToNode, end2, child);
        stConnectivity_addNode(connectivity, end2);
        stConnectivity_addEdge(connectivity, end2, arbitraryEnd);
        stSet_insert(child->ends, end2);
        stHash_insert(nameToBlock, blockLabel, block);
        stList_destruct(blockLabelParts);
        prevChild = child;
    }
    return tree;
}

// Get a new cactus tree from the newick string.
//
// The newick labels should be NET_a, CHAIN_b, BLOCK_c,
// where the parts after the _ are unique labels in the cactus forest.
//
// The tree is *not* a valid tree, as chain blocks aren't registered
// properly. The resulting tree will not pass stOnlineCactus_check,
// but it *is* valid to compare the labels against other trees after a
// set of operations, to check isomorphism.
static stCactusTree *getCactusTree(char *newick) {
    stTree *stTree = stTree_parseNewickString(newick);
    stCactusTree *tree = getCactusTreeR_node(stTree, NULL, NULL, NULL);
    stTree_destruct(stTree);
    stList_append(cactus->trees, tree);
    return tree;
}

static void teardown() {
    stConnectivity_destruct(connectivity);
    stOnlineCactus_destruct(cactus);
    stHash_destruct(blockToEnd1);
    stHash_destruct(blockToEnd2);
    stHash_destruct(nameToBlock);
    stHash_destruct(nameToEnd);
}

// Get an arbitrary end that originally belonged to this label.
static myEnd *getEndByLabel(char *label) {
    return stHash_search(nameToEnd, label);
}

static stCactusTree *getNodeByLabel(char *label) {
    myEnd *end = getEndByLabel(label);
    return stHash_search(cactus->endToNode, end);
}

// Get the set of labels for this node based on its ends.
static char *getTreeLabel(stCactusTree *tree) {
    stSetIterator *it = stSet_getIterator(tree->ends);
    myEnd *end;
    stSet *labels = stSet_construct3(stHash_stringKey, stHash_stringEqualKey, NULL);
    while ((end = stSet_getNext(it)) != NULL) {
        stSet_insert(labels, end->originalLabel);
    }
    stSet_destructIterator(it);

    // Sort the labels and create a new label combining them in lexicographic order.
    // (we do this instead of using a sorted set, because there's already a join method for lists)
    stList *sortedLabels = stList_construct();
    it = stSet_getIterator(labels);
    char *label;
    while ((label = stSet_getNext(it)) != NULL) {
        stList_append(sortedLabels, label);
    }
    stSet_destructIterator(it);
    stSet_destruct(labels);

    stList_sort(sortedLabels, (int (*)(const void *, const void *)) strcmp);
    char *treeLabel = stString_join2("_", sortedLabels);
    stList_destruct(sortedLabels);
    return treeLabel;
}

static void printNiceTree_r(stCactusTree *tree) {
    if (tree->firstChild) {
        printf("(");
        stCactusTree *child = tree->firstChild;
        while (child != NULL) {
            printf("(");
            printNiceTree_r(child);
            myBlock *block = child->parentEdge->block;
            printf(")BLOCK_%s", block->label);
            child = child->next;
            if (child != NULL) {
                printf(",");
            }
        }
        printf(")");
    }

    char *treeLabel;
    if (tree->ends == NULL) {
        treeLabel = stString_copy("newLabel");
    } else {
        treeLabel = getTreeLabel(tree);
    }

    switch (stCactusTree_type(tree)) {
    case NET:
        printf("NET_%s", treeLabel);
        break;
    case CHAIN:
        printf("CHAIN_%s", treeLabel);
        break;
    default:
        st_errAbort("dangling pointer");
    }
    free(treeLabel);
}

static void printNiceTree(stCactusTree *tree) {
    printNiceTree_r(tree);
    printf(";\n");
}

static void testStOnlineCactus_collapse3ECNets(CuTest *testCase) {
    setup();
    // Copy of Fig. 3A from the paper (up to node 17). Yes, I actually sat here and wrote this thing out.
    getCactusTree("((((((((((CHAIN_C3)BLOCK_E)NET_3)BLOCK_D)CHAIN_C2)BLOCK_C, (CHAIN_C4)BLOCK_F)NET_2)BLOCK_B, (((((((((((((NET_8)BLOCK_M)CHAIN_C6)BLOCK_L)NET_7)BLOCK_K, (((((((NET_11)BLOCK_Q)CHAIN_C7)BLOCK_P)NET_10)BLOCK_O)NET_9)BLOCK_N, (NET_12)BLOCK_R)CHAIN_C5)BLOCK_J)NET_6)BLOCK_I)NET_5)BLOCK_H)NET_4)BLOCK_G)CHAIN_C1)BLOCK_A, (((((((((NET_15)BLOCK_W, (NET_16)BLOCK_X, (NET_17)BLOCK_Y)CHAIN_C9)BLOCK_V)NET_14)BLOCK_U)CHAIN_C8)BLOCK_T)NET_13)BLOCK_S)NET_1;");
    stCactusTree *node9 = getNodeByLabel("9");
    CuAssertTrue(testCase, node9 != NULL);
    CuAssertTrue(testCase, stCactusTree_type(node9) == NET);
    stCactusTree *node15 = getNodeByLabel("15");
    CuAssertTrue(testCase, node15 != NULL);
    CuAssertTrue(testCase, stCactusTree_type(node15) == NET);
    collapse3ECNets(node9, node15, &node9, &node15, cactus->endToNode);
    CuAssertIntEquals(testCase, 1, stList_length(cactus->trees));
    printNiceTree(stList_get(cactus->trees, 0));

    // Test the case when the MRCA is a chain (sadly because we root trees this needs to be special-cased).
    stCactusTree *mrcaChain = getCactusTree("((((((CHAIN_C2)BLOCK_F)NET_2)BLOCK_B, (((((NET_8)BLOCK_J, (NET_9)BLOCK_K, (NET_10)BLOCK_L)CHAIN_C3)BLOCK_I)NET_3)BLOCK_C, (NET_11)BLOCK_M, (NET_12)BLOCK_N, (((NET_7)BLOCK_H)NET_4)BLOCK_D, (((NET_6)BLOCK_G)NET_5)BLOCK_E)CHAIN_C1)BLOCK_A)NET_1;");
    node9 = getNodeByLabel("9");
    CuAssertTrue(testCase, node9 != NULL);
    CuAssertTrue(testCase, stCactusTree_type(node9) == NET);
    stCactusTree *node7 = getNodeByLabel("7");
    CuAssertTrue(testCase, node7 != NULL);
    CuAssertTrue(testCase, stCactusTree_type(node7) == NET);
    collapse3ECNets(node7, node9, &node7, &node9, cactus->endToNode);
    CuAssertIntEquals(testCase, 2, stList_length(cactus->trees));
    printNiceTree(mrcaChain);
    teardown();
}

static void testStOnlineCactus_reroot(CuTest *testCase) {
    setup();
    // fig 1a again
    getCactusTree("((((((((((CHAIN_C3)BLOCK_E)NET_3)BLOCK_D)CHAIN_C2)BLOCK_C, (CHAIN_C4)BLOCK_F)NET_2)BLOCK_B, (((((((((((((NET_8)BLOCK_M)CHAIN_C6)BLOCK_L)NET_7)BLOCK_K, (((((((NET_11)BLOCK_Q)CHAIN_C7)BLOCK_P)NET_10)BLOCK_O)NET_9)BLOCK_N, (NET_12)BLOCK_R)CHAIN_C5)BLOCK_J)NET_6)BLOCK_I)NET_5)BLOCK_H)NET_4)BLOCK_G)CHAIN_C1)BLOCK_A, (((((((((NET_15)BLOCK_W, (NET_16)BLOCK_X, (NET_17)BLOCK_Y)CHAIN_C9)BLOCK_V)NET_14)BLOCK_U)CHAIN_C8)BLOCK_T)NET_13)BLOCK_S)NET_1;");
    // reroot at 9
    stCactusTree_reroot(getNodeByLabel("9"), cactus->trees);
    CuAssertIntEquals(testCase, 1, stList_length(cactus->trees));
    CuAssertTrue(testCase, stList_get(cactus->trees, 0) == getNodeByLabel("9"));
    printNiceTree(getNodeByLabel("9"));

    // more complicated chain
    getCactusTree("((((((NET_3)BLOCK_B, (NET_4)BLOCK_C, (NET_5)BLOCK_D, (NET_6)BLOCK_E, (NET_7)BLOCK_F)CHAIN_C1)BLOCK_G)NET_2)BLOCK_A)NET_1;");
    stCactusTree_reroot(getNodeByLabel("5"), cactus->trees);
    CuAssertIntEquals(testCase, 2, stList_length(cactus->trees));
    printNiceTree(getNodeByLabel("5"));
    teardown();
}

static void testStOnlineCactus_addEdge(CuTest *testCase) {
    setup();
    // fig 1a
    stCactusTree *tree = getCactusTree("((((((((((CHAIN_C3)BLOCK_E)NET_3)BLOCK_D)CHAIN_C2)BLOCK_C, (CHAIN_C4)BLOCK_F)NET_2)BLOCK_B, (((((((((((((NET_8)BLOCK_M)CHAIN_C6)BLOCK_L)NET_7)BLOCK_K, (((((((NET_11)BLOCK_Q)CHAIN_C7)BLOCK_P)NET_10)BLOCK_O)NET_9)BLOCK_N, (NET_12)BLOCK_R)CHAIN_C5)BLOCK_J)NET_6)BLOCK_I)NET_5)BLOCK_H)NET_4)BLOCK_G)CHAIN_C1)BLOCK_A, (((((((((NET_15)BLOCK_W, (NET_16)BLOCK_X, (NET_17)BLOCK_Y)CHAIN_C9)BLOCK_V)NET_14)BLOCK_U)CHAIN_C8)BLOCK_T)NET_13)BLOCK_S)NET_1;");
    myEnd *end1 = getEndByLabel("9");
    myEnd *end2 = getEndByLabel("15");
    myBlock *block = calloc(1, sizeof(myBlock));
    block->label = "new";
    block->end1 = end1;
    block->end2 = end2;
    stOnlineCactus_addEdge(cactus, getEndByLabel("9"), getEndByLabel("15"), block);
    printNiceTree(tree);
    teardown();
}

// === Testing against randomly generated graphs ===

// These tests use the same interface as above, which means a lot of
// conversion of ints to and from strings. It's ugly, but there are
// fewer moving parts this way.

// Gets a set of nodes of the given size in the online cactus and the adjacency list.
static void getRandomNodeSet(int64_t size) {
    assert(size > 0);
    adjacencyList = stList_construct3(0, (void(*)(void *)) stList_destruct);
    for (int64_t i = 0; i < size; i++) {
        // Create the space in the adjacency list.
        stList_append(adjacencyList, stList_construct3(0, (void(*)(void *)) stIntTuple_destruct));
        // Create the cactus node.
        char *newick = stString_print("NET_%" PRIi64 ";", i);
        getCactusTree(newick);
        free(newick);
    }
}

// Adds an edge to the adjacency list and to the online cactus.
static void addEdge(int64_t i, int64_t j, int64_t edgeNum) {
    assert(i < stList_length(adjacencyList));
    assert(j < stList_length(adjacencyList));
    stList_append(stList_get(adjacencyList, i), stIntTuple_construct1(j));
    stList_append(stList_get(adjacencyList, j), stIntTuple_construct1(i));
    // Create an edge in the cactus forest.
    char *iLabel = stString_print("%" PRIi64, i);
    char *jLabel = stString_print("%" PRIi64, j);
    myEnd *iEnd = calloc(1, sizeof(myEnd));
    myEnd *jEnd = calloc(1, sizeof(myEnd));
    myBlock *block = calloc(1, sizeof(myBlock));
    block->end1 = iEnd;
    block->end2 = jEnd;
    block->label = stString_print("%" PRIi64, edgeNum);
    stHash_insert(nameToBlock, stString_print("%" PRIi64, edgeNum), block);
    iEnd->block = block;
    iEnd->originalLabel = iLabel;
    stConnectivity_addNode(connectivity, iEnd);
    stConnectivity_addEdge(connectivity, getEndByLabel(iLabel), iEnd);
    stConnectivity_addNode(connectivity, jEnd);
    stConnectivity_addEdge(connectivity, getEndByLabel(jLabel), jEnd);
    stOnlineCactus_createEnd(cactus, iEnd);
    stOnlineCactus_createEnd(cactus, jEnd);
    jEnd->block = block;
    jEnd->originalLabel = jLabel;
    stOnlineCactus_addEdge(cactus, iEnd, jEnd, block);
}

// Get, in a very hacky way, the set of integers (as 1-length
// stIntTuples) corresponding to the nodes that got merged into this
// node.
static stSet *getMergedNodesBelongingToNode(stCactusTree *tree) {
    char *label = getTreeLabel(tree);
    stList *labelParts = stString_splitByString(label, "_");
    stSet *ret = stSet_construct3((uint64_t (*)(const void * )) stIntTuple_hashKey,
                                  (int (*)(const void *, const void *)) stIntTuple_equalsFn,
                                  (void (*)(void *)) stIntTuple_destruct);
    for (int64_t i = 0; i < stList_length(labelParts); i++) {
        stSet_insert(ret, stIntTuple_construct1(atoll(stList_get(labelParts, i))));
    }
    free(label);
    stList_destruct(labelParts);
    return ret;
}

static void printNiceCactus() {
    for (int64_t i = 0; i < stList_length(cactus->trees); i++) {
        stCactusTree *tree = stList_get(cactus->trees, i);
        printNiceTree(tree);
    }
}

// Do random tests in the "forward" direction (a.k.a. edge addition)
// and check that the 3-edge-connected components match a known-good
// non-dynamic algorithm.
static void testStOnlineCactus_random_forward(CuTest *testCase) {
    setup();
    int64_t numNodes = 100;
    getRandomNodeSet(numNodes);
    int64_t numEdges = numNodes * 2;
    for (int64_t i = 0; i < numEdges; i++) {
        int64_t node1 = st_randomInt64(0, numNodes);
        int64_t node2 = st_randomInt64(0, numNodes);
        addEdge(node1, node2, i);
        // printNiceCactus();
        stOnlineCactus_check(cactus);
    }
    stList *components = computeThreeEdgeConnectedComponents(adjacencyList);
    printf("Got %" PRIi64 " components from 3-edge-connected code\n", stList_length(components));
    for (int64_t i = 0; i < stList_length(components); i++) {
        stList *component = stList_get(components, i);
        char *label = stString_print("%" PRIi64, stIntTuple_get(stList_get(component, 0), 0));
        stCactusTree *tree = getNodeByLabel(label);
        stSet *mergedNodes = getMergedNodesBelongingToNode(tree);
        // Check that the component and the tree nodes are composed of the same original nodes.
        stSet *componentNodes = stSet_construct3((uint64_t (*)(const void * )) stIntTuple_hashKey,
                                                 (int (*)(const void *, const void *)) stIntTuple_equalsFn,
                                                 NULL);
        for (int64_t j = 0; j < stList_length(component); j++) {
            stSet_insert(componentNodes, stList_get(component, j));
        }
        CuAssertIntEquals(testCase, stSet_size(componentNodes), stSet_size(mergedNodes));
        stSet *difference = stSet_getDifference(componentNodes, mergedNodes);
        CuAssertTrue(testCase, stSet_size(difference) == 0);
        stSet_destruct(difference);
        stSet_destruct(componentNodes);
        stSet_destruct(mergedNodes);
        free(label);
    }
    stList_destruct(components);
    teardown();
}

static void deleteEdge(int64_t node1, int64_t node2, int64_t i) {
    char *blockLabel = stString_print("%" PRIi64, i);
    myBlock *block = stHash_search(nameToBlock, blockLabel);
    stOnlineCactus_deleteEdge(cactus, block->end1, block->end2, block);
    stConnectivity_removeNode(connectivity, block->end1);
    stConnectivity_removeNode(connectivity, block->end2);
    stList *node1Edges = stList_get(adjacencyList, node1);
    for (int64_t i = 0; i < stList_length(node1Edges); i++) {
        if (stIntTuple_get(stList_get(node1Edges, i), 0) == node2) {
            stList_remove(node1Edges, i);
            break;
        }
    }
    stList *node2Edges = stList_get(adjacencyList, node2);
    for (int64_t i = 0; i < stList_length(node2Edges); i++) {
        if (stIntTuple_get(stList_get(node2Edges, i), 0) == node1) {
            stList_remove(node2Edges, i);
            break;
        }
    }
}

// Do random tests of adding and deleting random edges and check that
// the 3-edge-connectivity relationships still make sense.
static void testStOnlineCactus_random_forward_and_backward(CuTest *testCase) {
    setup();
    int64_t numNodes = 200;
    getRandomNodeSet(numNodes);
    int64_t numEdges = numNodes * 1.5;
    stList *edges = stList_construct3(0, (void (*)(void *)) stIntTuple_destruct);
    for (int64_t i = 0; i < numEdges; i++) {
        int64_t node1 = st_randomInt64(0, numNodes);
        int64_t node2 = st_randomInt64(0, numNodes);
        printf("adding edge %" PRIi64 ": %" PRIi64 "<->%" PRIi64 "\n", i, node1, node2);
        addEdge(node1, node2, i);
        stList_append(edges, stIntTuple_construct3(node1, node2, i));
        printNiceCactus();
        stOnlineCactus_check(cactus);
    }
    int64_t numDeletions = st_randomInt64(0, stList_length(edges));
    for (int64_t i = 0; i < numDeletions; i++) {
        int64_t randomIndex = st_randomInt64(0, stList_length(edges));
        stIntTuple *edge = stList_get(edges, randomIndex);
        int64_t edgeNum = stIntTuple_get(edge, 2);
        int64_t node1 = stIntTuple_get(edge, 0);
        int64_t node2 = stIntTuple_get(edge, 1);
        printf("removing edge %" PRIi64 ": %" PRIi64 "<->%" PRIi64 "\n", edgeNum, node1, node2);
        deleteEdge(node1, node2, edgeNum);
        printNiceCactus();
        stOnlineCactus_check(cactus);
        stList_remove(edges, randomIndex);
    }
    stList *components = computeThreeEdgeConnectedComponents(adjacencyList);
    printf("Got %" PRIi64 " components from 3-edge-connected code\n", stList_length(components));
    for (int64_t i = 0; i < stList_length(components); i++) {
        stList *component = stList_get(components, i);
        char *label = stString_print("%" PRIi64, stIntTuple_get(stList_get(component, 0), 0));
        stCactusTree *tree = getNodeByLabel(label);
        stSet *mergedNodes = getMergedNodesBelongingToNode(tree);
        // Check that the component and the tree nodes are composed of the same original nodes.
        stSet *componentNodes = stSet_construct3((uint64_t (*)(const void * )) stIntTuple_hashKey,
                                                 (int (*)(const void *, const void *)) stIntTuple_equalsFn,
                                                 NULL);
        for (int64_t j = 0; j < stList_length(component); j++) {
            printf("component %" PRIi64 " has item %" PRIi64 "\n", i, stIntTuple_get(stList_get(component, j), 0));
            stSet_insert(componentNodes, stList_get(component, j));
        }
        CuAssertIntEquals(testCase, stSet_size(componentNodes), stSet_size(mergedNodes));
        stSet *difference = stSet_getDifference(componentNodes, mergedNodes);
        CuAssertTrue(testCase, stSet_size(difference) == 0);
        stSet_destruct(difference);
        stSet_destruct(componentNodes);
        stSet_destruct(mergedNodes);
        free(label);
    }
    stList_destruct(components);
    teardown();
}

CuSuite* stOnlineCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_collapse3ECNets);
    SUITE_ADD_TEST(suite, testStOnlineCactus_reroot);
    SUITE_ADD_TEST(suite, testStOnlineCactus_addEdge);
    SUITE_ADD_TEST(suite, testStOnlineCactus_random_forward);
    SUITE_ADD_TEST(suite, testStOnlineCactus_random_forward_and_backward);
    return suite;
}
