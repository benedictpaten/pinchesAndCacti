/*
 * stCactusGraph.h
 *
 *  Created on: 14 Apr 2012
 *      Author: benedictpaten
 */

#include "sonLib.h"
#include "stCactusGraphs.h"
#include "3_Absorb3edge2x.h"
#include <stdio.h>
#include <stdlib.h>

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

int64_t stCactusGraph_getNodeNumber(stCactusGraph *graph) {
    return stHash_size(graph->objectToNodeHash);
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
    stList *components = stCactusGraph_getComponents(graph, 0);
    for(int64_t i=0; i<stList_length(components); i++) {
        stSet *component = stList_get(components, i);
        if(startNode != NULL && stSet_search(component, startNode) == startNode) {
            stCactusGraph_markCycles(graph, startNode);
        }
        else {
            // Pick an arbitrary node as the start node for the component
            assert(stSet_size(component) > 0);
            stCactusGraph_markCycles(graph, stSet_peek(component));
        }
    }
    stList_destruct(components);

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
        assert(stCactusEdgeEnd_getNode(leftEdgeEnd->link) == stCactusEdgeEnd_getNode(rightEdgeEnd->link));
        assert(stCactusEdgeEnd_getNode(leftEdgeEnd) == stCactusEdgeEnd_getNode(rightEdgeEnd->link));

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
    while(1) {
        *rightEdgeEnd = stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd);
        if(stCactusEdgeEnd_isChainEnd(*rightEdgeEnd)) { //looped to beginning of chain
            return stCactusEdgeEnd_getNode(*leftEdgeEnd) != stCactusEdgeEnd_getNode(*rightEdgeEnd) &&
                    (stCactusEdgeEnd_isChainEnd(*leftEdgeEnd) || endIsNotInChain(*leftEdgeEnd, extraArg)); //We have distinct node to merge and the left end is a chain.
        }
        if(endIsNotInChain(*rightEdgeEnd, extraArg)) {
            return 1;
        }
        edgeEnd = stCactusEdgeEnd_getLink(*rightEdgeEnd);
        if(endIsNotInChain(edgeEnd, extraArg)) {
            *leftEdgeEnd = edgeEnd;
        }
    }
    return 0;
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
     * all resulting chains are maximal given this constraint. Returns the startNode, which may change due to merges.
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

static void buildComponent(stSet *component, stCactusNode *cactusNode, bool ignoreBridgeEdges) {
    stList *stack = stList_construct(); // List of nodes to be visited
    stList_append(stack, cactusNode);

    // While there exists nodes to visit
    while(stList_length(stack) > 0) {
        stCactusNode *cactusNode2 = stList_pop(stack);

        // If not already in the component
        if(stSet_search(component, cactusNode2) == NULL) {

            // Add to the component
            stSet_insert(component, cactusNode2);

            // Recursively add connected nodes
            stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(cactusNode2);
            stCactusEdgeEnd *edgeEnd;
            while ((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
                assert(cactusNode2 == stCactusEdgeEnd_getNode(edgeEnd));

                // If not ignoreBridgeEdges or is not a bridge edge
                if(!ignoreBridgeEdges || stCactusEdgeEnd_getLink(edgeEnd) != NULL) {
                    // Add to nodes to visit
                    stList_append(stack, stCactusEdgeEnd_getNode(stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd)));
                }
            }
        }
    }

    stList_destruct(stack);
}

stList *stCactusGraph_getComponents(stCactusGraph *cactusGraph,
                                    bool ignoreBridgeEdges) {
    stSet *seen = stSet_construct(); // Set of nodes already visited
    stList *components = stList_construct3(0, (void (*)(void *))stSet_destruct); // Components

    // For each node in the cactus graph
    stCactusGraphNodeIt *nodeIt = stCactusGraphNodeIterator_construct(cactusGraph);
    stCactusNode *cactusNode;
    while((cactusNode = stCactusGraphNodeIterator_getNext(nodeIt)) != NULL) {

        // Check if the node has not already been seen, otherwise it is already in
        // a component
        if(stSet_search(seen, cactusNode) == NULL) {

            // Make a new component
            stSet *component = stSet_construct();
            stList_append(components, component);

            // Recursively build the component
            buildComponent(component, cactusNode, ignoreBridgeEdges);

            // Add set of nodes to seen
            stSet_insertAll(seen, component);

            assert(stSet_search(seen, cactusNode) == cactusNode);
        }
    }

    // Cleanup
    stCactusGraphNodeIterator_destruct(nodeIt);
    stSet_destruct(seen);

    return components;
}

stHash *stCactusGraphComponents_getNodesToComponentsMap(stList *components) {
    /*
     * For set of components, as computed by stCactusGraph_getComponents returns
     * map (hash) of cactus nodes to their respective component (each a stSet).
     */
    stHash *nodesToComponents = stHash_construct();
    for(int64_t i=0; i<stList_length(components); i++) {
        stSet *component = stList_get(components, i);
        stSetIterator *componentIt = stSet_getIterator(component);
        stCactusNode *cactusNode;
        while((cactusNode = stSet_getNext(componentIt)) != NULL) {
            stHash_insert(nodesToComponents, cactusNode, component);
        }
        stSet_destructIterator(componentIt);
    }
    return nodesToComponents;
}

/*
 * Bridge graphs
 */

