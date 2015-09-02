#include "CuTest.h"
#include "sonLib.h"
#include "stOnlineCactus.h"
#include "stOnlineCactus_private.h"
#include "stPinchGraphs.h"
#include <stdlib.h>

stOnlineCactus *cactus;
stConnectivity *connectivity;

stHash *nameToEnds;
stHash *nameToBlock;

stHash *blockToEnd1;
stHash *blockToEnd2;

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

static void setup() {
    connectivity = stConnectivity_construct();
    cactus = stOnlineCactus_construct(connectivity, blockToEnd, endToBlock);
    blockToEnd1 = stHash_construct();
    blockToEnd2 = stHash_construct();
    nameToEnds = stHash_construct3(stHash_stringKey, stHash_stringEqualKey, free, NULL);
    nameToBlock = stHash_construct3(stHash_stringKey, stHash_stringEqualKey, free, free);
}

static stCactusTree *getCactusTreeR_node(stTree *stNode, stCactusTree *parent, stCactusTree *leftSib,
                                         void *block) {
    cactusNodeType type;
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

    stHash_insert(nameToEnds, nodeLabel, tree->ends);

    myEnd *nullNode = calloc(1, sizeof(myEnd));
    nullNode->originalLabel = nodeLabel;
    stConnectivity_addNode(connectivity, nullNode);
    stHash_insert(cactus->endToNode, nullNode, tree);
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
        end1->originalLabel = nodeLabel;
        end2->block = block;
        stConnectivity_addNode(connectivity, end1);
        stConnectivity_addEdge(connectivity, end1, nullNode);
        stConnectivity_addNode(connectivity, end2);
        stConnectivity_addEdge(connectivity, end2, nullNode);
        stHash_insert(cactus->endToNode, end1, tree);
        stSet_insert(tree->ends, end1);

        assert(stTree_getChildNumber(blockEdge) == 1);
        stTree *childStNode = stTree_getChild(blockEdge, 0);
        stCactusTree *child = getCactusTreeR_node(childStNode, tree, prevChild, block);

        end2->originalLabel = ((myEnd *) stSet_getNext(stSet_getIterator(child->ends)))->originalLabel;
        stHash_insert(cactus->endToNode, end2, child);
        stSet_insert(child->ends, end2);
        stHash_insert(nameToBlock, blockLabel, block);
        stList_destruct(blockLabelParts);
        prevChild = child;
    }
    return tree;
}

// Get a new cactus tree from the newick string.
// The newick labels should be NET_a, CHAIN_b, BLOCK_c,
// where the parts after the _ are unique labels in the cactus forest.
static stCactusTree *getCactusTree(char *newick) {
    stTree *stTree = stTree_parseNewickString(newick);
    stCactusTree *tree = getCactusTreeR_node(stTree, NULL, NULL, NULL);
    stList_append(cactus->trees, tree);
    return tree;
}

static void teardown() {
    stConnectivity_destruct(connectivity);
}

static stCactusTree *getNodeByLabel(char *label) {
    stSet *ends = stHash_search(nameToEnds, label);
    myEnd *end = stSet_getNext(stSet_getIterator(ends));
    return stHash_search(cactus->endToNode, end);
}

static void printNiceTree(stCactusTree *tree) {
    if (tree->firstChild) {
        printf("(");
        stCactusTree *child = tree->firstChild;
        while (child != NULL) {
            printf("(");
            printNiceTree(child);
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
        // Get the set of labels for this node based on its ends.
        stSetIterator *it = stSet_getIterator(tree->ends);
        myEnd *end;
        stSet *labels = stSet_construct();
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
        treeLabel = stString_join2("_", sortedLabels);
        stList_destruct(sortedLabels);
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

static void testStOnlineCactus_collapse3ECNets(CuTest *testCase) {
    setup();
    // Copy of Fig. 3A from the paper (up to node 17). Yes, I actually sat here and wrote this thing out.
    stCactusTree *tree = getCactusTree("((((((((((CHAIN_C3)BLOCK_E)NET_3)BLOCK_D)CHAIN_C2)BLOCK_C, (CHAIN_C4)BLOCK_F)NET_2)BLOCK_B, (((((((((((((NET_8)BLOCK_M)CHAIN_C6)BLOCK_L)NET_7)BLOCK_K, (((((((NET_11)BLOCK_Q)CHAIN_C7)BLOCK_P)NET_10)BLOCK_O)NET_9)BLOCK_N, (NET_12)BLOCK_R)CHAIN_C5)BLOCK_J)NET_6)BLOCK_I)NET_5)BLOCK_H)NET_4)BLOCK_G)CHAIN_C1)BLOCK_A, (((((((((NET_15)BLOCK_W, (NET_16)BLOCK_X, (NET_17)BLOCK_Y)CHAIN_C9)BLOCK_V)NET_14)BLOCK_U)CHAIN_C8)BLOCK_T)NET_13)BLOCK_S)NET_1;");
    stCactusTree *node9 = getNodeByLabel("9");
    CuAssertTrue(testCase, node9 != NULL);
    CuAssertTrue(testCase, stCactusTree_type(node9) == NET);
    stCactusTree *node15 = getNodeByLabel("15");
    CuAssertTrue(testCase, node15 != NULL);
    CuAssertTrue(testCase, stCactusTree_type(node15) == NET);
    collapse3ECNets(node9, node15, &node9, &node15, cactus->endToNode);
    printNiceTree(tree);
    teardown();
}

CuSuite* stOnlineCactusTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStOnlineCactus_collapse3ECNets);
    return suite;
}
