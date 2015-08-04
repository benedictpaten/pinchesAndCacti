#ifndef __ST_ONLINE_CACTUS_H_
#define __ST_ONLINE_CACTUS_H_

typedef enum {
    NODE,
    CHAIN,
    BRIDGE
} cactusNodeType;

typedef struct _stOnlineCactus stOnlineCactus;

typedef struct _stCactusTree stCactusTree;

typedef struct _stCactusTreeIt stCactusTreeIt;

stOnlineCactus *stOnlineCactus_construct(stConnectivity *connectivity,
                                         void *(*edgeToEnd)(void *, bool),
                                         void *(*endToEdge)(void *));

stCactusTree *stOnlineCactus_getCactusTree(stOnlineCactus *cactus);

cactusNodeType stCactusTree_type(const stCactusTree *tree);

stCactusTreeIt *stCactusTree_getIt(stCactusTree *tree);

stCactusTree *stCactusTreeIt_getNext(stCactusTreeIt *it);

void stCactusTreeIt_destruct(stCactusTreeIt *it);

void stOnlineCactus_splitEdgeHorizontally(stOnlineCactus *cactus, void *oldEdge,
                                          void *oldEndL, void *oldEndR,
                                          void *newEdgeL, void *newEdgeR);

void stOnlineCactus_mergeAdjacentEdges(stOnlineCactus *cactus, void *oldEdgeL,
                                       void *oldEdgeR, void *newEdge);

void stOnlineCactus_alignEdges(stOnlineCactus *cactus, void *edge1, void *edge2,
                               void *newEdge);

void stOnlineCactus_print(const stOnlineCactus *cactus);

#endif