stBridgeNode *stBridgeNode_construct() {
    stBridgeNode *bridgeNode = st_malloc(sizeof(stBridgeNode));
    bridgeNode->connectedNodes = stList_construct();
    bridgeNode->bridgeEnds = stSet_construct();
    bridgeNode->cactusNodes = stSet_construct();
    bridgeNode->connectedCactusNodes = stSet_construct(); // Cactus nodes on a path between pair of bridges in cactus tree

    return bridgeNode;
}

void stBridgeNode_destruct(stBridgeNode *bridgeNode) {
    stList_destruct(bridgeNode->connectedNodes);
    stSet_destruct(bridgeNode->bridgeEnds);
    stSet_destruct(bridgeNode->cactusNodes);
    stSet_destruct(bridgeNode->connectedCactusNodes);
    free(bridgeNode);
}

static void constructConnectedNodes(stBridgeNode *bridgeNode, stCactusEdgeEnd *edgeEnd) {
	/*
	 * Adds the subset of nodes in the 2-EC component containing bridgeNode that are on a path in the
	 * cactus tree between bridge edges to the set of connected nodes, bridgeNode->connectedCactusNodes
	 */

    // Stack object containing pairs of cactus edge ends
    stList *stack = stList_construct();

    // Initialise
    stList_append(stack, edgeEnd);
    stList_append(stack, edgeEnd);

    // Map of edge ends to booleans indicating if the node incident with the edge end
    // is either incident with a bridge edge or on a path between two bridge edge ends
    stHash *edgeEndsToConnectedStatus = stHash_construct2(NULL, free);

    while(stList_length(stack) > 0) {
        // Get the edge end and its predecessor edge end
        edgeEnd = stList_pop(stack);
        stCactusEdgeEnd *pEdgeEnd = stList_pop(stack);

        // Get the incident node
        stCactusNode *cactusNode = stCactusEdgeEnd_getNode(edgeEnd);

        // Boolean flag to indicate if incident with a bridge edge end or on a path between
        // two bridge edge ends
        bool *onConnectedPath;

        // First time through this will be true
        if((onConnectedPath = stHash_search(edgeEndsToConnectedStatus, edgeEnd)) == NULL) {

            // Initialise onConnectedPath
            onConnectedPath = st_malloc(sizeof(bool));
            stCactusEdgeEnd *linkEdgeEnd = stCactusEdgeEnd_getLink(edgeEnd);
            assert(linkEdgeEnd == NULL || stCactusEdgeEnd_getNode(linkEdgeEnd) == cactusNode);
            // If has not link then is an edge end of a bridge and so node is on a connected path
            onConnectedPath[0] = linkEdgeEnd == NULL;

            // Add back to the stack to process after all its successors
            stList_append(stack, pEdgeEnd);
            stList_append(stack, edgeEnd);
            stHash_insert(edgeEndsToConnectedStatus, edgeEnd, onConnectedPath);

            // For each edge incident with cactusNode
            stCactusNodeEdgeEndIt edgeIterator = stCactusNode_getEdgeEndIt(cactusNode);
            stCactusEdgeEnd *edgeEnd2;
            while ((edgeEnd2 = stCactusNodeEdgeEndIt_getNext(&edgeIterator))) {
                assert(cactusNode == stCactusEdgeEnd_getNode(edgeEnd2));

                // If not the incoming chain
                if(edgeEnd2 != edgeEnd && edgeEnd2 != linkEdgeEnd) {

                    // If part of a chain
                    if(stCactusEdgeEnd_getLink(edgeEnd2) != NULL) {

                        // If positive orientation (to avoid going around twice)
                        if(stCactusEdgeEnd_getLinkOrientation(edgeEnd2)) {

                            // For each other node in the chain
                            stCactusEdgeEnd *edgeEnd3 = stCactusEdgeEnd_getLink(stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd2));
                            while (edgeEnd3 != edgeEnd2) {
                                assert(stCactusEdgeEnd_getNode(edgeEnd3) != cactusNode);

                                // Add to the stack
                                stList_append(stack, edgeEnd);
                                stList_append(stack, edgeEnd3);

                                edgeEnd3 = stCactusEdgeEnd_getLink(stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd3));
                            }
                        }
                    }
                    // Else is a bridge edge end, so on a connected path
                    else {
                        onConnectedPath[0] = 1;
                    }
                }
            }
        }
        else {
            // If on a connected path add to the bridge node's connected nodes.
            if(onConnectedPath[0]) {
                stSet_insert(bridgeNode->connectedCactusNodes, cactusNode);

                // Search for the predecessor and update its connected path variable to
                // indicate it is also on a connected path
                onConnectedPath = stHash_search(edgeEndsToConnectedStatus, pEdgeEnd);
                assert(onConnectedPath != NULL);
                onConnectedPath[0] = 1;
            }
        }
    }

    // Cleanup
    stList_destruct(stack);
    stHash_destruct(edgeEndsToConnectedStatus);
}

