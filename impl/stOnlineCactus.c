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
    // NULL if this is a chain node.
    // This is needed because we don't have the guarantee (yet, anyway) that
    // the connected components have consistent pointers over time.
    stSet *ends;
};

typedef struct {
    stCactusTree *net;
} stCactusEndMapping;

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

    if (type == NET) {
        ret->ends = stSet_construct();
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
    if (tree->ends) {
        stSet_destruct(tree->ends);
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

static void stOnlineCactus_check3EC() {
}

static void mapEndToNode(stHash *endToNode, void *end, stCactusTree *node) {
    stHash_insert(endToNode, end, node);
    stSet_insert(node->ends, end);
}

static void unmapEndToNode(stHash* endToNode, void *end, stCactusTree *node) {
    assert(stSet_search(node->ends, end));
    stSet_remove(node->ends, end);
    stHash_remove(endToNode, end);
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
        mapEndToNode(cactus->endToNode, end, node);
        return;
    }
    stCactusTree *new = stCactusTree_construct(NULL, NULL, NET, NULL);
    mapEndToNode(cactus->endToNode, end, new);
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
        if (node1 == node2) {
            // Simplest case -- the two nodes are identical and we
            // need to add a self-loop chain.
            stCactusTree *chain = stCactusTree_construct(node1, NULL, CHAIN, edge);
            stHash_insert(cactus->blockToEdge, edge, chain->parentEdge);
        } else {
            st_errAbort("not implemented");
        }
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

static stCactusTree *stCactusTree_getMRCA(stCactusTree *node1, stCactusTree *node2) {
    stSet *node1Anc = stSet_construct();
    while (node1 != NULL) {
        stSet_insert(node1Anc, node1);
        node1 = node1->parent;
    }
    while (node2 != NULL) {
        if (stSet_search(node1Anc, node2) != NULL) {
            return node2;
        }
        node2 = node2->parent;
    }
    return NULL;
}

// NB: ordering of nodes in the input list MUST reflect the ordering in their parent chain.
static void moveToNewChain(stList *nodes, stCactusTreeEdge *chainParentEdge,
                           stCactusTree *netToAttachNewChainTo) {
    // UPDATE PAPER: In fig 3, a block edge is in B) that was not in
    // A)--this is the anonymous chain node under 6,9
    stCactusTree *chain = stCactusTree_construct(netToAttachNewChainTo, NULL, CHAIN, NULL);
    chain->parentEdge = chainParentEdge;
    chainParentEdge->child->parentEdge = NULL;
    chainParentEdge->child = chain;
    for (int64_t i = 0; i < stList_length(nodes); i++) {
        stCactusTree *node = stList_get(nodes, i);
        if (i == 0) {
            node->prev = NULL;
            chain->firstChild = node;
        }
        if (i == stList_length(nodes)) {
            if (node->next != NULL) {
                node->next->prev = NULL;
            }
            node->next = NULL;
        }
        node->parent = chain;
    }
}

// Merge node1 "into" node2, i.e. node1 will be deleted while node2
// will remain with all of node1's children.
static void mergeNets(stCactusTree *node1, stCactusTree *node2, stHash *endToNode) {
    stCactusTree *first = node1->firstChild;
    stCactusTree *cur = first;
    node1->firstChild = NULL;
    while (cur != NULL) {
        cur->parent = node2;
        if (cur->next == NULL) {
            cur->next = node2->firstChild;
            if (node2->firstChild != NULL) {
                cur->next->prev = cur;
            }
            break;
        }
        cur = cur->next;
    }
    node2->firstChild = first;

    // Remap every end of node1 to point to node2.
    stSetIterator *it = stSet_getIterator(node1->ends);
    void *end;
    while ((end = stSet_getNext(it)) != NULL) {
        mapEndToNode(endToNode, end, node2);
    }
    stSet_destructIterator(it);

    stCactusTree_destruct(node1);
}

void collapse3ECNets(stCactusTree *node1, stCactusTree *node2, stHash *endToNode) {
    stCactusTree *mrca = stCactusTree_getMRCA(node1, node2);
    // Node1 -> mrca path
    while (node1 != mrca) {
        if (stCactusTree_type(node1->parent) == CHAIN) {
            stCactusTree *chain = node1->parent;
            // Create lists of the nodes before and after node1 in the chain ordering.
            // UPDATE PAPER: currently only one list X is created.
            // UPDATE PAPER: N_l should presumably be
            //               N_j'. Realistically there should be two: N_j+ and N_j-.
            stList *before = stList_construct();
            stCactusTree *cur = node1->prev;
            while (cur != NULL) {
                stList_append(before, cur);
                cur = cur->prev;
            }
            stList *after = stList_construct();
            cur = node1->next;
            while (cur != NULL) {
                stList_append(after, cur);
                cur = cur->next;
            }
            // Detach all nodes in these lists from their parent and
            // create a new chain node for each. Their ordering must
            // be preserved.
            moveToNewChain(before, node1->parentEdge, chain->parent);
            moveToNewChain(after, chain->parentEdge, chain->parent);
            assert(chain->firstChild == node1);
            assert(node1->next == NULL);
            assert(node1->prev == NULL);

            // Merge the two nets and remove the now isolated chain node.
            mergeNets(node1, node1->parent->parent, endToNode);
            stCactusTree_destruct(chain);
        } else {
            // parent is a bridge
            node1 = node1->parent;
        }
    }

    // Node2->mrca path

    // check the path between the two now in case the mrca is a chain.
}

void stOnlineCactus_netMerge(stOnlineCactus *cactus, void *end1, void *end2) {
    stCactusTree *node1 = stHash_search(cactus->endToNode, end1);
    assert(node1 != NULL);
    stCactusTree *node2 = stHash_search(cactus->endToNode, end2);
    assert(node2 != NULL);
    if (node1 == node2) {
        return;
    }
    
}

static void stCactusTree_removeChild(stCactusTree *parent, stCactusTree *child) {
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
}

void stOnlineCactus_deleteEdge(stOnlineCactus *cactus, void *end1, void *end2, void *block) {
    stCactusTree *node1 = stHash_search(cactus->endToNode, end1);
    assert(node1 != NULL);
    stCactusTree *node2 = stHash_search(cactus->endToNode, end2);
    assert(node2 != NULL);
    unmapEndToNode(cactus->endToNode, end1, node1);
    unmapEndToNode(cactus->endToNode, end2, node2);
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
        stCactusTree_removeChild(parent, child);
    } else if (node1 == node2) {
        // endpoints are in the same net, we need to delete a chain
        stCactusTreeEdge *edge = stHash_search(cactus->blockToEdge, block);
        stCactusTree *chain = edge->child;
        assert(stCactusTree_type(chain) == CHAIN);
        assert(chain->firstChild == NULL);
        stCactusTree_removeChild(node1, chain);
        stCactusTree_destruct(chain);
    } else {
        // part of a chain
        st_errAbort("not implemented");
    }
    stOnlineCactus_check3EC();
    if (stSet_size(node1->ends) == 0) {
        stCactusTree_destruct(node1);
        stList_removeItem(cactus->trees, node1);
    }
    if (node1 != node2 && stSet_size(node2->ends) == 0) {
        stCactusTree_destruct(node2);
        stList_removeItem(cactus->trees, node2);
    }
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

static void stCactusTree_check(stCactusTree *tree, stOnlineCactus *cactus) {
    if (tree->ends != NULL) {
        if (stCactusTree_type(tree) == CHAIN) {
            st_errAbort("chain has registered ends");
        }
        if (stSet_size(tree->ends) == 0) {
            st_errAbort("dangling node with no ends");
        }
        stSetIterator *it = stSet_getIterator(tree->ends);
        void *end;
        while ((end = stSet_getNext(it)) != NULL) {
            stConnectedComponent *component = stConnectivity_getConnectedComponent(cactus->adjacencyComponents, end);
            // Check that each of the ends in the adjacency component belong
            // to the node (or none, if they are not representative ends).
            stConnectedComponentNodeIterator *compIt = stConnectedComponent_getNodeIterator(component);
            void *end2;
            while ((end2 = stConnectedComponentNodeIterator_getNext(compIt)) != NULL) {
                stCactusTree *node = stHash_search(cactus->endToNode, end2);
                if (node != NULL && node != tree) {
                    st_errAbort("adjacency component contains end in different node");
                }
            }
        }
        stSet_destructIterator(it);
    }
    if (tree->parentEdge != NULL) {
        if (tree->parent == NULL) {
            st_errAbort("tree has parent edge but no parent");
        }
        // Check that this parent edge is actually connecting what it's supposed to in the tree.
        void *block = tree->parentEdge->block;
        void *end1 = cactus->edgeToEnd(block, 0);
        void *end2 = cactus->edgeToEnd(block, 1);
        if (tree->type == NET) {
            bool end1present = stSet_search(tree->ends, end1);
            bool end2present = stSet_search(tree->ends, end2);
            if (!end1present && !end2present) {
                st_errAbort("block edge is not even attached to correct node");
            }
            if (tree->parent->type == CHAIN) {
                // Chain edge.
                if (tree->prev == NULL) {
                    // This edge connects the current net to the grandparent
                    bool end1presentGP = stSet_search(tree->parent->parent->ends, end1);
                    bool end2presentGP = stSet_search(tree->parent->parent->ends, end2);
                    if (!((end1present && end2presentGP) || (end2present && end1presentGP))) {
                        st_errAbort("first chain (front-edge) not placed correctly");
                    }
                } else {
                    // This edge connects the current net to the one previous.
                    bool end1presentPrev = stSet_search(tree->prev->ends, end1);
                    bool end2presentPrev = stSet_search(tree->prev->ends, end2);
                    if (!((end1present && end2presentPrev) || (end2present && end1presentPrev))) {
                        st_errAbort("internal chain link not placed correctly");
                    }
                }
            } else {
                // Bridge edge.
                assert(tree->parent->type == NET);
                bool end1presentParent = stSet_search(tree->parent->ends, end1);
                bool end2presentParent = stSet_search(tree->parent->ends, end2);
                if (!((end1present && end2presentParent) || (end2present && end1presentParent))) {
                    st_errAbort("bridge edge is incorrectly placed");
                }
            }
        } else {
            // Find the last child of the chain ordering.
            stCactusTree *child = tree->firstChild;
            while (child != NULL && child->next != NULL) {
                child = child->next;
            }
            if (child == NULL) {
                // The parent edge should connect the parent net to itself.
                if (!stSet_search(tree->parent->ends, end1) || !stSet_search(tree->parent->ends, end2)) {
                    st_errAbort("parent edge of self-loop chain is incorrect");
                }
            } else {
                // The parent edge connects the last child to the parent net.
                bool end1presentChild = stSet_search(child->ends, end1);
                bool end2presentChild = stSet_search(child->ends, end2);
                bool end1presentParent = stSet_search(tree->parent->ends, end1);
                bool end2presentParent = stSet_search(tree->parent->ends, end2);
                if (!((end1presentChild && end2presentParent) || (end2presentChild && end1presentParent))) {
                    st_errAbort("parent edge of chain does not connect last child to parent net");
                }
            }
        }
    }
    if (tree->parent != NULL) {
        if (tree->parentEdge == NULL) {
            st_errAbort("tree has parent but no parent edge");
        }
    } else {
        if (tree->type == CHAIN) {
            // Isolated chain nodes are no good.
            st_errAbort("isolated chain node");
        }
    }
    // Check sibling ordering is consistent.
    if (tree->next != NULL) {
        if (tree->next->prev != tree) {
            st_errAbort("sibling ordering is incorrectly linked");
        }
    }

    if (tree->prev != NULL) {
        if (tree->prev->next != tree) {
            st_errAbort("sibling ordering is incorrectly linked");
        }
    } else {
        if (tree->parent != NULL) {
            if (tree->parent->firstChild != tree) {
                st_errAbort("tree has parent and no previous sibling but is not first child");
            }
        }
    }

    // Recurse on children.
    stCactusTree *child = tree->firstChild;
    while (child != NULL) {
        stCactusTree_check(child, cactus);
        child = child->next;
    }
}

// Make sure Joel hasn't fucked anything up.
void stOnlineCactus_check(stOnlineCactus *cactus) {
    for (int64_t i = 0; i < stList_length(cactus->trees); i++) {
        stCactusTree_check(stList_get(cactus->trees, i), cactus);
    }
}
