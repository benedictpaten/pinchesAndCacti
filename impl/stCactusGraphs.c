/*
 * stCactusGraph.h
 *
 *  Created on: 14 Apr 2012
 *      Author: benedictpaten
 */

#include <stdlib.h>
#include "sonLib.h"
#include "stCactusGraphs.h"
#include "3_Absorb3edge2x.h"

struct _stCactusNode {
    stCactusEdgeEnd *head;
    stCactusEdgeEnd *tail;
    void *nodeObject;
};

struct _stCactusEdgeEnd {
    stCactusEdgeEnd *otherEdgeEnd;
    stCactusNode *node;
    stCactusEdgeEnd *nEdgeEnd;
    void *endObject;
    stCactusEdgeEnd *link;
    bool linkOrientation;
    bool isChainEnd;
};

struct _stCactusGraph {
    stHash *objectToNodeHash;
    void (*destructNodeObjectFn)(void *);
    void (*destructEdgeEndObjectFn)(void *);
};

//Node functions

stCactusNode *stCactusNode_construct(stCactusGraph *graph, void *nodeObject) {
    stCactusNode *node = st_calloc(1, sizeof(stCactusNode));
    node->nodeObject = nodeObject;
    assert(stHash_search(graph->objectToNodeHash, nodeObject) == NULL);
    stHash_insert(graph->objectToNodeHash, nodeObject, node);
    return node;
}

void *stCactusNode_getObject(stCactusNode *node) {
    return node->nodeObject;
}

stCactusNodeEdgeEndIt stCactusNode_getEdgeEndIt(stCactusNode *node) {
    stCactusNodeEdgeEndIt it;
    it.edgeEnd = node->head;
    return it;
}

stCactusEdgeEnd *stCactusNodeEdgeEndIt_getNext(stCactusNodeEdgeEndIt *it) {
    stCactusEdgeEnd *edgeEnd = it->edgeEnd;
    if (edgeEnd != NULL) {
        it->edgeEnd = it->edgeEnd->nEdgeEnd;
    }
    return edgeEnd;
}

stCactusEdgeEnd *stCactusNode_getFirstEdgeEnd(stCactusNode *node) {
    return node->head;
}

//Private node functions

static void stCactusEdgeEnd_destruct(stCactusEdgeEnd *edge, void(*destructEdgeEndObjectFn)(void *));

static void stCactusNode_destruct(stCactusNode *node, void(*destructNodeObjectFn)(void *), void(*destructEdgeEndObjectFn)(void *)) {
    stCactusNodeEdgeEndIt it = stCactusNode_getEdgeEndIt(node);
    stCactusEdgeEnd *edgeEnd;
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&it)) != NULL) {
        stCactusEdgeEnd_destruct(edgeEnd, destructEdgeEndObjectFn);
    }
    if (destructNodeObjectFn != NULL) {
        destructNodeObjectFn(node->nodeObject);
    }
    free(node);
}

static void stCactusNode_mergeNodes(stCactusGraph *graph, stCactusNode *node1, stCactusNode *node2,
        void *(*mergeNodeObjects)(void *, void *)) {
    if (node1 == node2) {
        return;
    }
    assert(stHash_search(graph->objectToNodeHash, stCactusNode_getObject(node1)) == node1);
    assert(stHash_search(graph->objectToNodeHash, stCactusNode_getObject(node2)) == node2);
    stHash_remove(graph->objectToNodeHash, stCactusNode_getObject(node1));
    stHash_remove(graph->objectToNodeHash, stCactusNode_getObject(node2));
    node1->nodeObject = mergeNodeObjects(node1->nodeObject, node2->nodeObject);
    stHash_insert(graph->objectToNodeHash, stCactusNode_getObject(node1), node1);
    if (node2->head != NULL) {
        if (node1->head == NULL) {
            node1->head = node2->head;
        } else {
            assert(node1->tail->nEdgeEnd == NULL);
            node1->tail->nEdgeEnd = node2->head;
        }
        stCactusEdgeEnd *edgeEnd = node2->head;
        while (edgeEnd != NULL) {
            edgeEnd->node = node1;
            edgeEnd = edgeEnd->nEdgeEnd;
        }
        node1->tail = node2->tail;
    }
    free(node2);
}

//Edge functions