static void getBridges(stBridgeNode *bridgeNode) {
    /*
     * Compute set of bridge edge ends to bridgeNode->bridgeEnds.
     */

    // For each cactus node in bridgeNode->cactusNodes
    stSetIterator *nodeIt = stSet_getIterator(bridgeNode->cactusNodes);
    stCactusNode *cactusNode;
    while((cactusNode = stSet_getNext(nodeIt)) != NULL) {

        // For each incident edge end
        stCactusNodeEdgeEndIt edgeIt = stCactusNode_getEdgeEndIt(cactusNode);
        stCactusEdgeEnd *edgeEnd;
        while((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeIt)) != NULL) {
            if(stCactusEdgeEnd_getLink(edgeEnd) == NULL) {

                // Add to the set
                stSet_insert(bridgeNode->bridgeEnds, edgeEnd);
            }
        }
    }
    stSet_destructIterator(nodeIt);
}

void stBridgeNode_print(stBridgeNode *bridgeNode, FILE *fileHandle) {
    // Print nodes and edges indicating bridges and chain edges.
    fprintf(fileHandle, "Bridge node graph: %" PRIi64 " nodes, %" PRIi64 " bridges, %" PRIi64 " bridge connecting nodes\n",
            stSet_size(bridgeNode->cactusNodes),
            stSet_size(bridgeNode->bridgeEnds),
            stSet_size(bridgeNode->connectedCactusNodes));
    stSetIterator *nodeIt = stSet_getIterator(bridgeNode->cactusNodes);
    stCactusNode *cactusNode;
    while((cactusNode = stSet_getNext(nodeIt)) != NULL) {
        fprintf(fileHandle, "Node: %" PRIi64 " in connected nodes: %i, connected to:\n",
                (int64_t)cactusNode, stSet_search(bridgeNode->connectedCactusNodes, cactusNode) != NULL);
        stCactusNodeEdgeEndIt edgeEndIt = stCactusNode_getEdgeEndIt(cactusNode);
        stCactusEdgeEnd *edgeEnd;
        while((edgeEnd = stCactusNodeEdgeEndIt_getNext(&edgeEndIt)) != NULL) {
            assert(stCactusEdgeEnd_getNode(edgeEnd) == cactusNode);
            fprintf(fileHandle, "\tEdge: %" PRIi64 "\tis_bridge: %i\n",
                    (int64_t)stCactusEdgeEnd_getNode(stCactusEdgeEnd_getOtherEdgeEnd(edgeEnd)),
                    stCactusEdgeEnd_getLink(edgeEnd) == NULL);
        }
    }
    stSet_destructIterator(nodeIt);
}

stBridgeGraph *stBridgeGraph_getBridgeGraph(stCactusNode *cactusNode) {
	/*
	 * Build the bridge graph for the connnected component containing cactusNode.
	 */

    // Create the bridge graph container
    stBridgeGraph *bridgeGraph = st_malloc(sizeof(stBridgeGraph));
    bridgeGraph->bridgeNodes = stList_construct3(0, (void (*)(void *))stBridgeNode_destruct);

    // Get cactus nodes
    stSet *component = stSet_construct();
    buildComponent(component, cactusNode, 0);

    stSet *seen = stSet_construct(); // A record of which nodes are in a bridge node;

    stHash *bridgeEndsToBridgeNodes = stHash_construct();

    // Make bridge nodes in component

    // For each cactus node
    stSetIterator *componentIt = stSet_getIterator(component);
    stCactusNode *cactusNode2;
    while((cactusNode2 = stSet_getNext(componentIt)) != NULL) {

        // If not already in a bridge graph node
        if(stSet_search(seen, cactusNode2) == NULL) {

            // Make a bridge node
            stBridgeNode *bridgeNode = stBridgeNode_construct();

            // Add to bridge nodes
            stList_append(bridgeGraph->bridgeNodes, bridgeNode);

            // Get nodes in the bridge component
            buildComponent(bridgeNode->cactusNodes, cactusNode2, 1);

            // Add nodes in bridge component to seen
            stSet_insertAll(seen, bridgeNode->cactusNodes);
            assert(stSet_search(bridgeNode->cactusNodes, cactusNode2) == cactusNode2);

            // Iterate to find bridge edges
            getBridges(bridgeNode);

            // For each bridge end recursively search to find connected nodes
            // and add the bridge end to the bridgeEndsToBridgeNodes map
            stSetIterator *bridgeEndIt = stSet_getIterator(bridgeNode->bridgeEnds);
            stCactusEdgeEnd *bridgeEnd;
            while((bridgeEnd = stSet_getNext(bridgeEndIt)) != NULL) {

            	// Call adds the set of connected nodes to bridgeNode->connectedNodes reachable
            	// from connected nodes
            	constructConnectedNodes(bridgeNode, bridgeEnd);

            	// Map bridgeEnd to the bridgeNode
                stHash_insert(bridgeEndsToBridgeNodes, bridgeEnd, bridgeNode);
            }
            stSet_destructIterator(bridgeEndIt);
        }
    }
    assert(stSet_size(component) == stSet_size(seen));
    stSet_destructIterator(componentIt);

    // Link the bridge nodes together

    // For each bridge node
    for(int64_t i=0; i<stList_length(bridgeGraph->bridgeNodes); i++) {
        stBridgeNode *bridgeNode = stList_get(bridgeGraph->bridgeNodes, i);

        // For each bridge end in the node
        stSetIterator *bridgeEndIt = stSet_getIterator(bridgeNode->bridgeEnds);
        stCactusEdgeEnd *bridgeEnd;
        while((bridgeEnd = stSet_getNext(bridgeEndIt)) != NULL) {

            // Find the connected bridge node
            stBridgeNode *bridgeNode2 = stHash_search(bridgeEndsToBridgeNodes,
                    stCactusEdgeEnd_getOtherEdgeEnd(bridgeEnd));
            assert(bridgeNode2 != NULL);

            // Attach to the bridge node
            stList_append(bridgeNode->connectedNodes, bridgeNode2);
        }
        stSet_destructIterator(bridgeEndIt);
    }

    // Cleanup
    stSet_destruct(seen);
    stSet_destruct(component);
    stHash_destruct(bridgeEndsToBridgeNodes);

    return bridgeGraph;
}

