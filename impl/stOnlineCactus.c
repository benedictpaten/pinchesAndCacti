#include <stdlib.h>

#include "sonLib.h"
#include "stOnlineCactus.h"

typedef enum {
    NODE,
    CHAIN,
    BRIDGE
} nodeType;

struct stCactusTree {
    nodeType type;
    struct stCactusTree *parent;
    struct stCactusTree *prev;
    struct stCactusTree *next;
    struct stCactusTree *firstChild;
};

typedef struct stCactusTree stCactusTree;

struct _stOnlineCactus {
    stCactusTree *tree;
    stHash *endToNode;
    stHash *edgeToBridgeOrCycle;
    void *(*edgeToEnd)(void *, bool);
    void *(*endToEdge)(void *);
    stConnectivity *adjacencyComponents;
};

nodeType stCactusTree_type(const stCactusTree *tree) {
    return tree->type;
}

stCactusTree *stCactusTree_construct(stCactusTree *parent, stCactusTree *leftSib, nodeType type) {
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
void stOnlineCactus_createEdge(stOnlineCactus *cactus, void *edge) {
    void *endL = cactus->edgeToEnd(edge, 0);
    void *endR = cactus->edgeToEnd(edge, 1);
    stConnectedComponent *component = stConnectivity_getConnectedComponent(cactus->connectivity, endL);
    assert(component == stConnectivity_getConnectedComponent(cactus->connectivity, endR));
    // TODO: this is probably a bad bottleneck--ideally there should be
    // some straightforward mapping between adjacency components and
    // nodes, but that can cause huge issues when the adjacency
    // components are modified after the operation and the "old"
    // information is needed.
    stCactusTree *node = NULL;
    stConnectedComponent *nodeIt = stConnectedComponent_getNodeIterator(component);
}

/* // callback for splitting an edge "horizontally" */
/* void stOnlineCactus_splitEdgeHorizontally(stOnlineCactus *cactus, void *oldEdge, */
/*                                           void *newEdgeL, void *newEdgeR) { */
/*     // Find the corresponding node in the 3ect. */
/*     stCactusTree *edgeNode = stHash_search(cactus->edgeToNode, oldEdge); */
/*     assert(edgeNode != NULL); */
/*     assert(stCactusTree_type(edgeNode) == EDGE); */

/*     stCactusTree *leftSib = edgeNode->prev; */
/*     stCactusTree *parent = edgeNode->parent; */
/*     stCactusTree_destruct(edgeNode); */
/*     stHash_remove(cactus->edgeToNode, oldEdge); */
/*     stCactusTree *leftEdge = stCactusTree_construct(parent, leftSib, EDGE); */
/*     stCactusTree *rightEdge = stCactusTree_construct(parent, leftEdge, EDGE); */
/*     stHash_insert(cactus->edgeToNode, newEdgeL, leftEdge); */
/*     stHash_insert(cactus->edgeToNode, newEdgeR, rightEdge); */
/* } */

/* // callback for merging two adjacent edges */
/* void stOnlineCactus_mergeAdjacentEdges(stOnlineCactus *cactus, void *oldEdgeL, */
/*                                        void *oldEdgeR, void *newEdge) { */
/*     stCactusTree *oldEdgeLNode = stHash_search(cactus->edgeToNode, oldEdgeL); */
/*     assert(oldEdgeLNode != NULL); */
/*     assert(stCactusTree_type(oldEdgeLNode) == EDGE); */
/*     stCactusTree *oldEdgeRNode = stHash_search(cactus->edgeToNode, oldEdgeR); */
/*     assert(oldEdgeRNode != NULL); */
/*     assert(stCactusTree_type(oldEdgeRNode) == EDGE); */
/*     assert(oldEdgeLNode->next == oldEdgeRNode); */
/*     assert(oldEdgeRNode->prev == oldEdgeLNode); */
/*     stCactusTree *leftSib = oldEdgeLNode->prev; */
/*     stCactusTree *parent = oldEdgeLNode->parent; */
/*     assert(parent == oldEdgeRNode->parent); */
/*     stCactusTree_destruct(oldEdgeLNode); */
/*     stCactusTree_destruct(oldEdgeRNode); */
/*     stHash_remove(cactus->edgeToNode, oldEdgeL); */
/*     stHash_remove(cactus->edgeToNode, oldEdgeR); */

/*     stCactusTree *newEdgeNode = stCactusTree_construct(parent, leftSib, EDGE); */
/*     stHash_insert(cactus->edgeToNode, newEdge, newEdgeNode); */
/* } */

/* static void breakParentChain(stCactusTree *node) { */
/*     stCactusTree *parent = node->parent; */
/*     stCactusTree *grandparent = node->parent->parent; */
/*     assert(stCactusTree_type(node) == EDGE); */
/*     assert(stCactusTree_type(parent) == CHAIN); */
/*     assert(stCactusTree_type(grandparent) == NET); */
/*     // Create the "previous" chain. */
/*     if (node->prev != NULL) { */
/*         stCactusTree *prev = node->prev; */
/*         node->prev = NULL; */
/*         prev->next = NULL; */
/*         stCactusTree *prevChain = stCactusTree_construct(grandparent, NULL, CHAIN); */
/*         prevChain->firstChild = parent->firstChild; */
/*         parent->firstChild = node; */
/*     } else { */
/*         assert(node == node->parent->firstChild); */
/*     } */
/*     if (node->next != NULL) { */
/*         stCactusTree *next = node->next; */
/*         node->next = NULL; */
/*         next->prev = NULL; */
/*         stCactusTree *nextChain = stCactusTree_construct(grandparent, NULL, CHAIN); */
/*         nextChain->firstChild = next; */
/*     } */
/* } */

/* // callback for merging two edges */
/* void stOnlineCactus_alignEdges(stOnlineCactus *cactus, void *edge1, void *edge2, */
/*                                void *newEdge) { */
/*     stCactusTree *node1 = stHash_search(cactus->edgeToNode, edge1); */
/*     assert(node1 != NULL); */
/*     assert(stCactusTree_type(node1) == EDGE); */
/*     stCactusTree *node2 = stHash_search(cactus->edgeToNode, edge2); */
/*     assert(node2 != NULL); */
/*     assert(stCactusTree_type(node2) == EDGE); */

/*     // break the chain(s) or bridge(s) containing edge1 and edge2 into */
/*     // 3 parts, "before", "after", and "middle". */
/*     breakParentChain(node1); */
/*     breakParentChain(node2); */

/*     // find the most recent common ancestral net of the two edges. */
/*     stList *ancestors1 = stList_construct(); */
/*     stList *ancestors2 = stList_construct(); */
/*     stSet *ancestorSet1 = stSet_construct(); */
/*     stCactusTree *parent = node1->parent; */
/*     while (parent != NULL) { */
/*         stList_append(ancestors1, parent); */
/*         stSet_insert(ancestorSet1, parent); */
/*         parent = parent->parent; */
/*     } */
/*     parent = node2->parent; */
/*     while (stSet_search(ancestorSet1, parent) != NULL) { */
/*         stList_append(ancestors2, parent); */
/*         parent = parent->parent; */
/*     } */
/*     while (stCactusTree_type(parent) != NET) { */
/*         parent = parent->parent; */
/*     } */
/*     // just for clarification of what the variable actually is now. */
/*     stCactusTree *mrcaNet = parent; */

/*     for (int64_t i = 0; i < stList_length(ancestors1); i++) { */
/*         stCactusTree *ancestor = stList_get(ancestors1, i); */
/*         if (stCactusTree_type(ancestor) != NET) { */
/*             continue; */
/*         } */
/*         if (ancestor == mrcaNet) { */
/*             break; */
/*         } */
/*         if (stCactusTree_type(ancestor->parent) != BRIDGE) { */
/*             stCactusTree *grandparent = ancestor->parent->parent; */
/*             stCactusTree_mergeNets(ancestor, grandparent); */
/*         } */
/*     } */

/*     for (int64_t i = 0; i < stList_length(ancestors2); i++) { */
/*         stCactusTree *ancestor = stList_get(ancestors2, i); */
/*         if (stCactusTree_type(ancestor) != NET) { */
/*             continue; */
/*         } */
/*         if (ancestor == mrcaNet) { */
/*             break; */
/*         } */
/*         if (stCactusTree_type(ancestor->parent) != BRIDGE) { */
/*             stCactusTree *grandparent = ancestor->parent->parent; */
/*             stCactusTree_mergeNets(ancestor, grandparent); */
/*         } */
/*     } */

/*     // TODO: Now only bridges and their parents are left. Merge their */
/*     // children into a chain. */

/*     if (node1->prev == NULL && node1->next == NULL) { */
/*         stCactusTree_destruct(node1->parent); */
/*     } else { */
/*         stCactusTree_destruct(node1); */
/*     } */
/*     if (node2->prev == NULL && node2->next == NULL) { */
/*         stCactusTree_destruct(node2->parent); */
/*     } else { */
/*         stCactusTree_destruct(node2); */
/*     } */
/*     stHash_remove(cactus->edgeToNode, edge1); */
/*     stHash_remove(cactus->edgeToNode, edge2); */

/*     stCactusTree *newChain = stCactusTree_construct(mrcaNet, NULL, CHAIN); */
/*     stCactusTree *newNode = stCactusTree_construct(newChain, NULL, EDGE); */
/*     stHash_insert(cactus->edgeToNode, newEdge, newNode); */
/* } */

/* // callback for splitting an edge "vertically" */
/* void stOnlineCactus_unalignEdge(stOnlineCactus *cactus, void *oldEdge, */
/*                                 void *edge1, void *edge2) { */
    
/* } */

/* void stOnlineCactus_printR(const stCactusTree *tree, stHash *nodeToEdge) { */
/*     if (tree->firstChild) { */
/*         printf("("); */
/*         stCactusTree *child = tree->firstChild; */
/*         while (child != NULL) { */
/*             stOnlineCactus_printR(child, nodeToEdge); */
/*             child = child->next; */
/*             if (child != NULL) { */
/*                 printf(","); */
/*             } */
/*         } */
/*         printf(")"); */
/*     } */
/*     switch (stCactusTree_type(tree)) { */
/*     case NODE: */
/*         printf("NODE%p", (void *) tree); */
/*         break; */
/*     case CHAIN: */
/*         printf("CHAIN%p", (void *) tree); */
/*         break; */
/*     case BRIDGE: */
/*         printf("BRIDGE%p", (void *) tree); */
/*         break; */
/*     } */
/* } */

/* void stOnlineCactus_print(const stOnlineCactus *cactus) { */
/*     stHash *nodeToEdge = stHash_invert(cactus->edgeToNode, stHash_pointer, stHash_getEqualityFunction(cactus->edgeToNode), NULL, NULL); */
/*     stOnlineCactus_printR(cactus->tree, nodeToEdge); */
/*     printf(";\n"); */
/*     stHash_destruct(nodeToEdge); */
/* } */