static void connectUpEdgeEnd(stCactusEdgeEnd *edgeEnd, stCactusNode *node, stCactusEdgeEnd *otherEdgeEnd, void *endObject) {
    edgeEnd->node = node;
    edgeEnd->otherEdgeEnd = otherEdgeEnd;
    edgeEnd->endObject = endObject;
    if (node->head == NULL) {
        node->head = edgeEnd;
    } else {
        node->tail->nEdgeEnd = edgeEnd;
    }
    node->tail = edgeEnd;
}

stCactusEdgeEnd *stCactusEdgeEnd_construct(stCactusGraph *graph, stCactusNode *node1, stCactusNode *node2, void *edgeEndObject1,
        void *edgeEndObject2) {
    stCactusEdgeEnd *edgeEnd1 = st_calloc(1, sizeof(stCactusEdgeEnd));
    stCactusEdgeEnd *edgeEnd2 = st_calloc(1, sizeof(stCactusEdgeEnd));

    connectUpEdgeEnd(edgeEnd1, node1, edgeEnd2, edgeEndObject1);
    connectUpEdgeEnd(edgeEnd2, node2, edgeEnd1, edgeEndObject2);
    return edgeEnd1;
}

void *stCactusEdgeEnd_getObject(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->endObject;
}

stCactusNode *stCactusEdgeEnd_getNode(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->node;
}

stCactusNode *stCactusEdgeEnd_getOtherNode(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->otherEdgeEnd->node;
}

stCactusEdgeEnd *stCactusEdgeEnd_getOtherEdgeEnd(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->otherEdgeEnd;
}

stCactusEdgeEnd *stCactusEdgeEnd_getLink(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->link;
}

bool stCactusEdgeEnd_getLinkOrientation(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->linkOrientation;
}

bool stCactusEdgeEnd_isChainEnd(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->isChainEnd;
}

stCactusEdgeEnd *stCactusEdgeEnd_getNextEdgeEnd(stCactusEdgeEnd *edgeEnd) {
    return edgeEnd->nEdgeEnd;
}

//Private edge functions

static void stCactusEdgeEnd_destruct(stCactusEdgeEnd *edgeEnd, void(*destructEdgeEndObjectFn)(void *)) {
    if (destructEdgeEndObjectFn != NULL) {
        destructEdgeEndObjectFn(edgeEnd->endObject);
    }
    free(edgeEnd);
}

static void stCactusEdgeEnd_setLink(stCactusEdgeEnd *edgeEnd, stCactusEdgeEnd *otherEdgeEnd) {
    edgeEnd->link = otherEdgeEnd;
}

static void stCactusEdgeEnd_setLinkOrientation(stCactusEdgeEnd *edgeEnd, bool orientation) {
    edgeEnd->linkOrientation = orientation;
}

static void stCactusEdgeEnd_setIsChainEnd(stCactusEdgeEnd *edgeEnd, bool isChainEnd) {
    edgeEnd->isChainEnd = isChainEnd;
}

//Graph functions

stCactusGraph *stCactusGraph_construct2(void(*destructNodeObjectFn)(void *), void(*destructEdgeEndObjectFn)(void *)) {
    stCactusGraph *cactusGraph = st_malloc(sizeof(stCactusGraph));
    cactusGraph->objectToNodeHash = stHash_construct();
    cactusGraph->destructNodeObjectFn = destructNodeObjectFn;
    cactusGraph->destructEdgeEndObjectFn = destructEdgeEndObjectFn;
    return cactusGraph;
}

stCactusGraph *stCactusGraph_construct() {
    return stCactusGraph_construct2(NULL, NULL);
}

void stCactusGraph_destruct(stCactusGraph *graph) {
    stCactusGraphNodeIt *nodeIt = stCactusGraphNodeIterator_construct(graph);
    stCactusNode *node;
    while ((node = stCactusGraphNodeIterator_getNext(nodeIt)) != NULL) {
        stCactusNode_destruct(node, graph->destructNodeObjectFn, graph->destructEdgeEndObjectFn);
    }
    stCactusGraphNodeIterator_destruct(nodeIt);
    stHash_destruct(graph->objectToNodeHash);
    free(graph);
}

stCactusNode *stCactusGraph_getNode(stCactusGraph *node, void *nodeObject) {
    return stHash_search(node->objectToNodeHash, nodeObject);
}

