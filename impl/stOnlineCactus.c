#include <stdlib.h>

#include "sonLib.h"
#include "stOnlineCactus.h"

struct _stCactusTree {
    cactusNodeType type;
    stCactusTree *parent;
    stCactusTree *prev;
    stCactusTree *next;
    stCactusTree *firstChild;
};

struct _stOnlineCactus {
    stCactusTree *tree;
    stHash *endToNode;
    stHash *edgeToBridgeOrCycle;
    void *(*edgeToEnd)(void *, bool);
    void *(*endToEdge)(void *);
    stConnectivity *adjacencyComponents;
};

struct _stCactusTreeIt {
    stCactusTree *cur;
};

cactusNodeType stCactusTree_type(const stCactusTree *tree) {
    return tree->type;
}

stCactusTree *stCactusTree_construct(stCactusTree *parent, stCactusTree *leftSib, cactusNodeType type) {
    stCactusTree *ret = calloc(1, sizeof(stCactusTree));
    ret->type = type;
    if (parent != NULL) {
        if (type == CHAIN || type == BRIDGE) {
            assert(stCactusTree_type(parent) == NODE);
        }
        ret->parent = parent;
        if (leftSib == NULL) {
            ret->next = parent->firstChild;
            parent->firstChild = ret;
            if (ret->next != NULL) {
                ret->next->prev = ret;
            }
        } else {
            assert(leftSib->parent == parent);
            ret->prev = leftSib;
            ret->next = leftSib->next;
            if (ret->next != NULL) {
                ret->next->prev = ret;
            }
            leftSib->next = ret;
        }
    } else {
        assert(type == NODE);
    }
    return ret;
}

// Recursively destruct a 3-edge-connected component tree.
void stCactusTree_destruct(stCactusTree *tree) {
    stCactusTree *child = tree->firstChild;
    while (child != NULL) {
        stCactusTree *next = child->next;
        stCactusTree_destruct(child);
        child = next;
    }
    if (tree->prev != NULL) {
        tree->prev->next = tree->next;
    } else if (tree->parent != NULL) {
        tree->parent->firstChild = tree->next;
    }
    if (tree->next != NULL) {
        tree->next->prev = tree->prev;
    }
    free(tree);
}

// Moves a 3ect node "child" to the end of the child list for "tree".
static void stCactusTree_appendChild(stCactusTree *tree, stCactusTree *child) {
    child->parent = tree;
    stCactusTree *cur = tree->firstChild;
    if (cur == NULL) {
        tree->firstChild = child;
        child->next = NULL;
        child->prev = NULL;
        return;
    }
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = child;
    child->prev = cur;
    child->next = NULL;
}

// Merge two net nodes together. Assumes that dest is the ancestor of
// src.
void stCactusTree_mergeNets(stCactusTree *src, stCactusTree *dest) {
    assert(stCactusTree_type(src->parent) != BRIDGE);
    stCactusTree *child = src->firstChild;
    while (child != NULL) {
        stCactusTree_appendChild(dest, child);
        child = child->next;
    }
    stCactusTree_destruct(src);
}

stCactusTreeIt *stCactusTree_getIt(stCactusTree *tree) {
    stCactusTreeIt *ret = malloc(sizeof(stCactusTreeIt));
    ret->cur = tree->firstChild;
    return ret;
}

stCactusTree *stCactusTreeIt_getNext(stCactusTreeIt *it) {
    stCactusTree *ret = it->cur;
    if (it->cur != NULL) {
        it->cur = it->cur->next;
    }
    return ret;
}

void stCactusTreeIt_destruct(stCactusTreeIt *it) {
    free(it);
}

// Construct a new, empty, online cactus graph.
stOnlineCactus *stOnlineCactus_construct(stConnectivity *connectivity,
                                         void *(*edgeToEnd)(void *, bool),
                                         void *(*endToEdge)(void *)) {
    stOnlineCactus *ret = calloc(1, sizeof(stOnlineCactus));
    ret->edgeToEnd = edgeToEnd;
    ret->endToEdge = endToEdge;
    ret->adjacencyComponents = connectivity;
    ret->tree = stCactusTree_construct(NULL, NULL, NODE);
    ret->endToNode = stHash_construct();
    // Add in all the existing ends.
    stConnectedComponentIterator *componentIt = stConnectivity_getConnectedComponentIterator(connectivity);
    size_t numComponents = 0;
    stConnectedComponent *component;
    while ((component = stConnectedComponentIterator_getNext(componentIt)) != NULL) {
        stConnectedComponentNodeIterator *nodeIt = stConnectedComponent_getNodeIterator(component);
        void *node;
        while ((node = stConnectedComponentNodeIterator_getNext(nodeIt)) != NULL) {
            stHash_insert(ret->endToNode, node, ret->tree);
        }
        stConnectedComponentNodeIterator_destruct(nodeIt);
        numComponents++;
    }
    assert(numComponents == 1);
    (void) numComponents;
    stConnectedComponentIterator_destruct(componentIt);
    ret->edgeToBridgeOrCycle = stHash_construct();
    return ret;
}

stCactusTree *stOnlineCactus_getCactusTree(stOnlineCactus *cactus) {
    return cactus->tree;
}

// Get the two nodes associated with an edge in the graph (which may
// involve multiple, or no, edges in the tree).
void stOnlineCactus_getNodes(stOnlineCactus *cactus, void *edge,
                                    stCactusTree **node1, stCactusTree **node2) {
    void *end1 = cactus->edgeToEnd(edge, 0);
    void *end2 = cactus->edgeToEnd(edge, 1);
    *node1 = stHash_search(cactus->endToNode, end1);
    assert(*node1 != NULL);
    assert(stCactusTree_type(*node1) == NODE);
    *node2 = stHash_search(cactus->endToNode, end2);
    assert(*node2 != NULL);
    assert(stCactusTree_type(*node2) == NODE);
}