void stBridgeGraph_destruct(stBridgeGraph *bridgeGraph) {
    stList_destruct(bridgeGraph->bridgeNodes);
    free(bridgeGraph);
}

stHash *stBridgeGraph_getBridgeEdgeEndsToBridgeNodesHash(stBridgeGraph *bridgeGraph) {
	/*
	 * Returns a hash of the bridge edge ends in the bridge graph to their incident nodes in the graph.
	 */
	stHash *bridgeEndsToBridgeNodes = stHash_construct();

	for(int64_t i=0; i<stList_length(bridgeGraph->bridgeNodes); i++) {
		stBridgeNode *bridgeNode = stList_get(bridgeGraph->bridgeNodes, i);

		stCactusEdgeEnd *edgeEnd;
		stSetIterator *endIt = stSet_getIterator(bridgeNode->bridgeEnds);
		while((edgeEnd = stSet_getNext(endIt)) != NULL) {
			assert(stHash_search(bridgeEndsToBridgeNodes, edgeEnd) == NULL);
			stHash_insert(bridgeEndsToBridgeNodes, edgeEnd, bridgeNode);
		}
		stSet_destructIterator(endIt);
	}

	return bridgeEndsToBridgeNodes;
}

/*
 * Functions to build the snarl decomposition
 */

stSnarl *stSnarl_constructEmptySnarl(stCactusEdgeEnd *edgeEnd1, stCactusEdgeEnd *edgeEnd2) {
	stSnarl *snarl = st_calloc(1, sizeof(stSnarl));

	snarl->edgeEnd1 = edgeEnd1;
	snarl->edgeEnd2 = edgeEnd2;
	snarl->chains = stList_construct3(0, (void (*)(void *))stList_destruct);
	snarl->unarySnarls = stList_construct3(0, (void (*)(void *))stSnarl_destruct);
	snarl->parentSnarls = stList_construct();

	return snarl;
}

void stSnarl_destruct(stSnarl *snarl) {
	stList_destruct(snarl->chains);
	stList_destruct(snarl->unarySnarls);
	stList_destruct(snarl->parentSnarls);
	free(snarl);
}

uint64_t stSnarl_hashKey(const void *snarl) {
	/*
	 * Hash key for snarls that hashes the two boundary edge end references.
	 */
	stSnarl *s = (stSnarl *)snarl;
	return stHash_pointer(s->edgeEnd1) + stHash_pointer(s->edgeEnd2);
}

int stSnarl_equals(const void *snarl1, const void *snarl2) {
	/*
	 * Returns zero iff the two snarls have the same boundaries in the same order.
	 */
	stSnarl *s1 = (stSnarl *)snarl1;
	stSnarl *s2 = (stSnarl *)snarl2;
	return s1->edgeEnd1 == s2->edgeEnd1 && s1->edgeEnd2 == s2->edgeEnd2;
}

static stSet *getEmptySnarlCache() {
	return stSet_construct3(stSnarl_hashKey, stSnarl_equals, NULL);
}

static stSnarl *getSnarlFromCache(stSet *snarlCache,
		stCactusEdgeEnd *edgeEnd1, stCactusEdgeEnd *edgeEnd2) {
	/*
	 * Gets a snarl from the cache, NULL if not present;
	 */
	stSnarl snarl;
	snarl.edgeEnd1 = edgeEnd1;
	snarl.edgeEnd2 = edgeEnd2;
	return stSet_search(snarlCache, &snarl);
}