void stCactusGraph_collapseToCactus(stCactusGraph *graph, void *(*mergeNodeObjects)(void *, void *), stCactusNode *startNode) {
    //Basic data structures
    stHash *nodesToPositions = stHash_construct();
    stHash *positionsToNodes = stHash_construct3((uint64_t(*)(const void *)) stIntTuple_hashKey,
            (int(*)(const void *, const void *)) stIntTuple_equalsFn, (void(*)(void *)) stIntTuple_destruct, NULL);
    stList *adjacencyList = stList_construct3(0, (void(*)(void *)) stList_destruct);

    //Set up and run the three edge connected code.
    stCactusGraphNodeIt *nodeIt = stCactusGraphNodeIterator_construct(graph);
    stCactusNode *node;
    int64_t nodeIdCounter = 0;
    while ((node = stCactusGraphNodeIterator_getNext(nodeIt)) != NULL) {
        stIntTuple *nodeId = stIntTuple_construct1( nodeIdCounter++);
        stHash_insert(nodesToPositions, node, nodeId);
        stHash_insert(positionsToNodes, nodeId, node);
    }
    stCactusGraphNodeIterator_destruct(nodeIt);
    nodeIt = stCactusGraphNodeIterator_construct(graph);
    while ((node = stCactusGraphNodeIterator_getNext(nodeIt)) != NULL) {
        stList *edges = stList_construct();
        stList_append(adjacencyList, edges);
        stCactusNodeEdgeEndIt edgeEndIt = stCactusNode_getEdgeEndIt(node);
        stCactusEdgeEnd *edgeEnd;
        while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeEndIt)) != NULL) {
            stCactusNode *otherNode = stCactusEdgeEnd_getOtherNode(edgeEnd);
            stIntTuple *otherNodeId = stHash_search(nodesToPositions, otherNode);
            assert(otherNodeId != NULL);
            stList_append(edges, otherNodeId);
        }
    }
    stCactusGraphNodeIterator_destruct(nodeIt);
    //Now do the merging
    stList *_3EdgeConnectedComponents = computeThreeEdgeConnectedComponents(adjacencyList);
    for (int64_t i = 0; i < stList_length(_3EdgeConnectedComponents); i++) {
        stList *_3EdgeConnectedComponent = stList_get(_3EdgeConnectedComponents, i);
        stCactusNode *node = stHash_search(positionsToNodes, stList_get(_3EdgeConnectedComponent, 0));
        assert(node != NULL);
        for (int64_t j = 1; j < stList_length(_3EdgeConnectedComponent); j++) {
            stCactusNode *otherNode = stHash_search(positionsToNodes, stList_get(_3EdgeConnectedComponent, j));
            assert(otherNode != NULL);
            assert(node != otherNode);
            if (otherNode == startNode) { //This prevents the start node from being destructed.
                otherNode = node;
                node = startNode;
            }
            stCactusNode_mergeNodes(graph, node, otherNode, mergeNodeObjects);
        }
    }
    //Cleanup
    stList_destruct(adjacencyList);
    stList_destruct(_3EdgeConnectedComponents);
    stHash_destruct(nodesToPositions);
    stHash_destruct(positionsToNodes);

    //Mark the cycles
    stCactusGraph_markCycles(graph, startNode);
}

stCactusGraphNodeIt *stCactusGraphNodeIterator_construct(stCactusGraph *graph) {
    stCactusGraphNodeIt *nodeIt = st_malloc(sizeof(stCactusGraphNodeIt));
    nodeIt->it = stHash_getIterator(graph->objectToNodeHash);
    nodeIt->graph = graph;
    return nodeIt;
}

stCactusNode *stCactusGraphNodeIterator_getNext(stCactusGraphNodeIt *nodeIt) {
    void *key = stHash_getNext(nodeIt->it);
    if (key != NULL) {
        return stHash_search(nodeIt->graph->objectToNodeHash, key);
    }
    return NULL;
}

void stCactusGraphNodeIterator_destruct(stCactusGraphNodeIt *nodeIt) {
    stHash_destructIterator(nodeIt->it);
    free(nodeIt);
}

void stCactusGraph_unmarkCycles(stCactusGraph *graph) {
    stCactusGraphNodeIt *nodeIt = stCactusGraphNodeIterator_construct(graph);
    stCactusNode *node;
    while ((node = stCactusGraphNodeIterator_getNext(nodeIt)) != NULL) {
        stCactusNodeEdgeEndIt edgeEndIt = stCactusNode_getEdgeEndIt(node);
        stCactusEdgeEnd *edgeEnd;
        while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeEndIt)) != NULL) {
            stCactusEdgeEnd_setIsChainEnd(edgeEnd, 0);
            stCactusEdgeEnd_setLink(edgeEnd, NULL);
            stCactusEdgeEnd_setLinkOrientation(edgeEnd, 0);
        }
    }
    stCactusGraphNodeIterator_destruct(nodeIt);
}

