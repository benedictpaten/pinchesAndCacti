#ifndef __ST_ONLINE_CACTUS_PRIVATE_H_
#define __ST_ONLINE_CACTUS_PRIVATE_H_

// We keep arbitrarily rooted trees for efficiency reasons.
struct _stCactusTree {
    cactusNodeType type;
    stCactusTree *parent;
    stCactusTreeEdge *parentEdge;
    // Previous child in the child ordering of the parent.
    stCactusTree *prev;
    // Next child in the parent's child ordering.
    stCactusTree *next;
    stCactusTree *firstChild;
    // Contains the base nodes contained in this net.
    // NULL if this is a chain node.
    stSet *nodes;
};

struct _stCactusTreeEdge {
    stCactusTree *child;
    void *block;
};

struct _stOnlineCactus {
    stList *trees;
    stHash *blockToEdge;
    stHash *endToNode;
    stHash *nodeToEnds;
    stHash *nodeToNet;
    void *(*edgeToEnd)(void *, bool);
    void *(*endToEdge)(void *);
    uint64_t (*getEdgeWeight)(const void *);
    stHash *blockToMaximalPath;
    stSortedSet *maximalPaths;
    int64_t numMergeOps;
    int64_t numSplitOps;
    int64_t numEdgeAddOps;
    int64_t numEdgeDeleteOps;
    int64_t numNodeAddOps;
    int64_t numNodeDeleteOps;
};

struct _stCactusTreeIt {
    stCactusTree *cur;
};

// This simple structure keeps track of the chains and maximal
// bridge-paths in the cactus forest, so that they can be updated and
// queried quickly.
typedef struct {
    uint64_t weight;
    stList *blocks;
} cactusPath;

void collapse3ECNets(stOnlineCactus *cactus,
                     stCactusTree *node1, stCactusTree *node2,
                     stCactusTree **newNode1, stCactusTree **newNode2);

stCactusTree *stCactusTree_construct(stCactusTree *parent, stCactusTree *leftSib, cactusNodeType type, void *block);

void stCactusTree_reroot(stCactusTree *newRoot, stList *trees);

void fix3EC(stOnlineCactus *cactus, stCactusTree *tree);

void updateMaximalChainOrBridgePath(stOnlineCactus *cactus, void *block);

void updateMaximalBridgePathForTree(stOnlineCactus *cactus, stCactusTree *tree);

stList *naivelyGetMaximalChainOrBridgePath(stOnlineCactus *cactus, void *block);

#endif