stSnarl *stSnarl_makeRecursiveSnarl(stCactusEdgeEnd *edgeEnd1, stCactusEdgeEnd *edgeEnd2,
									stSet *snarlCache, stSnarl *parentSnarl) {
	/*
	 * Makes a snarl, recursively constructing all its child snarls. If a unary snarl
	 * edgeEnd1 == edgeEnd2.
	 */

	// Must either be a bridge or be opposite ends of a link within a chain
	if(edgeEnd1 != edgeEnd2) { // If both equal is a unary snarl
		assert(edgeEnd1->node == edgeEnd2->node);
		assert(edgeEnd1->link == edgeEnd2);
		assert(edgeEnd2->link == edgeEnd1);
	}

	// Check the cache
	stSnarl *snarl = getSnarlFromCache(snarlCache, edgeEnd1, edgeEnd2);

	if(snarl == NULL) { // If not in the cache
		snarl = stSnarl_constructEmptySnarl(edgeEnd1, edgeEnd2);

		// For each edge end incident with the cactus node
		stCactusEdgeEnd *edgeEnd = edgeEnd1->node->head;
		while(edgeEnd != NULL) {

			// If not edgeEnd1 or edgeEnd2
			if(edgeEnd != edgeEnd1 && edgeEnd != edgeEnd2) {

				if(edgeEnd->link == NULL) { // is a bridge
					assert(edgeEnd->otherEdgeEnd->node != edgeEnd->node); // not a trivial chain
					// Make a new nested unary snarl
					stList_append(snarl->unarySnarls, stSnarl_makeRecursiveSnarl(edgeEnd->otherEdgeEnd,
							edgeEnd->otherEdgeEnd, snarlCache, snarl));
				}
				// else is a chain
				else if(edgeEnd->linkOrientation /* is the positive end of link in chain */ &&
						edgeEnd->link != edgeEnd->otherEdgeEnd /* is not a trivial chain (i.e. a self loop) */) {

					// Make a new nested chain
					stList *chain = stList_construct3(0, (void (*)(void *))stSnarl_destruct);
					stList_append(snarl->chains, chain);

					// Add links to the chain
					stCactusEdgeEnd *chainEnd = edgeEnd->link->otherEdgeEnd;
					assert(chainEnd != edgeEnd);
					do {
						assert(chainEnd->linkOrientation);
						stList_append(chain, stSnarl_makeRecursiveSnarl(chainEnd, chainEnd->link,
								snarlCache, snarl));
						chainEnd = chainEnd->link->otherEdgeEnd;
					} while(chainEnd != edgeEnd);
				}
			}

			// Get next incident edge end
			edgeEnd = edgeEnd->nEdgeEnd;
		}
	}

	// Add parent snarl
	if(parentSnarl != NULL) {
		stList_append(snarl->parentSnarls, parentSnarl);
	}

	return snarl;
}

stList *getBridgePathConnectingEnds(stBridgeGraph *bridgeGraph, stCactusEdgeEnd *startEdgeEnd, stCactusEdgeEnd *endEdgeEnd) {
	/*
	 * Gets a path of bridge edges in the bridge graph connecting the projection of two cactus edge ends, and including these ends.
	 * Returned path is represented as a sequence of pairs of bridge edge ends, each of which is a snarl.
	 */

	// Build hash of bridge ends to bridge nodes
	stHash *bridgeEndsToBridgeNodes = stBridgeGraph_getBridgeEdgeEndsToBridgeNodesHash(bridgeGraph);

	// Get bridge node incident with startEdgeEnd
	stBridgeNode *startBridgeNode = stHash_search(bridgeEndsToBridgeNodes, startEdgeEnd);
	assert(startBridgeNode != NULL);

	// Do DFS from startBridgeNode until we find a path to endBridgeNode
	// We use a stack to prevent stack overflows as recursion can get pretty deep
	stList *stack = stList_construct();

	stList_append(stack, startEdgeEnd);
	stList_append(stack, stSet_getIterator(startBridgeNode->bridgeEnds)); // Iterator over incident bridge ends
	while(1) {
		assert(stList_length(stack) > 0);
		stSetIterator *endIt = stList_get(stack, stList_length(stack)-1);
		stCactusEdgeEnd *edgeEnd = stList_get(stack, stList_length(stack)-2);

		// Get next incident edge end
		stCactusEdgeEnd *edgeEnd2 = stSet_getNext(endIt);

		if(edgeEnd == edgeEnd2) { // Go around again in this case
			continue;
		}

		// Pop the stack if finished
		if(edgeEnd2 == NULL) {
			stList_pop(stack);
			stList_pop(stack);
			stSet_destructIterator(endIt);
			continue;
		}

		// If we're done, convert stack to final path
		if(edgeEnd2 == endEdgeEnd) {
			stList *bridgePath = stList_construct();
			assert(stList_length(stack) % 2 == 0); // Check stack has even length
			for(int64_t i=0; i<stList_length(stack); i+=2) {
				stList_append(bridgePath, stList_get(stack, i));
				stSet_destructIterator(stList_get(stack, i+1));
				stList_append(bridgePath, i+2 < stList_length(stack) ?
						((stCactusEdgeEnd *)stList_get(stack, i+2))->otherEdgeEnd : endEdgeEnd);
			}

			// Cleanup
			stList_destruct(stack);
			stHash_destruct(bridgeEndsToBridgeNodes);

			return bridgePath;
		}

		// Else add to the stack and continue traverse
		stList_append(stack, edgeEnd2->otherEdgeEnd);
		stList_append(stack, stSet_getIterator(((stBridgeNode *)stHash_search(bridgeEndsToBridgeNodes,
				edgeEnd2->otherEdgeEnd))->bridgeEnds));
	}
	return NULL; // This is unreachable
}