static void makeChain(stCactusEdgeEnd *edgeEnd, stCactusEdgeEnd *edgeEnd2, stList *chainPath) {
    assert(edgeEnd != edgeEnd2);
    stCactusEdgeEnd_setLink(edgeEnd, edgeEnd2);
    stCactusEdgeEnd_setLink(edgeEnd2, edgeEnd);
    stCactusEdgeEnd_setIsChainEnd(edgeEnd, 1);
    stCactusEdgeEnd_setIsChainEnd(edgeEnd2, 1);
    stCactusEdgeEnd_setLinkOrientation(edgeEnd2, 1);
    for (int64_t j = stList_length(chainPath) - 1; j >= 0; j -= 2) {
        stCactusEdgeEnd *edgeEnd3 = stList_get(chainPath, j);
        stCactusEdgeEnd *edgeEnd4 = stList_get(chainPath, j - 1);
        assert(edgeEnd3 != edgeEnd2);
        assert(edgeEnd4 != edgeEnd2);
        assert(edgeEnd4 != edgeEnd);
        if (edgeEnd3 == edgeEnd) {
            break;
        }
        stCactusEdgeEnd_setLink(edgeEnd3, edgeEnd4);
        stCactusEdgeEnd_setLink(edgeEnd4, edgeEnd3);
        stCactusEdgeEnd_setLinkOrientation(edgeEnd4, 1);
    }
}

static void stCactusGraph_markCyclesP(stCactusEdgeEnd *edgeEnd, stHash *nodesOnChainToEdgeEnds, stList *chainPath) {
    stList_append(chainPath, edgeEnd);
    stList_append(chainPath, NULL);
    while (stList_length(chainPath) > 0) {
        stCactusEdgeEnd *edgeEnd2 = stList_pop(chainPath);
        edgeEnd = stList_peek(chainPath);
        stCactusNode *node = stCactusEdgeEnd_getNode(edgeEnd);
        if (edgeEnd2 == NULL) {
            edgeEnd2 = stCactusNode_getFirstEdgeEnd(node);
        } else {
            edgeEnd2 = stCactusEdgeEnd_getNextEdgeEnd(edgeEnd2);
        }
        if (edgeEnd2 == NULL) {
            stList_pop(chainPath);
            stHash_remove(nodesOnChainToEdgeEnds, node);
        } else {
            stList_append(chainPath, edgeEnd2);
            if (edgeEnd2 != edgeEnd && stCactusEdgeEnd_getLink(edgeEnd2) == NULL) {
                stHash_insert(nodesOnChainToEdgeEnds, node, edgeEnd2);
                stCactusEdgeEnd *edgeEnd3 = stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd2);
                stCactusEdgeEnd *edgeEnd4;
                if ((edgeEnd4 = stHash_search(nodesOnChainToEdgeEnds, stCactusEdgeEnd_getNode(edgeEnd3))) != NULL) { //We've traversed a cycle
                    makeChain(edgeEnd4, edgeEnd3, chainPath);
                } else {
                    stList_append(chainPath, edgeEnd3);
                    stList_append(chainPath, NULL);
                }
            }
        }
    }
}

void stCactusGraph_markCycles(stCactusGraph *graph, stCactusNode *startNode) {
    stHash *nodesOnChainToEdgeEnds = stHash_construct();
    stList *chainPath = stList_construct();
    stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(startNode);
    stCactusEdgeEnd *edgeEnd;
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
        if (!stCactusEdgeEnd_isChainEnd(edgeEnd)) {
            stCactusEdgeEnd *edgeEnd2 = stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd);
            if (stCactusEdgeEnd_getNode(edgeEnd2) == startNode) {
                makeChain(edgeEnd, edgeEnd2, chainPath);
            } else {
                stHash_insert(nodesOnChainToEdgeEnds, startNode, edgeEnd);
                stCactusGraph_markCyclesP(edgeEnd2, nodesOnChainToEdgeEnds, chainPath);
                stHash_remove(nodesOnChainToEdgeEnds, startNode);
            }
        }
    }
    stList_destruct(chainPath);
    assert(stHash_size(nodesOnChainToEdgeEnds) == 0);
    stHash_destruct(nodesOnChainToEdgeEnds);
}

