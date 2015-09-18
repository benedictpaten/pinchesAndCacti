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
    // NULL if this is a chain node.
    // This is needed because we don't have the guarantee (yet, anyway) that
    // the connected components have consistent pointers over time.
    stSet *ends;
};

struct _stCactusTreeEdge {
    stCactusTree *child;
    void *block;
};

struct _stOnlineCactus {
    stList *trees;
    stHash *blockToEdge;
    stHash *endToNode;
    void *(*edgeToEnd)(void *, bool);
    void *(*endToEdge)(void *);
    stConnectivity *adjacencyComponents;
};

struct _stCactusTreeIt {
    stCactusTree *cur;
};

void collapse3ECNets(stCactusTree *node1, stCactusTree *node2,
                     stCactusTree **newNode1, stCactusTree **newNode2,
                     stHash *endToNode);

stCactusTree *stCactusTree_construct(stCactusTree *parent, stCactusTree *leftSib, cactusNodeType type, void *block);

void stCactusTree_reroot(stCactusTree *newRoot, stList *trees);

#endif