bool getNodesOnPathInCactusTreeBetweenBridgeSnarlBoundaries2(stList *nodePath, stSet *seen,
		stCactusEdgeEnd *currentEdgeEnd, stCactusEdgeEnd *endEdgeEnd) {
	/*
	 * Recursive sub-function of getNodesOnPathInCactusTreeBetweenBridgeSnarlBoundaries.
	 */

	// First check if we've visited this node before, we don't need do anything if that's the case
	if(stSet_search(seen, currentEdgeEnd->node) == NULL) {
		stSet_insert(seen, currentEdgeEnd->node);

		// If currentEdgeEnd is in a chain,
		// extend this chain without adding the node incident with currentEdgeEnd
		// to the path, because as we are extending the chain the node incident with currentEdgeEnd
		// is not on the path in the corresponding cactus tree
		if(currentEdgeEnd->link != NULL) {

			// If return value is true then we have found the path and can stop
			if(getNodesOnPathInCactusTreeBetweenBridgeSnarlBoundaries2(nodePath, seen,
					currentEdgeEnd->link->otherEdgeEnd, endEdgeEnd)) {
				return 1;
			}
		}

		// Now add the node incident with currentEdgeEnd to the current path, as any path
		// between the start and end edges in the cactus
		// graph reached in this traversal must include the node incident with currentEdgeEnd
		stList_append(nodePath, currentEdgeEnd->node);

		// For each other chain / bridge incident with the node
		stCactusEdgeEnd *edgeEnd = currentEdgeEnd->node->head;
		do {
			// If we've reached the end of the path we're done
			if(edgeEnd == endEdgeEnd) {
				return 1;
			}

			// Otherwise, if edgeEnd is in a chain and not in the chain
			// containing currentEdgeEnd
			if(edgeEnd->link != NULL && edgeEnd->linkOrientation /* traverse in one direction only */ &&
			   edgeEnd != currentEdgeEnd && edgeEnd != currentEdgeEnd->link) {

				// Recurse
				if(getNodesOnPathInCactusTreeBetweenBridgeSnarlBoundaries2(nodePath, seen, edgeEnd, endEdgeEnd)) {
					return 1;
				}
			}

		} while((edgeEnd = edgeEnd->nEdgeEnd) != NULL);

		// Remove the current node from the path as we haven't found a path to endEdgeEnd
		// including the node
		assert(stList_peek(nodePath) == currentEdgeEnd->node);
		stList_pop(nodePath);
	}

	return 0; // Indicate that we haven't found a complete path yet in the DFS
}

stList *getNodesOnPathInCactusTreeBetweenBridgeSnarlBoundaries(stCactusEdgeEnd *edgeEnd1, stCactusEdgeEnd *edgeEnd2) {
	/*
	 * Gets net nodes in the cactus tree on the path connecting two cactus bridge edge ends,
	 * edgeEnd1 and edgeEnd2, that are both in the same bridge component.
	 * Path includes the nodes in the cactus graph
	 * incident with these ends. Path goes from edgeEnd1 to edgeEnd2.
	 */

	// Check they are both bridges
	assert(edgeEnd1->link == NULL);
	assert(edgeEnd2->link == NULL);
	assert(edgeEnd1->node != edgeEnd1->otherEdgeEnd->node);
	assert(edgeEnd2->node != edgeEnd2->otherEdgeEnd->node);

	// A set to represent the vertices visited by the dfs,
	// used to avoid walking backwards in the cactus tour
	stSet *seen = stSet_construct();

	// A list representing the nodes on the path
	stList *nodePath = stList_construct();

	// Uses a DFS from edgeEnd1 until we have have found a path to edgeEnd2
	bool b = getNodesOnPathInCactusTreeBetweenBridgeSnarlBoundaries2(nodePath, seen, edgeEnd1, edgeEnd2);
	(void)b; // Stop compiler complaining in non-debug mode
	assert(b); // If this is not true then we did not find a path in the cactus tree between the edge ends

	// Cleanup
	stSet_destruct(seen);

	return nodePath;
}