static void stCactusGraph_collapseBridgesP(stCactusNode *parentNode, stCactusEdgeEnd *edgeEnd, stList *nodesToMerge) {
    stList *stack = stList_construct();
    stList_append(stack, parentNode);
    stList_append(stack, edgeEnd);
    while (stList_length(stack) > 0) {
        edgeEnd = stList_pop(stack);
        parentNode = stList_pop(stack);
        stCactusNode *node = stCactusEdgeEnd_getNode(edgeEnd);
        if (stCactusEdgeEnd_getLink(edgeEnd) == NULL) { //Is a bridge
            //Establish if this is a leaf
            stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(node);
            stCactusEdgeEnd *edgeEnd2;
            int64_t bridges = 0;
            while ((edgeEnd2 = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
                if (edgeEnd2 != edgeEnd) {
                    if (stCactusEdgeEnd_getLink(edgeEnd2) == NULL) {
                        bridges++;
                    }
                    if (!stCactusEdgeEnd_getLinkOrientation(edgeEnd2)) {
                        stList_append(stack, parentNode);
                        stList_append(stack, stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd2));
                    }
                }
            }
            if (bridges != 1) {
                stList_append(nodesToMerge, parentNode);
                stList_append(nodesToMerge, node);
            }
        } else if (!stCactusEdgeEnd_isChainEnd(edgeEnd)) {
            stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(node);
            stCactusEdgeEnd *edgeEnd2;
            while ((edgeEnd2 = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
                if (edgeEnd2 != edgeEnd && !stCactusEdgeEnd_getLinkOrientation(edgeEnd2)) {
                    stList_append(stack, node);
                    stList_append(stack, stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd2));
                }
            }
        }
    }
    stList_destruct(stack);
}

void mergeNodes(stCactusGraph *graph, stCactusNode *startNode, stList *nodesToMerge, void *(*mergeNodeObjects)(void *, void *)) {
    for (int64_t i = 0; i < stList_length(nodesToMerge); i += 2) {
        stCactusNode *parentNode = stList_get(nodesToMerge, i);
        stCactusNode *nodeToMerge = stList_get(nodesToMerge, i + 1);
        stCactusNode_mergeNodes(graph, parentNode, nodeToMerge, mergeNodeObjects);
    }
    stList_destruct(nodesToMerge);
    stCactusGraph_unmarkCycles(graph);
    stCactusGraph_markCycles(graph, startNode);
}

void stCactusGraph_collapseBridges(stCactusGraph *graph, stCactusNode *startNode, void *(*mergeNodeObjects)(void *, void *)) {
    stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(startNode);
    stCactusEdgeEnd *edgeEnd;
    stList *bridgesToMerge = stList_construct();
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
        if (!stCactusEdgeEnd_getLinkOrientation(edgeEnd)) {
            stCactusGraph_collapseBridgesP(startNode, stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd), bridgesToMerge);
        }
    }
    mergeNodes(graph, startNode, bridgesToMerge, mergeNodeObjects); //Now merge bridges
}

int64_t stCactusEdgeEnd_getChainLength(stCactusEdgeEnd *edgeEnd) {
    stCactusEdgeEnd *edgeEnd2 = stCactusEdgeEnd_getLink(stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd));
    int64_t chainLength = 1;
    while (edgeEnd != edgeEnd2) {
        chainLength++;
        edgeEnd2 = stCactusEdgeEnd_getLink(stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd2));
    }
    return chainLength;
}

int64_t stCactusNode_getTotalEdgeLengthOfFlower(stCactusNode *node) {
    stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(node);
    stCactusEdgeEnd *edgeEnd;
    int64_t totalLength = 0;
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
        if (stCactusEdgeEnd_isChainEnd(edgeEnd) && stCactusEdgeEnd_getLinkOrientation(edgeEnd)) {
            totalLength += stCactusEdgeEnd_getChainLength(edgeEnd);
        }
    }
    return totalLength;
}

int64_t stCactusNode_getChainNumber(stCactusNode *node) {
    stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(node);
    stCactusEdgeEnd *edgeEnd;
    int64_t totalNumber = 0;
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
        if (stCactusEdgeEnd_isChainEnd(edgeEnd) && stCactusEdgeEnd_getLinkOrientation(edgeEnd)) {
            totalNumber++;
        }
    }
    return totalNumber;
}

