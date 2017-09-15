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

/*
 * The basic data structures representing a cactus graph
 */

typedef struct _stCactusEdgeEnd stCactusEdgeEnd;

typedef struct _stCactusNode {
	/*
	 * A vertex in the cactus graph
	 */

	// A linked list of the edge ends (sides) in the cactus graph incident with this node
    stCactusEdgeEnd *head;
    stCactusEdgeEnd *tail;

    // The underlying object, e.g. adjacency component of pinchNodes, that "project to" this cactus node
    void *nodeObject;
} stCactusNode;

struct _stCactusEdgeEnd {
	/*
	 * The end of an edge in the cactus graph.
	 * Symmetrically, each edge is composed of two such ends.
	 */

	// The other end (side) of the edge
    stCactusEdgeEnd *otherEdgeEnd;

    // The incident vertex in the cactus graph
    stCactusNode *node;

    // A link in linked-list of edge ends that are incident with the incident node. See
    // stCactusNode->head and stCactusNode->tail
    stCactusEdgeEnd *nEdgeEnd;

    // The underlying object, e.g. a pinchEdgeEnd
    void *endObject;

    // If the edge end is in a snarl, the other edge end in the snarl - which must also
    // be incident with the given cactus node
    stCactusEdgeEnd *link;

    // Variables used to orient the cactus graph
    bool linkOrientation;
    bool isChainEnd;
};

typedef struct _stCactusGraph {
	// A hash from underlying node objects to their given nodes
    stHash *objectToNodeHash;

    // Cleanup functions for the underlying node and edge end objects
    void (*destructNodeObjectFn)(void *);
    void (*destructEdgeEndObjectFn)(void *);
} stCactusGraph;

typedef struct _stCactusNodeEdgeEndIt {
	/*
	 * Iterator over the edge ends incident with a given node in the cactus graph.
	 */
    stCactusEdgeEnd *edgeEnd;
} stCactusNodeEdgeEndIt;

typedef struct _stCactusGraphNodeIterator {
	/*
	 * Iterator over the nodes in a cactus graph
	 */
    stHashIterator *it;
    stCactusGraph *graph;
} stCactusGraphNodeIt;

/*
 * Snarls
 */

typedef struct _stSnarl {
    // The boundaries of the snarl
    stCactusEdgeEnd *edgeEnd1, *edgeEnd2;

    // The chains contained in the snarl
    stList *chains; // Each chain is an stList list of stSnarls in a sequence
    // such that for i > 0, edgeEnd1 of snarl i in the chain is the opposite end to edgeEnd2 of snarl i-1.

    // The unary snarls contained in the snarl, for which edgeEnd1 == edgeEnd2
    stList *unarySnarls;

    // The number of snarls / top-level chains this snarl is contained, vital for memory management
    uint64_t parentCount;

} stSnarl;

typedef struct _stSnarlDecomposition {
	// List of top level chains. These chains may overlap.
	stList *topLevelChains;

	// Top level unary snarls, these are created by handing in a pair of equal bridge ends as telomeres
	stList *topLevelUnarySnarls;

} stSnarlDecomposition;

/*
 * Bridge graph
 */

typedef struct _stBridgeNode stBridgeNode;
struct _stBridgeNode {
    stList *connectedNodes; // (stBridgeNode) Connected bridge nodes
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

int64_t stCactusGraph_getNodeNumber(stCactusGraph *graph);

stSnarlDecomposition *stCactusGraph_getSnarlDecomposition(stCactusGraph *cactusGraph, stList *snarlChainEnds);

stList *stCactusGraph_getTopLevelSnarlChain(stCactusGraph *cactusGraph,
		stCactusEdgeEnd *startEdgeEnd, stCactusEdgeEnd *endEdgeEnd, stSet *snarlCache);

// Bridge graphs

void stBridgeNode_destruct(stBridgeNode *bridgeNode);

stBridgeGraph *stBridgeGraph_getBridgeGraph(stCactusNode *cactusNode);

void stBridgeGraph_destruct(stBridgeGraph *bridgeGraph);

void stBridgeNode_print(stBridgeNode *bridgeNode, FILE *fileHandle);

stHash *stBridgeGraph_getBridgeEdgeEndsToBridgeNodesHash(stBridgeGraph *bridgeGraph);

// Snarls

stSnarl *stSnarl_constructEmptySnarl(stCactusEdgeEnd *edgeEnd1, stCactusEdgeEnd *edgeEnd2);

void stSnarl_destruct(stSnarl *snarl);

stSnarl *stSnarl_makeRecursiveSnarl(stCactusEdgeEnd *edgeEnd1, stCactusEdgeEnd *edgeEnd2,
		stSet *snarlCache);

uint64_t stSnarl_hashKey(const void *snarl);

int stSnarl_equals(const void *snarl1, const void *snarl2);

void stSnarl_print(stSnarl *snarl, FILE *fileHandle);

void stSnarlDecomposition_destruct(stSnarlDecomposition *snarls);

void stSnarlDecomposition_print(stSnarlDecomposition *snarls, FILE *fileHandle);

#ifdef __cplusplus
}
#endif

#endif /* ST_CACTUS_GRAPH_H_ */