stList *stCactusGraph_getTopLevelSnarlChain(stCactusGraph *cactusGraph,
		stCactusEdgeEnd *startEdgeEnd, stCactusEdgeEnd *endEdgeEnd, stSet *snarlCache) {
	/*
	 * Constructs the set of nested snarls and unary snarls between two edge ends in a cactus graph,
	 * returning the top level chain as a list.
     *
     * The startEdgeEnd and endEdgeEnd are used as the boundaries of the top level chain.
     * They must both be in the same chain and distinct or both be part of bridges in the same component
	 */

	// Get cactus graph component containing startEdgeEnd and endEdgeEnd
	stSet *component = stSet_construct();
	buildComponent(component, startEdgeEnd->node, 0);

	// Check two edge ends are in the same component
	assert(stSet_search(component, startEdgeEnd->node) == startEdgeEnd->node);
	assert(stSet_search(component, endEdgeEnd->node) == endEdgeEnd->node);

	// Make top level chain, which we'll then fill out
	stList *topLevelChain = stList_construct3(0, (void (*)(void *))stSnarl_destruct);

	// If the two edge ends are in a chain
	// then walk over the links in the chain between the two boundaries creating
	// the top level chain

	if(startEdgeEnd->link != NULL) { // Is a chain end

		assert(endEdgeEnd->link != NULL); // Check the other edge end is also in a chain
		assert(startEdgeEnd != endEdgeEnd);  // Check not the same

		stCactusEdgeEnd *end = startEdgeEnd;
		while(1) {
			stList_append(topLevelChain, stSnarl_makeRecursiveSnarl(end, end->link, snarlCache, NULL));

			if(end->link == endEdgeEnd) {
				break; // we're done
			}

			end = end->link->otherEdgeEnd;
			assert(end != startEdgeEnd); // Check we haven't looped
			assert(end != endEdgeEnd); // Check end edge end doesn't have the wrong orientation
			// with respect start edge end
		}
	}

	// Else they are bridges
	else {
		assert(endEdgeEnd->link == NULL); // Check other end is also a bridge
		assert(startEdgeEnd != endEdgeEnd);

		// Get bridge graph
		stBridgeGraph *bridgeGraph = stBridgeGraph_getBridgeGraph(stSet_peek(component));
		assert(stList_length(bridgeGraph->bridgeNodes) > 0);

		// Get edges on the path in the bridge graph connecting the projection of startEdgeEnd and endEdgeEnd, starting
		// with startEdgeEnd and ending with endEdgeEnd
		stList *bridgePath = getBridgePathConnectingEnds(bridgeGraph, startEdgeEnd, endEdgeEnd);

		// For each induced snarl on the path of bridge nodes

		for(int64_t i=0; i+1<stList_length(bridgePath); i+=2) {
			// Get the link in the chain
			stCactusEdgeEnd *leftSnarlEnd = stList_get(bridgePath, i);
			stCactusEdgeEnd *rightSnarlEnd = stList_get(bridgePath, i+2);

			// If we can't find it in the cache build it
			// if it is in the cache we need to nothing further
			if(getSnarlFromCache(snarlCache, leftSnarlEnd, rightSnarlEnd) == NULL) {

				// Build top level snarl and add to top level chain
				stSnarl *snarl = stSnarl_constructEmptySnarl(leftSnarlEnd, rightSnarlEnd);

				// Get nodes on the path in the cactus tree that connect the induced snarl
				stList *nodesOnPathBetweenSnarlBoundariesList = getNodesOnPathInCactusTreeBetweenBridgeSnarlBoundaries(leftSnarlEnd, rightSnarlEnd);
				stSet *nodesOnPathBetweenSnarlBoundaries = stList_getSet(nodesOnPathBetweenSnarlBoundariesList);
				stList_destruct(nodesOnPathBetweenSnarlBoundariesList);

				// Traverse chains incident with these nodes, breaking chains where ever they intersect these nodes

				// For each node on the path in the cactus tree between the induced snarl ends
				stSetIterator *nodeIt = stSet_getIterator(nodesOnPathBetweenSnarlBoundaries);
				stCactusNode *cactusNode;
				while((cactusNode = stSet_getNext(nodeIt)) != NULL) {

					// For each chain or bridge incident with the node
					stCactusEdgeEnd *edgeEnd = cactusNode->head;
					while(edgeEnd != NULL) {
						assert(edgeEnd->node == cactusNode);

						if(edgeEnd->link != NULL) { // Is in a chain
							if(edgeEnd->linkOrientation) { // Is positively oriented (avoid going around it twice)

								// There exists a part of the chain that contains snarls not collapsed by the induced snarl
								if(stSet_search(nodesOnPathBetweenSnarlBoundaries, edgeEnd->link->otherEdgeEnd->node) == NULL) {

									// Create a new nested chain
									stList *nestedChain = stList_construct3(0, (void (*)(void *))stSnarl_destruct);
									stList_append(snarl->chains, nestedChain);

									// Add links to the chain while they involve a node not in the
									// nodesOnPathBetweenSnarlBoundaries

									stCactusEdgeEnd *chainEnd = edgeEnd->link->otherEdgeEnd; // Walk around chain in positive orientation
									do {
										assert(chainEnd->linkOrientation);
										assert(stSet_search(nodesOnPathBetweenSnarlBoundaries, chainEnd) == NULL);

										//  Add a nested snarl to the chain
										stList_append(nestedChain, stSnarl_makeRecursiveSnarl(chainEnd, chainEnd->link, snarlCache, snarl));

										chainEnd = chainEnd->link->otherEdgeEnd;
									} while(stSet_search(nodesOnPathBetweenSnarlBoundaries, chainEnd) == NULL);
								}
							}
						}
						else { // Is a bridge
							assert(edgeEnd->otherEdgeEnd->node != edgeEnd->node); // Is a bridge, not a trivial chain
							stList_append(snarl->unarySnarls, stSnarl_makeRecursiveSnarl(edgeEnd->otherEdgeEnd, edgeEnd->otherEdgeEnd, snarlCache, snarl));
						}

						// Get next edge end incident with the node
						edgeEnd = edgeEnd->nEdgeEnd;
					}
				}

				// Cleanup
				stSet_destruct(nodesOnPathBetweenSnarlBoundaries);
			}
		}

		// Cleanup
		stBridgeGraph_destruct(bridgeGraph);
		stList_destruct(bridgePath);
	}

	// Cleanup
	stSet_destruct(component);


	return topLevelChain;
}

stSnarlDecomposition *stCactusGraph_getSnarlDecomposition(stCactusGraph *cactusGraph, stList *snarlChainEnds) {
	/*
	 * Gets the snarl decomposition for a set paths, each specified by a pair of cactus edge ends which are either
	 * both ends of bridge edges or both in the same chain and oriented so that they form a non-minimal snarl.
	 * Each such pair is specified by a successive pair of ends in snarlChainEnds.
	 */

	// Make snarl decomposition object
	stSnarlDecomposition *snarlDecomposition = st_calloc(1, sizeof(stSnarlDecomposition));
	snarlDecomposition->topLevelChains = stList_construct3(0, (void (*)(void *))stList_destruct);
	snarlDecomposition->topLevelUnarySnarls = stList_construct3(0, (void (*)(void *))stSnarl_destruct);

	// Make empty snarl cache, used to avoid constructing the same snarl twice
	stSet *snarlCache = getEmptySnarlCache();

	// Build snarl chain for each pair of ends
	for(int64_t i=0; i+1<stList_length(snarlChainEnds); i+=2) {
		stCactusEdgeEnd *edgeEnd1 = stList_get(snarlChainEnds, i);
		stCactusEdgeEnd *edgeEnd2 = stList_get(snarlChainEnds, i+1);
		if(edgeEnd1 != edgeEnd2) { // If forms a chain
			stList_append(snarlDecomposition->topLevelChains,
					stCactusGraph_getTopLevelSnarlChain(cactusGraph, edgeEnd1, edgeEnd2, snarlCache));
		}
		else { // If is a unary snarl
			stList_append(snarlDecomposition->topLevelUnarySnarls,
					stSnarl_makeRecursiveSnarl(edgeEnd1, edgeEnd2, snarlCache, NULL));
		}
	}

	// Cleanup
	stSet_destruct(snarlCache);

	return snarlDecomposition;
}

