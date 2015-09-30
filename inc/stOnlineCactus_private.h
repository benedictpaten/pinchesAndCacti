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
};

struct _stCactusTreeIt {
    stCactusTree *cur;
};

void collapse3ECNets(stOnlineCactus *cactus,
                     stCactusTree *node1, stCactusTree *node2,
                     stCactusTree **newNode1, stCactusTree **newNode2);

stCactusTree *stCactusTree_construct(stCactusTree *parent, stCactusTree *leftSib, cactusNodeType type, void *block);

void stCactusTree_reroot(stCactusTree *newRoot, stList *trees);

#endif
