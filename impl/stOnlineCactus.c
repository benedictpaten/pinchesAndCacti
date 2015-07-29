#include <stdlib.h>

#include "sonLib.h"
#include "stOnlineCactus.h"

typedef enum {
    NET,
    CHAIN,
    BRIDGE,
    EDGE
} nodeType;

struct st3ECT {
    nodeType type;
    struct st3ECT *parent;
    struct st3ECT *prev;
    struct st3ECT *next;
    struct st3ECT *firstChild;
};

typedef struct st3ECT st3ECT;

struct _stOnlineCactus {
    st3ECT *tree;
    stHash *edgeToNode;
    const stConnectivity *adjacencyComponents;
};

nodeType st3ECT_type(const st3ECT *tree) {
    return tree->type;
}

st3ECT *st3ECT_construct(st3ECT *parent, st3ECT *leftSib, nodeType type) {
    st3ECT *ret = calloc(1, sizeof(st3ECT));
    ret->type = type;
    if (parent != NULL) {
        if (type == CHAIN || type == BRIDGE) {
            assert(st3ECT_type(parent) == NET);
        } else if (type == EDGE) {
            assert(st3ECT_type(parent) == CHAIN || st3ECT_type(parent) == BRIDGE);
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
        assert(type == NET);
    }
    return ret;
}

// Recursively destruct a 3-edge-connected component tree.
void st3ECT_destruct(st3ECT *tree) {
    st3ECT *child = tree->firstChild;
    while (child != NULL) {
        st3ECT *next = child->next;
        st3ECT_destruct(child);
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

// Construct a new, empty, online cactus graph.
stOnlineCactus *stOnlineCactus_construct(const stConnectivity *connectivity,
                                         stSet *threadBlocks) {
    stOnlineCactus *ret = calloc(1, sizeof(stOnlineCactus));
    // Initialize an "empty" online cactus: one net node with one
    // chain for every thread (containing that thread's block).
    ret->edgeToNode = stHash_construct();
    st3ECT *root = st3ECT_construct(NULL, NULL, NET);
    stSetIterator *it = stSet_getIterator(threadBlocks);
    void *block;
    st3ECT *prevChain = NULL;
    printf("set size %" PRIi64 "\n", stSet_size(threadBlocks));
    while ((block = stSet_getNext(it)) != NULL) {
        printf("adding block %p\n", block);
        st3ECT *chain = st3ECT_construct(root, prevChain, CHAIN);
        st3ECT *edge = st3ECT_construct(chain, NULL, EDGE);
        stHash_insert(ret->edgeToNode, block, edge);
        prevChain = chain;
    }
    ret->tree = root;
    ret->adjacencyComponents = connectivity;
    return ret;
}

// callback for splitting an edge "horizontally"
void stOnlineCactus_splitEdgeHorizontally(stOnlineCactus *cactus, void *oldEdge,
                                          void *newEdgeL, void *newEdgeR) {
    // Find the corresponding node in the 3ect.
    st3ECT *edgeNode = stHash_search(cactus->edgeToNode, oldEdge);
    assert(edgeNode != NULL);
    assert(st3ECT_type(edgeNode) == EDGE);

    st3ECT *leftSib = edgeNode->prev;
    st3ECT *parent = edgeNode->parent;
    st3ECT_destruct(edgeNode);
    stHash_remove(cactus->edgeToNode, oldEdge);
    st3ECT *leftEdge = st3ECT_construct(parent, leftSib, EDGE);
    st3ECT *rightEdge = st3ECT_construct(parent, leftEdge, EDGE);
    stHash_insert(cactus->edgeToNode, newEdgeL, leftEdge);
    stHash_insert(cactus->edgeToNode, newEdgeR, rightEdge);
}

// callback for merging two adjacent edges
void stOnlineCactus_mergeAdjacentEdges(stOnlineCactus *cactus, void *oldEdgeL,
                                       void *oldEdgeR, void *newEdge) {
    st3ECT *oldEdgeLNode = stHash_search(cactus->edgeToNode, oldEdgeL);
    assert(oldEdgeLNode != NULL);
    assert(st3ECT_type(oldEdgeLNode) == EDGE);
    st3ECT *oldEdgeRNode = stHash_search(cactus->edgeToNode, oldEdgeR);
    assert(oldEdgeRNode != NULL);
    assert(st3ECT_type(oldEdgeRNode) == EDGE);
    assert(oldEdgeLNode->next == oldEdgeRNode);
    assert(oldEdgeRNode->prev == oldEdgeLNode);
    st3ECT *leftSib = oldEdgeLNode->prev;
    st3ECT *parent = oldEdgeLNode->parent;
    assert(parent == oldEdgeRNode->parent);
    st3ECT_destruct(oldEdgeLNode);
    st3ECT_destruct(oldEdgeRNode);
    stHash_remove(cactus->edgeToNode, oldEdgeL);
    stHash_remove(cactus->edgeToNode, oldEdgeR);

    st3ECT *newEdgeNode = st3ECT_construct(parent, leftSib, EDGE);
    stHash_insert(cactus->edgeToNode, newEdge, newEdgeNode);
}

// callback for merging two edges
void stOnlineCactus_alignEdges(stOnlineCactus *cactus, void *edge1, void *edge2,
                               void *newEdge) {
    st3ECT *node1 = stHash_search(cactus->edgeToNode, edge1);
    assert(node1 != NULL);
    assert(st3ECT_type(node1) == EDGE);
    st3ECT *node2 = stHash_search(cactus->edgeToNode, edge2);
    assert(node2 != NULL);
    assert(st3ECT_type(node2) == EDGE);

    // find the most recent common ancestral net of the two edges.
    stList *ancestors1 = stList_construct();
    stList *ancestors2 = stList_construct();
    stSet *ancestorSet1 = stSet_construct();
    st3ECT *parent = node1->parent;
    while (parent != NULL) {
        stList_append(ancestors1, parent);
        stSet_insert(ancestorSet1, parent);
        parent = parent->parent;
    }
    parent = node2->parent;
    while (stSet_search(ancestorSet1, parent) != NULL) {
        stList_append(ancestors2, parent);
        parent = parent->parent;
    }
    while (st3ECT_type(parent) != NET) {
        parent = parent->parent;
    }
    // just for clarification of what the variable actually is now.
    st3ECT *mrcaNet = parent;
    // here is where i would do the merging if i weren't lazy as hell
    (void) mrcaNet;

    st3ECT_destruct(node1);
    st3ECT_destruct(node2);
    stHash_remove(cactus->edgeToNode, node1);
    stHash_remove(cactus->edgeToNode, node2);
}

// callback for splitting an edge "vertically"
void stOnlineCactus_unalignEdge(stOnlineCactus *cactus, void *oldEdge,
                                void *edge1, void *edge2) {
    
}
void stOnlineCactus_printR(const st3ECT *tree, stHash *nodeToEdge) {
    if (tree->firstChild) {
        printf("(");
        st3ECT *child = tree->firstChild;
        while (child != NULL) {
            stOnlineCactus_printR(child, nodeToEdge);
            child = child->next;
            if (child != NULL) {
                printf(",");
            }
        }
        printf(")");
    }
    switch (st3ECT_type(tree)) {
    case NET:
        printf("NET%p", (void *) tree);
        break;
    case CHAIN:
        printf("CHAIN%p", (void *) tree);
        break;
    case BRIDGE:
        printf("BRIDGE%p", (void *) tree);
        break;
    case EDGE:
        printf("EDGE%p", (void *) stHash_search(nodeToEdge, (void *) tree));
        break;
    }
}

void stOnlineCactus_print(const stOnlineCactus *cactus) {
    stHash *nodeToEdge = stHash_invert(cactus->edgeToNode, stHash_pointer, stHash_getEqualityFunction(cactus->edgeToNode), NULL, NULL);
    stOnlineCactus_printR(cactus->tree, nodeToEdge);
    printf(";\n");
    stHash_destruct(nodeToEdge);
}
