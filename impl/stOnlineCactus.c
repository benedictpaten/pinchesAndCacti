#include <stdlib.h>

#include "sonLib.h"
#include "stOnlineCactus.h"

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
};

struct _stCactusTreeEdge {
    stCactusTree *child;
    void *block;
};

struct _stOnlineCactus {
    stList *trees;
    stHash *blockToEdge;
    stHash *endToNode;
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

stCactusTree *stCactusTree_construct(stCactusTree *parent, stCactusTree *leftSib, cactusNodeType type, void *block) {
    stCactusTreeEdge *edge = NULL;
    if (block != NULL) {
        edge = calloc(1, sizeof(stCactusTreeEdge));
        edge->block = block;
    }
    stCactusTree *ret = calloc(1, sizeof(stCactusTree));
    if (edge != NULL) {
        edge->child = ret;
    }
    ret->type = type;
    if (parent != NULL) {
        if (type == CHAIN) {
            assert(stCactusTree_type(parent) == NET);
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

stCactusTree *stCactusTree_parentNode(stCactusTree *tree) {
    if (tree->parent == NULL) {
        return NULL;
    }
    return tree->parent->parent;
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
static void stCactusTree_prependChild(stCactusTree *tree, stCactusTree *child, void *block) {
    child->parent = tree;
    stCactusTree *oldFirst = tree->firstChild;
    tree->firstChild = child;
    child->parentEdge = calloc(1, sizeof(stCactusTreeEdge));
    child->parentEdge->block = block;
    child->next = oldFirst;
    if (oldFirst != NULL) {
        oldFirst->prev = child;
    }
}

// Find the root of the tree given any node in the tree.
static stCactusTree *stCactusTree_root(stCactusTree *tree) {
    if (tree->parent == NULL) {
        return tree;
    }
    return stCactusTree_root(tree->parent);
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
    ret->trees = stList_construct();
    ret->endToNode = stHash_construct();
    ret->blockToEdge = stHash_construct();
    return ret;
}

stList *stOnlineCactus_getCactusTrees(stOnlineCactus *cactus) {
    return cactus->trees;
}

// Get the two nodes associated with an edge in the graph (which may
// involve multiple, or no, edges in the tree).
void stOnlineCactus_getNodes(stOnlineCactus *cactus, void *edge,
                                    stCactusTree **node1, stCactusTree **node2) {
    void *end1 = cactus->edgeToEnd(edge, 0);
    void *end2 = cactus->edgeToEnd(edge, 1);
    *node1 = stHash_search(cactus->endToNode, end1);
    assert(*node1 != NULL);
    assert(stCactusTree_type(*node1) == NET);
    *node2 = stHash_search(cactus->endToNode, end2);
    assert(*node2 != NULL);
    assert(stCactusTree_type(*node2) == NET);
}

static void stOnlineCactus_check3EC() {
    
}

// Add a new end to the cactus forest.
void stOnlineCactus_createEnd(stOnlineCactus *cactus, void *end) {
    assert(stHash_search(cactus->endToNode, end) == NULL);

    // Search for other ends in the same adjacency component.
    stConnectedComponent *component = stConnectivity_getConnectedComponent(cactus->adjacencyComponents, end);
    stConnectedComponentNodeIterator *it = stConnectedComponent_getNodeIterator(component);
    void *curEnd;
    stCactusTree *node = NULL;
    while ((curEnd = stConnectedComponentNodeIterator_getNext(it)) != NULL) {
        node = stHash_search(cactus->endToNode, curEnd);
        if (node != NULL) {
            break;
        }
    }
    stConnectedComponentNodeIterator_destruct(it);
    if (node != NULL) {
        stHash_insert(cactus->endToNode, end, node);
        return;
    }
    stCactusTree *new = stCactusTree_construct(NULL, NULL, NET, NULL);
    stHash_insert(cactus->endToNode, end, new);
    stList_append(cactus->trees, new);
}

// Add a new edge between the adjacency components containing two existing ends.
void stOnlineCactus_addEdge(stOnlineCactus *cactus, void *end1, void *end2, void *edge) {
    stCactusTree *node1 = stHash_search(cactus->endToNode, end1);
    assert(node1 != NULL);
    stCactusTree *node2 = stHash_search(cactus->endToNode, end2);
    assert(node2 != NULL);

    // Are node1 and node2 in the same tree?
    if (stCactusTree_root(node1) == stCactusTree_root(node2)) {
        st_errAbort("not implemented");
    } else {
        // Simply add node2 as a child of node1.
        stList_removeItem(cactus->trees, stCactusTree_root(node2));
        stCactusTree_prependChild(node1, node2, edge);
        assert(stList_contains(cactus->trees, stCactusTree_root(node2)));
        stHash_insert(cactus->blockToEdge, edge, node2->parentEdge);
    }
}

void stOnlineCactus_netCleave() {
    
}

void stOnlineCactus_netMerge() {
    
}

void stOnlineCactus_deleteEdge(stOnlineCactus *cactus, void *end1, void *end2) {
    stCactusTree *node1 = stHash_search(cactus->endToNode, end1);
    assert(node1 != NULL);
    stCactusTree *node2 = stHash_search(cactus->endToNode, end2);
    assert(node2 != NULL);
    if (node1->parent == node2 || node2->parent == node1) {
        // a bridge edge
        stCactusTree *child;
        stCactusTree *parent;
        if (node1->parent == node2) {
            child = node1;
            parent = node2;
        } else {
            child = node2;
            parent = node1;
        }
        stList_append(cactus->trees, child);
        free(child->parentEdge);
        child->parentEdge = NULL;
        child->parent = NULL;
        if (child == parent->firstChild) {
            parent->firstChild = child->next;
        }
        if (child->prev != NULL) {
            child->prev->next = child->next;
        }
        if (child->next != NULL) {
            child->next->prev = child->prev;
        }
        child->prev = NULL;
        child->next = NULL;
    } else if (node1 == node2) {
        
    } else {
        // part of a chain
        st_errAbort("not implemented");
    }
    stOnlineCactus_check3EC();
    stHash_remove(cactus->endToNode, end1);
    stHash_remove(cactus->endToNode, end2);
}

void stOnlineCactus_printR(const stCactusTree *tree, stHash *nodeToEnd) {
    if (tree->firstChild) {
        printf("(");
        stCactusTree *child = tree->firstChild;
        while (child != NULL) {
            printf("(");
            stOnlineCactus_printR(child, nodeToEnd);
            printf(")BLOCK%p", child->parentEdge->block);
            child = child->next;
            if (child != NULL) {
                printf(",");
            }
        }
        printf(")");
    }
    switch (stCactusTree_type(tree)) {
    case NET:
        printf("NET%p", (void *) tree);
        break;
    case CHAIN:
        printf("CHAIN%p", (void *) tree);
        break;
    }
}

void stOnlineCactus_print(const stOnlineCactus *cactus) {
    stHash *nodeToEnd = stHash_invert(cactus->endToNode, stHash_pointer, stHash_getEqualityFunction(cactus->endToNode), NULL, NULL);
    for (int64_t i = 0; i < stList_length(cactus->trees); i++) {
        stOnlineCactus_printR(stList_get(cactus->trees, i), nodeToEnd);
        printf(";\n");
    }
    stHash_destruct(nodeToEnd);
}
