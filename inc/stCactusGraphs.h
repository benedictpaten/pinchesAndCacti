/*
 * stCactusGraph.h
 *
 *  Created on: 14 Apr 2012
 *      Author: benedictpaten
 */

#ifndef ST_CACTUS_GRAPH_H_
#define ST_CACTUS_GRAPH_H_

#include "sonLib.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _stCactusNode stCactusNode;

typedef struct _stCactusEdgeEnd stCactusEdgeEnd;

typedef struct _stCactusNodeEdgeEndIt {
    stCactusEdgeEnd *edgeEnd;
} stCactusNodeEdgeEndIt;

typedef struct _stCactusGraph stCactusGraph;

typedef struct _stCactusGraphNodeIterator {
    stHashIterator *it;
    stCactusGraph *graph;
} stCactusGraphNodeIt;

// Ultra bubbles
typedef struct _stUltraBubble stUltraBubble;
struct _stUltraBubble {
    stList *chains; // Each chain is an stList list of ultrabubbles in a sequence
    // such that for i > 0, edgeEnd1 of ultrabubble i in the chain is the opposite end to edgeEnd2 of ultrabubble i-1.
    stCactusEdgeEnd *edgeEnd1, *edgeEnd2;
};

typedef struct _stBridgeNode stBridgeNode;
struct _stBridgeNode {
    stList *connectedNodes; // (stBridgNode) Connected bridge nodes
    stSet *bridgeEnds; // (stCactusEdgeEnd) The ends of the bridges in the cactus graph
    // that project to the node
    stSet *cactusNodes; // (stCactusNode) The cactus nodes which project to the node
    stSet *connectedCactusNodes; // (stCactusNode) The set of cactus nodes on paths
    // between the bridge ends or incident with the bridge end
};

typedef struct _stBridgeGraph stBridgeGraph;
struct _stBridgeGraph {
    stList *bridgeNodes; // List of bridge nodes in the bridge graph
};

//Node functions

stCactusNode *stCactusNode_construct(stCactusGraph *graph,
        void *nodeObject);

void *stCactusNode_getObject(stCactusNode *node);

stCactusNodeEdgeEndIt stCactusNode_getEdgeEndIt(stCactusNode *node);

stCactusEdgeEnd *stCactusNodeEdgeEndIt_getNext(stCactusNodeEdgeEndIt *it);

stCactusEdgeEnd *stCactusNode_getFirstEdgeEnd(stCactusNode *node);

int64_t stCactusNode_getTotalEdgeLengthOfFlower(stCactusNode *node);

int64_t stCactusNode_getChainNumber(stCactusNode *node);

//Edge functions

stCactusEdgeEnd *stCactusEdgeEnd_construct(stCactusGraph *graph,
        stCactusNode *node1, stCactusNode *node2, void *edgeEndObject1,
        void *edgeEndObject2);

void *stCactusEdgeEnd_getObject(stCactusEdgeEnd *edgeEnd);

stCactusNode *stCactusEdgeEnd_getNode(stCactusEdgeEnd *edgeEnd);

stCactusNode *stCactusEdgeEnd_getOtherNode(stCactusEdgeEnd *edgeEnd);

stCactusEdgeEnd *stCactusEdgeEnd_getOtherEdgeEnd(stCactusEdgeEnd *edgeEnd);

stCactusEdgeEnd *stCactusEdgeEnd_getLink(stCactusEdgeEnd *edgeEnd);

bool stCactusEdgeEnd_getLinkOrientation(stCactusEdgeEnd *edgeEnd);

bool stCactusEdgeEnd_isChainEnd(stCactusEdgeEnd *edgeEnd);

stCactusEdgeEnd *stCactusEdgeEnd_getNextEdgeEnd(stCactusEdgeEnd *edgeEnd);

int64_t stCactusEdgeEnd_getChainLength(stCactusEdgeEnd *edgeEnd);

void stCactusEdgeEnd_demote(stCactusEdgeEnd *edgeEnd);

//Graph functions

stCactusGraph *stCactusGraph_construct2(void (*destructNodeObjectFn)(void *), void (*destructEdgeEndObjectFn)(void *));

stCactusGraph *stCactusGraph_construct(void);

void stCactusGraph_collapseToCactus(
        stCactusGraph *graph, void *(*mergeNodeObjects)(void *, void *), stCactusNode *startNode);

stCactusNode *stCactusGraph_getNode(stCactusGraph *node, void *nodeObject);

void stCactusGraph_destruct(stCactusGraph *graph);

stCactusGraphNodeIt *stCactusGraphNodeIterator_construct(stCactusGraph *graph);

stCactusNode *stCactusGraphNodeIterator_getNext(stCactusGraphNodeIt *nodeIt);

void stCactusGraphNodeIterator_destruct(stCactusGraphNodeIt *);

void stCactusGraph_unmarkCycles(stCactusGraph *graph);

void stCactusGraph_markCycles(stCactusGraph *graph, stCactusNode *startNode);

void stCactusGraph_collapseBridges(stCactusGraph *graph,
        stCactusNode *startNode, void *(*mergeNodeObjects)(void *, void *));

stSet *stCactusGraph_collapseLongChainsOfBigFlowers(stCactusGraph *graph, stCactusNode *startNode, int64_t chainLengthForBigFlower,
        int64_t longChain, void *(*mergeNodeObjects)(void *, void *), bool recursive);

//Functions used for getting rid of reverse tandem dups
stCactusNode *stCactusGraph_breakChainsByEndsNotInChains(stCactusGraph *graph,
        stCactusNode *startNode, void *(*mergeNodeObjects)(void *, void *),
        bool (*endIsNotInChain)(stCactusEdgeEnd *, void *), void *extraArg);

// Get the connected components, each represented as a set of nodes.
stList *stCactusGraph_getComponents(stCactusGraph *cactusGraph, bool ignoreBridgeEdges);

stHash *stCactusGraphComponents_getNodesToComponentsMap(stList *components);

// Used to compute ultrabubbles, startNode may be NULL.
stList *stCactusGraph_getUltraBubbles(stCactusGraph *graph, stCactusNode *startNode);

int64_t stCactusGraph_getNodeNumber(stCactusGraph *graph);

// Bridge graphs

void stBridgeNode_destruct(stBridgeNode *bridgeNode);

stBridgeGraph *stBridgeGraph_getBridgeGraph(stCactusNode *cactusNode);

void stBridgeGraph_destruct(stBridgeGraph *bridgeGraph);

void stBridgeNode_print(stBridgeNode *bridgeNode, FILE *fileHandle);

// Ultrabubbles

stUltraBubble *stUltraBubble_construct(stList *parentChain,
        stCactusEdgeEnd *edgeEnd1, stCactusEdgeEnd *edgeEnd2);

void stUltraBubble_destruct(stUltraBubble *ultraBubble);

void stUltraBubble_print(stUltraBubble *ultraBubble, FILE *fileHandle);

void stUltraBubble_printChains(stList *ultraBubbleChains, FILE *fileHandle);


#ifdef __cplusplus
}
#endif

#endif /* ST_CACTUS_GRAPH_H_ */