static void stCactusGraph_collapseLongChainsP(stCactusNode *node, stList *nodesToMerge, int64_t longChain) {
    stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(node);
    stCactusEdgeEnd *edgeEnd;
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
        if (stCactusEdgeEnd_isChainEnd(edgeEnd) && stCactusEdgeEnd_getLinkOrientation(edgeEnd) && stCactusEdgeEnd_getChainLength(edgeEnd)
                > longChain) {
            stCactusNode *node2 = stCactusEdgeEnd_getNode(stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd));
            stCactusNode *node3 = stCactusEdgeEnd_getNode(stCactusEdgeEnd_getOtherEdgeEnd(stCactusEdgeEnd_getLink(edgeEnd)));
            if (node2 != node3) {
                assert(node2 != node);
                assert(node3 != node);
                stList_append(nodesToMerge, node3);
                stList_append(nodesToMerge, node2); //subtle, but this ordering ensures that when
                //we remark the nodes with orientations they end up with different orientations between the edges that are
                //really linked but now merged in the same node.
            } else {
                assert(longChain <= 1);
            }
        }
    }
}

stSet *stCactusGraph_collapseLongChainsOfBigFlowers(stCactusGraph *graph, stCactusNode *startNode, int64_t chainLengthForBigFlower,
        int64_t longChain, void *(*mergeNodeObjects)(void *, void *), bool recursive) {
    int64_t nodesMerged = 0;
    stSet *bigNodes = stSet_construct();
    do {
        stCactusGraphNodeIt *it = stCactusGraphNodeIterator_construct(graph);
        stCactusNode *node;
        stList *chainNodesToMerge = stList_construct();
        while ((node = stCactusGraphNodeIterator_getNext(it))) {
            if (stCactusNode_getTotalEdgeLengthOfFlower(node) > chainLengthForBigFlower) {
                stCactusGraph_collapseLongChainsP(node, chainNodesToMerge, longChain);
                stSet_insert(bigNodes, node);
            }
        }
        stCactusGraphNodeIterator_destruct(it);
        if(stList_length(chainNodesToMerge) > 0) { //We go around because the result of the merge may require more merges.
            nodesMerged += stList_length(chainNodesToMerge)/2;
            mergeNodes(graph, startNode, chainNodesToMerge, mergeNodeObjects); //Now merge bridges
        }
        else {
            stList_destruct(chainNodesToMerge);
            break;
        }
    } while(recursive);
    return bigNodes;
}

/*
 * Following functions are used for breaking chains by edge-ends that can-not be part of a link, i.e. because they have an incident effective self loops.
 */

static void stCactusEdgeEnd_mergeIncidentNodesInMarkedCactusGraph(stCactusGraph *graph,
        stCactusEdgeEnd *leftEdgeEnd, stCactusEdgeEnd *rightEdgeEnd,
        void *(*mergeNodeObjects)(void *, void *)) {
    /*
     * Merges together the nodes incident with the given edge ends, if distinct, updating the
     * chain links in the process.
     */
    if(stCactusEdgeEnd_getNode(leftEdgeEnd) != stCactusEdgeEnd_getNode(rightEdgeEnd)) {
        stCactusNode_mergeNodes(graph, stCactusEdgeEnd_getNode(leftEdgeEnd), stCactusEdgeEnd_getNode(rightEdgeEnd), mergeNodeObjects);
        assert(leftEdgeEnd->link != rightEdgeEnd);
        assert(rightEdgeEnd->link != leftEdgeEnd);
        assert(rightEdgeEnd->link != leftEdgeEnd->link);
        assert(stCactusEdgeEnd_getNode(leftEdgeEnd) == stCactusEdgeEnd_getNode(rightEdgeEnd));

        //Deal with the edge ends connected to their links
        leftEdgeEnd->link->link = rightEdgeEnd->link;
        rightEdgeEnd->link->link = leftEdgeEnd->link;
        leftEdgeEnd->link->isChainEnd = leftEdgeEnd->link->isChainEnd || rightEdgeEnd->link->isChainEnd;
        rightEdgeEnd->link->isChainEnd = leftEdgeEnd->link->isChainEnd;

        assert(leftEdgeEnd->link->link == rightEdgeEnd->link);
        assert(rightEdgeEnd->link->link == leftEdgeEnd->link);
        assert(leftEdgeEnd->link->linkOrientation != rightEdgeEnd->link->linkOrientation);
        assert(stCactusEdgeEnd_getNode(leftEdgeEnd->link) == stCactusEdgeEnd_getNode(rightEdgeEnd->link));

        //Deal with making the chain circular
        leftEdgeEnd->link = rightEdgeEnd;
        rightEdgeEnd->link = leftEdgeEnd;
        leftEdgeEnd->isChainEnd = 1;
        rightEdgeEnd->isChainEnd = 1;

        assert(leftEdgeEnd->link == rightEdgeEnd);
        assert(rightEdgeEnd->link == leftEdgeEnd);
        assert(leftEdgeEnd->linkOrientation != rightEdgeEnd->linkOrientation);
    }
}

