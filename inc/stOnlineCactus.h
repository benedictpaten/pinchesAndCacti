#ifndef __ST_ONLINE_CACTUS_H_
#define __ST_ONLINE_CACTUS_H_

typedef struct _stOnlineCactus stOnlineCactus;

stOnlineCactus *stOnlineCactus_construct(stConnectivity *connectivity,
                                         void *(*edgeToEnd)(void *, bool),
                                         void *(*endToEdge)(void *));

void stOnlineCactus_splitEdgeHorizontally(stOnlineCactus *cactus, void *oldEdge,
                                          void *oldEndL, void *oldEndR,
                                          void *newEdgeL, void *newEdgeR);

void stOnlineCactus_mergeAdjacentEdges(stOnlineCactus *cactus, void *oldEdgeL,
                                       void *oldEdgeR, void *newEdge);

void stOnlineCactus_alignEdges(stOnlineCactus *cactus, void *edge1, void *edge2,
                               void *newEdge);

void stOnlineCactus_print(const stOnlineCactus *cactus);

#endif