// Gets the bridge or cycle node that contains this edge.
static stCactusTree *stOnlineCactus_getBridgeOrCycle(stOnlineCactus *cactus,
                                                     void *edge) {
    stCactusTree *ret = stHash_search(cactus->edgeToBridgeOrCycle, edge);
    assert(ret != NULL);
    assert(stCactusTree_type(ret) == CHAIN || stCactusTree_type(ret) == BRIDGE);
    return ret;
}

// callback for splitting an edge
void stOnlineCactus_splitEdgeHorizontally(stOnlineCactus *cactus, void *oldEdge,
                                          void *oldEndL, void *oldEndR,
                                          void *newEdgeL, void *newEdgeR) {
    stCactusTree *oldNodeL = stHash_remove(cactus->endToNode, oldEndL);
    assert(oldNodeL != NULL);
    assert(stCactusTree_type(oldNodeL) == NODE);
    stCactusTree *oldNodeR = stHash_remove(cactus->endToNode, oldEndR);
    assert(oldNodeR != NULL);
    assert(stCactusTree_type(oldNodeR) == NODE);

    stCactusTree *chain = stOnlineCactus_getBridgeOrCycle(cactus, oldEdge);
    stCactusTree *new;
    if (chain == oldNodeL->parent && chain == oldNodeR->parent) {
        // Simple case: this edge is wholly contained within the
        // chain.
        assert(oldNodeR->prev == oldNodeL);
        assert(oldNodeL->next == oldNodeR);
        new = stCactusTree_construct(oldNodeL, chain, NODE);
    } else if (chain == oldNodeL->parent || chain == oldNodeR->parent) {
        // Attach the new node as the first node in the chain.
        new = stCactusTree_construct(NULL, chain, NODE);
        stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeL, 0), oldNodeL);
        stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeL, 1), new);
        stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeR, 0), new);
        stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeR, 1), oldNodeR);
    } else {
        assert(chain->parent == oldNodeL && oldNodeL == oldNodeR);
        new = stCactusTree_construct(NULL, chain, NODE);
    }
    stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeL, 0), oldNodeL);
    stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeL, 1), new);
    stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeR, 0), new);
    stHash_insert(cactus->endToNode, cactus->edgeToEnd(newEdgeR, 1), oldNodeR);

    stHash_remove(cactus->edgeToBridgeOrCycle, oldEdge);
    stHash_insert(cactus->edgeToBridgeOrCycle, newEdgeL, chain);
    stHash_insert(cactus->edgeToBridgeOrCycle, newEdgeR, chain);
}

// callback for an edge merge

// callback for an adj component merge

// callback for an adj component split

// callback for a partition of an edge

// callback for edge creation (fully inside an existing adj component)
// It's assumed that the adjacency components have already been updated.
void stOnlineCactus_createEdge(stOnlineCactus *cactus, void *edge) {
    void *endL = cactus->edgeToEnd(edge, 0);
    void *endR = cactus->edgeToEnd(edge, 1);
    stConnectedComponent *component = stConnectivity_getConnectedComponent(cactus->adjacencyComponents, endL);
    assert(component == stConnectivity_getConnectedComponent(cactus->adjacencyComponents, endR));
    // TODO: this is probably a bad bottleneck--ideally there should be
    // some straightforward mapping between adjacency components and
    // nodes, but that can cause huge issues when the adjacency
    // components are modified after the operation and the "old"
    // information is needed.
    stCactusTree *nodeForComponent = NULL;
    stConnectedComponentNodeIterator *nodeIt = stConnectedComponent_getNodeIterator(component);
    void *node;
    while ((node = stConnectedComponentNodeIterator_getNext(nodeIt)) != NULL) {
        if ((nodeForComponent = stHash_search(cactus->endToNode, node)) != NULL) {
            break;
        }
    }
    assert(nodeForComponent != NULL);

    stHash_insert(cactus->endToNode, endL, nodeForComponent);
    stHash_insert(cactus->endToNode, endR, nodeForComponent);

    // Create a new chain hanging off the adjacency component that
    // will represent this edge.
    stCactusTree *chain = stCactusTree_construct(nodeForComponent, NULL, CHAIN);
    stHash_insert(cactus->edgeToBridgeOrCycle, edge, chain);
}

void stOnlineCactus_printR(const stCactusTree *tree, stHash *nodeToEnd) {
    if (tree->firstChild) {
        printf("(");
        stCactusTree *child = tree->firstChild;
        while (child != NULL) {
            stOnlineCactus_printR(child, nodeToEnd);
            child = child->next;
            if (child != NULL) {
                printf(",");
            }
        }
        printf(")");
    }
    switch (stCactusTree_type(tree)) {
    case NODE:
        printf("NODE%p", (void *) tree);
        break;
    case CHAIN:
        printf("CHAIN%p", (void *) tree);
        break;
    case BRIDGE:
        printf("BRIDGE%p", (void *) tree);
        break;
    }
}

void stOnlineCactus_print(const stOnlineCactus *cactus) {
    stHash *nodeToEnd = stHash_invert(cactus->endToNode, stHash_pointer, stHash_getEqualityFunction(cactus->endToNode), NULL, NULL);
    stOnlineCactus_printR(cactus->tree, nodeToEnd);
    printf(";\n");
    stHash_destruct(nodeToEnd);
}