static bool getNestedEdgeEndsNotInChain(stCactusEdgeEnd *edgeEnd,
        stCactusEdgeEnd **leftEdgeEnd, stCactusEdgeEnd **rightEdgeEnd,
        bool (*endIsNotInChain)(stCactusEdgeEnd *, void *extraArg), void *extraArg) {
    /*
     * Finds a pair of edge-ends for which (1) each end either returns non-zero for endIsNotInChain or is an edge-end at the end of chain
     * and (2) the ends are in the chain that linked by a chain of cactus edges and links, starting on both ends with cactus edges.
     * If no such pair exists, except for the very ends of the chain, then returns 0, else returns 1.
     */
    assert(stCactusEdgeEnd_getLinkOrientation(edgeEnd));
    *leftEdgeEnd = edgeEnd;
    stCactusNode *startNode = stCactusEdgeEnd_getNode(edgeEnd);
    while(1) {
        *rightEdgeEnd = stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd);
        if(startNode == stCactusEdgeEnd_getNode(*rightEdgeEnd)) { //looped around
            return stCactusEdgeEnd_getNode(*leftEdgeEnd) != startNode; //We have distinct node to merge.
        }
        if(endIsNotInChain(*rightEdgeEnd, extraArg)) {
            return 1;
        }
        edgeEnd = stCactusEdgeEnd_getLink(*rightEdgeEnd);
        if(endIsNotInChain(edgeEnd, extraArg)) {
            *leftEdgeEnd = edgeEnd;
        }
    }
}

static stCactusNode *stCactusGraph_breakChainByEndsNotInChain(stCactusGraph *graph,
        stCactusEdgeEnd *edgeEnd, void *(*mergeNodeObjects)(void *, void *),
        bool (*endIsNotInChain)(stCactusEdgeEnd *, void *extraArg), void *extraArg) {
    /*
     * The edgeEnd argument represents the end of a chain. Method breaks up the chain so that the resulting set of chains
     * contains no internal edge end for which endIsNotInChain is true. This is deterministic and results in the resulting set of chains being maximal.
     */
    assert(stCactusEdgeEnd_isChainEnd(edgeEnd));
    assert(stCactusEdgeEnd_getLink(edgeEnd) != NULL);
    assert(stCactusEdgeEnd_getLinkOrientation(edgeEnd));

    //get location of first pair of nested parentheses
    stCactusEdgeEnd *leftEdgeEnd, *rightEdgeEnd;
    bool hasPair = getNestedEdgeEndsNotInChain(edgeEnd, &leftEdgeEnd, &rightEdgeEnd, endIsNotInChain, extraArg);

    //while pair of parentheses exists
    while(hasPair) {
        assert(stCactusEdgeEnd_getNode(leftEdgeEnd) != stCactusEdgeEnd_getNode(rightEdgeEnd));
        assert(stCactusEdgeEnd_getLinkOrientation(leftEdgeEnd));
        assert(!stCactusEdgeEnd_getLinkOrientation(rightEdgeEnd));
        //if we are going to brake the chain into two by merging an internal node with "node", then we add
        if(stCactusEdgeEnd_getNode(leftEdgeEnd) == stCactusEdgeEnd_getNode(edgeEnd)) {
            assert(stCactusEdgeEnd_isChainEnd(leftEdgeEnd));
            assert(!stCactusEdgeEnd_isChainEnd(rightEdgeEnd));
            stCactusEdgeEnd *nextChainEdgeEnd = stCactusEdgeEnd_getLink(rightEdgeEnd);
            stCactusEdgeEnd_mergeIncidentNodesInMarkedCactusGraph(graph, leftEdgeEnd, rightEdgeEnd, mergeNodeObjects);
            stCactusGraph_breakChainByEndsNotInChain(graph, nextChainEdgeEnd, mergeNodeObjects, endIsNotInChain, extraArg);
            hasPair = getNestedEdgeEndsNotInChain(edgeEnd, &leftEdgeEnd, &rightEdgeEnd, endIsNotInChain, extraArg);
        }
        else if(stCactusEdgeEnd_getNode(rightEdgeEnd) == stCactusEdgeEnd_getNode(edgeEnd)) {
            assert(!stCactusEdgeEnd_isChainEnd(leftEdgeEnd));
            assert(stCactusEdgeEnd_isChainEnd(rightEdgeEnd));
            stCactusEdgeEnd_mergeIncidentNodesInMarkedCactusGraph(graph, leftEdgeEnd, rightEdgeEnd, mergeNodeObjects);
            stCactusGraph_breakChainByEndsNotInChain(graph, leftEdgeEnd, mergeNodeObjects, endIsNotInChain, extraArg);
            hasPair = getNestedEdgeEndsNotInChain(edgeEnd, &leftEdgeEnd, &rightEdgeEnd, endIsNotInChain, extraArg);
        }
        else { //Two internal nodes
            assert(!stCactusEdgeEnd_isChainEnd(leftEdgeEnd));
            assert(!stCactusEdgeEnd_isChainEnd(rightEdgeEnd));
            stCactusEdgeEnd *nextChainEdgeEnd = stCactusEdgeEnd_getLink(rightEdgeEnd);
            stCactusEdgeEnd_mergeIncidentNodesInMarkedCactusGraph(graph, leftEdgeEnd, rightEdgeEnd, mergeNodeObjects);
            hasPair = getNestedEdgeEndsNotInChain(nextChainEdgeEnd, &leftEdgeEnd, &rightEdgeEnd, endIsNotInChain, extraArg);
            if(!hasPair) {
                hasPair = getNestedEdgeEndsNotInChain(edgeEnd, &leftEdgeEnd, &rightEdgeEnd, endIsNotInChain, extraArg);
            }
        }
        assert(stCactusEdgeEnd_isChainEnd(edgeEnd));
    }
    return stCactusEdgeEnd_getNode(edgeEnd);
}