void stSnarlDecomposition_destruct(stSnarlDecomposition *snarls) {
	stList_destruct(snarls->topLevelChains);
	stList_destruct(snarls->topLevelUnarySnarls);
	free(snarls);
}

/*
 * Functions for printing snarl decomposition.
 */

void stSnarl_print2(stSnarl *snarl, FILE *fileHandle, const char *parentPrefix, const char *parentChainPrefix);

void stSnarl_printUnary(stSnarl *snarl, FILE *fileHandle, const char *parentPrefix, const char *parentChainPrefix);

void stSnarl_printChains2(stList *chains, FILE *fileHandle, const char *parentPrefix, const char *parentChainPrefix) {
    for(int64_t i=0; i<stList_length(chains); i++) {
        stList *chain = stList_get(chains, i);
        char *chainPrefix = stString_print("%s-%" PRIi64 "", parentChainPrefix, i);
        fprintf(fileHandle, "%s\tChain: %s\tLength: %" PRIi64 "\n", parentPrefix, chainPrefix, stList_length(chain));
        for(int64_t j=0; j<stList_length(chain); j++) {
            char *prefix = stString_print("%s%s", parentPrefix, "\t\t");
            stSnarl_print2(stList_get(chain, j), fileHandle, prefix, chainPrefix);
            free(prefix);
        }
        free(chainPrefix);
    }
}

void stSnarl_print2(stSnarl *snarl, FILE *fileHandle, const char *parentPrefix, const char *parentChainPrefix) {
    fprintf(fileHandle, "%sSnarl,\tchild-chain-number: %" PRIi64 ", \tunary-snarl-child-number: %" PRIi64 ", \tparent-number: %" PRIi64
    		"\tis_bridge_snarl: %s\tSide1:(%" PRIi64 ",%" PRIi64 ")\tSide2:(%" PRIi64 ",%" PRIi64 ")\n", parentPrefix,
            stList_length(snarl->chains), stList_length(snarl->unarySnarls), stList_length(snarl->parentSnarls),
			stCactusEdgeEnd_getLink(snarl->edgeEnd1) == NULL ? "true" : "false",
			(int64_t)stCactusEdgeEnd_getOtherEdgeEnd(snarl->edgeEnd1), (int64_t)snarl->edgeEnd1,
            (int64_t)snarl->edgeEnd2, (int64_t)stCactusEdgeEnd_getOtherEdgeEnd(snarl->edgeEnd2));
    stSnarl_printChains2(snarl->chains, fileHandle, parentPrefix, parentChainPrefix);
    for(int64_t i=0; i<stList_length(snarl->unarySnarls); i++) {
    	stSnarl_printUnary(stList_get(snarl->unarySnarls, i), fileHandle, parentPrefix, parentChainPrefix);
    }
}

void stSnarl_printUnary(stSnarl *snarl, FILE *fileHandle, const char *parentPrefix, const char *parentChainPrefix) {
	fprintf(fileHandle, "%sUnary Snarl,\tchild-chain-number: %" PRIi64 ", \tunary-snarl-child-number: %" PRIi64 ", \tparent-number: %" PRIi64
			"\tSide:(%" PRIi64 ",%" PRIi64 ")\n", parentPrefix,
			stList_length(snarl->chains), stList_length(snarl->unarySnarls), stList_length(snarl->parentSnarls),
			(int64_t)stCactusEdgeEnd_getOtherEdgeEnd(snarl->edgeEnd1), (int64_t)snarl->edgeEnd1);
	stSnarl_printChains2(snarl->chains, fileHandle, parentPrefix, parentChainPrefix);
	for(int64_t i=0; i<stList_length(snarl->unarySnarls); i++) {
		stSnarl_printUnary(stList_get(snarl->unarySnarls, i), fileHandle, parentPrefix, parentChainPrefix);
	}
}

void stSnarl_print(stSnarl *snarl, FILE *fileHandle) {
    stSnarl_print2(snarl, fileHandle, "", "C");
}

void stSnarlDecomposition_print(stSnarlDecomposition *snarls, FILE *fileHandle) {
    fprintf(fileHandle, "Top level chains, total: %" PRIi64 "\n", stList_length(snarls->topLevelChains));
    stSnarl_printChains2(snarls->topLevelChains, fileHandle, "", "C");
    for(int64_t i=0; i<stList_length(snarls->topLevelUnarySnarls); i++) {
    	stSnarl_printUnary(stList_get(snarls->topLevelUnarySnarls, i), fileHandle, "", "C");
    }
}