stList *stCactusNode_getChains(stCactusNode *startNode) {
    /*
     * Collate a static list of chains attached to the node.
     */
    stList *chainStack = stList_construct();
    stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(startNode);
    stCactusEdgeEnd *edgeEnd;
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
        if (stCactusEdgeEnd_isChainEnd(edgeEnd) && stCactusEdgeEnd_getLinkOrientation(edgeEnd)) {
            stList_append(chainStack, edgeEnd);
        }
    }
    return chainStack;
}

stCactusNode *stCactusGraph_breakChainsByEndsNotInChains(stCactusGraph *graph,
        stCactusNode *startNode, void *(*mergeNodeObjects)(void *, void *),
        bool (*endIsNotInChain)(stCactusEdgeEnd *, void *extraArg), void *extraArg) {
    /*
     * Removes all ends that return non-zero from endIsNotInChain from within internal links within chains, breaking them down deterministically and correctly, in that
     * all results chains are maximal given this constraint. Returns the startNode, which may change die to merges.
     */
    //First call function recursively
    stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(startNode);
    stCactusEdgeEnd *edgeEnd;
    while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
        assert(stCactusEdgeEnd_getNode(edgeEnd) == startNode);
        if (stCactusEdgeEnd_isChainEnd(edgeEnd) && stCactusEdgeEnd_getLinkOrientation(edgeEnd)) {
            stCactusEdgeEnd *edgeEnd2 = stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd);
            while(stCactusEdgeEnd_getNode(edgeEnd2) != startNode) {
                stCactusGraph_breakChainsByEndsNotInChains(graph, stCactusEdgeEnd_getNode(edgeEnd2), mergeNodeObjects, endIsNotInChain, extraArg);
                edgeEnd2 = stCactusEdgeEnd_getOtherEdgeEnd(stCactusEdgeEnd_getLink(edgeEnd2));
            }
            assert(stCactusEdgeEnd_getLink(edgeEnd2) == edgeEnd);
            assert(stCactusEdgeEnd_isChainEnd(edgeEnd2));
            assert(!stCactusEdgeEnd_getLinkOrientation(edgeEnd2));
            assert(stCactusEdgeEnd_isChainEnd(edgeEnd));
            assert(stCactusEdgeEnd_getLinkOrientation(edgeEnd));
        }
    }
    //Now process the chains we have for this node
    stList *chainStack = stCactusNode_getChains(startNode);
    while(stList_length(chainStack) > 0) {
        startNode = stCactusGraph_breakChainByEndsNotInChain(graph, stList_pop(chainStack), mergeNodeObjects, endIsNotInChain, extraArg);
    }
    stList_destruct(chainStack);
    return startNode;
}

