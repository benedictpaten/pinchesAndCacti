#include <stdlib.h>

#include "sonLib.h"
#include "stOnlineCactus.h"
#include "stOnlineCactus_private.h"

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
        ret->parentEdge = edge;
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
    child->parentEdge->child = child;
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

static void stCactusTree_removeChild(stCactusTree *parent, stCactusTree *child) {
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

stCactusTreeEdge *stOnlineCactus_getEdge(stOnlineCactus *cactus, void *block) {
    return stHash_search(cactus->blockToEdge, block);
}

static void reassignParentEdge(stCactusTreeEdge *edge, stCactusTree *node) {
    node->parentEdge = edge;
    if (edge->child != NULL && edge->child != node) {
        edge->child->parentEdge = NULL;
    }
    edge->child = node;
}

// NB: ordering of nodes in the input list MUST reflect the ordering in their parent chain.
static void moveToNewChain(stList *nodes, stCactusTreeEdge *chainParentEdge,
                           stCactusTree *netToAttachNewChainTo) {
    // UPDATE PAPER: In fig 3, a block edge is in B) that was not in
    // A)--this is the anonymous chain node under 6,9
    stCactusTree *chain = stCactusTree_construct(netToAttachNewChainTo, NULL, CHAIN, NULL);
    reassignParentEdge(chainParentEdge, chain);
    for (int64_t i = 0; i < stList_length(nodes); i++) {
        stCactusTree *node = stList_get(nodes, i);
        stCactusTree *next = node->next;
        stCactusTree_removeChild(node->parent, node);
        node->next = next;
        if (i == 0) {
            node->prev = NULL;
            chain->firstChild = node;
        }
        if (i == stList_length(nodes) - 1) {
            if (next != NULL) {
                next->prev = NULL;
            }
            node->next = NULL;
        }
        node->parent = chain;
        printf("node %p: next: %p\n", (void *) node, (void *) node->next);
    }
}

// Merge node1 "into" node2, i.e. node1 will be deleted while node2
// will remain with all of node1's children.
static void mergeNets(stCactusTree *node1, stCactusTree *node2, stHash *endToNode) {
    stCactusTree *first = node1->firstChild;
    stCactusTree *cur = first;
    node1->firstChild = NULL;
    printf("%p\n", (void *) cur);
    while (cur != NULL) {
        cur->parent = node2;
        printf("cur %p, cur->next %p\n", (void *) cur, (void *) cur->next);
        if (cur->next == NULL) {
            printf("reassigning firstChild\n");
            cur->next = node2->firstChild;
            if (node2->firstChild != NULL) {
                cur->next->prev = cur;
            }
            break;
        }
        cur = cur->next;
    }
    if (first != NULL) {
        node2->firstChild = first;
    }

    // Remap every end of node1 to point to node2.
    stSetIterator *it = stSet_getIterator(node1->ends);
    void *end;
    while ((end = stSet_getNext(it)) != NULL) {
        mapEndToNode(endToNode, end, node2);
    }
    stSet_destructIterator(it);

    stCactusTree_destruct(node1);
}

static void stOnlineCactus_printR(const stCactusTree *, stHash *);

static stCactusTree *collapse3ECNetsBetweenChain(stCactusTree *node1, stCactusTree *node2, stHash *endToNode) {
    stCactusTree *chain = node1->parent;
    if (chain == node2->parent) {
        // Create lists of the nodes between node1 and node2 in the chain ordering.
        stList *between = stList_construct();

        // Figure out what direction node2 is in relation to node1.
        // TODO: this is pretty slow, and there is probably a more clever way of doing this
        stCactusTree *cur = node1;
        while (cur != NULL) {
            if (cur == node2) {
                break;
            }
            cur = cur->next;
        }
        if (cur == NULL) {
            // swap node1 and node2 to ensure that we always move right
            stCactusTree *tmp = node1;
            node1 = node2;
            node2 = tmp;
        }

        cur = node1->next;
        while (cur != NULL && cur != node2) {
            stList_append(between, cur);
        }
        moveToNewChain(between, node2->parentEdge, node2);
        stList_destruct(between);
        mergeNets(node2, node1, endToNode);
        return node1;
    } else {
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
        stList_reverse(before);
        stList *after = stList_construct();
        cur = node1->next;
        while (cur != NULL) {
            stList_append(after, cur);
            cur = cur->next;
        }

        printf("len(before) = %" PRIi64 "\n", stList_length(before));
        printf("len(after) = %" PRIi64 "\n", stList_length(after));

        // Detach all nodes in these lists from their parent and
        // create a new chain node for each. Their ordering must
        // be preserved.
        moveToNewChain(before, node1->parentEdge, node2);
        moveToNewChain(after, chain->parentEdge, node2);

        stList_destruct(before);
        stList_destruct(after);
        assert(chain->firstChild == node1);
        assert(node1->next == NULL);
        assert(node1->prev == NULL);

        // Merge the two nets and remove the now isolated chain node.
        mergeNets(node1, node2, endToNode);
        stCactusTree_destruct(chain);
        return node2;
    }
}

void collapse3ECNets(stCactusTree *node1, stCactusTree *node2,
                     stCactusTree **newNode1, stCactusTree **newNode2,
                     stHash *endToNode) {
    stCactusTree *mrca = stCactusTree_getMRCA(node1, node2);
    // Node1 -> mrca path
    printf("node1->mrca\n");
    while (node1->parent != NULL && node1->parent != mrca) {
        if (stCactusTree_type(node1->parent) == CHAIN) {
            printf("merging nodes %p and %p\n", (void *) node1, (void *) node1->parent->parent);
            node1 = collapse3ECNetsBetweenChain(node1, node1->parent->parent, endToNode);
        } else {
            // parent is a bridge
            node1 = node1->parent;
        }
    }

    // Node2->mrca path
    stOnlineCactus_printR(mrca, endToNode);
    printf("\n");
    printf("node2->mrca\n");
    while (node2->parent != NULL && node2->parent != mrca) {
        if (stCactusTree_type(node2->parent) == CHAIN) {
            printf("merging nodes %p and %p\n", (void *) node2, (void *) node2->parent->parent);
            node2 = collapse3ECNetsBetweenChain(node2, node2->parent->parent, endToNode);
            stOnlineCactus_printR(mrca, endToNode);
            printf("\n");
        } else {
            // parent is a bridge
            node2 = node2->parent;
        }
    }

    // check the path between the two now in case the mrca is a chain.
    if (stCactusTree_type(mrca) == CHAIN) {
        printf("between chain\n");
        printf("merging nodes %p and %p\n", (void *) node1, (void *) node2);
        node1 = node2 = collapse3ECNetsBetweenChain(node1, node2, endToNode);
    }
    *newNode1 = node1;
    *newNode2 = node2;
}

static void pathToChain(stList *pathUp, stList *pathDown, stCactusTree *mrca, stCactusTreeEdge *extraEdge) {
    stCactusTree *chain = stCactusTree_construct(mrca, NULL, CHAIN, NULL);
    // Assign the correct parent edge to the new chain node.
    if (stList_length(pathDown) > 0) {
        stCactusTree *tree = stList_get(pathDown, 0);
        reassignParentEdge(((stCactusTree *) stList_get(pathUp, 0))->parentEdge, chain);
        chain->firstChild = tree;
    } else {
        reassignParentEdge(extraEdge, chain);
        return;
    }
    for (int64_t i = 0; i < stList_length(pathDown); i++) {
        stCactusTree *tree = stList_get(pathDown, i);
        stCactusTree_removeChild(tree->parent, tree);
        if (i != stList_length(pathDown) - 1) {
            stCactusTree *next = stList_get(pathDown, i + 1);
            reassignParentEdge(next->parentEdge, tree);
            tree->next = next;
        }
        tree->parent = chain;
        if (i > 0) {
            tree->prev = stList_get(pathDown, i - 1);
        } else {
            tree->prev = NULL;
        }
    }

    for (int64_t i = 0; i < stList_length(pathUp); i++) {
        stCactusTree *tree = stList_get(pathUp, i);
        stCactusTree_removeChild(tree->parent, tree);
        tree->parent = chain;
        if (i < stList_length(pathUp) - 1) {
            tree->next = stList_get(pathUp, i + 1);
        }
        if (i > 0) {
            tree->prev = stList_get(pathUp, i - 1);
        } else {
            printf("connecting %p to %p\n", stList_get(pathDown, stList_length(pathDown) - 1), (void *) tree);
            tree->prev = stList_get(pathDown, stList_length(pathDown) - 1);
            tree->prev->next = tree;
            reassignParentEdge(extraEdge, tree);
            printf("parent edge of %p now %p\n", (void *) tree, (void *) tree->parentEdge);
        }
    }
}

void stOnlineCactus_netMerge(stOnlineCactus *cactus, void *end1, void *end2) {
    // UPDATE PAPER: if they are *NOT* in the same tree of CF(G) do the trivial merge
    stCactusTree *node1 = stHash_search(cactus->endToNode, end1);
    assert(node1 != NULL);
    stCactusTree *node2 = stHash_search(cactus->endToNode, end2);
    assert(node2 != NULL);
    if (node1 == node2) {
        return;
    } else if (stCactusTree_root(node1) != stCactusTree_root(node2)) {
        printf("trivial end merge of %p and %p\n", (void *) node1, (void *) node2);
        stOnlineCactus_print(cactus);
        // TODO: reroot
        // We can do a trivial merge since the two nodes are not in the same tree.
        mergeNets(node1, node2, cactus->endToNode);
        stList_removeItem(cactus->trees, node1);
    } else {
        printf("nontrivial end merge of %p and %p\n", (void *) node1, (void *) node2);
        stOnlineCactus_print(cactus);
        // They're in the same tree so we have to do the tricky bit.
        collapse3ECNets(node1, node2, &node1, &node2, cactus->endToNode);
        if (node1 == node2) {
            // UPDATE PAPER: a cycle may not exist after
            // edge removal if the two nodes got squashed together.

            // All done.
            return;
        }
        printf("after collapse3ECNets\n");
        stOnlineCactus_print(cactus);
        // Find the path between node1 and node2.
        stCactusTree *mrca = stCactusTree_getMRCA(node1, node2);
        // node1->mrca path, exclusive of mrca
        stList *pathUp = stList_construct();
        stCactusTree *cur = node1;
        while (cur != mrca) {
            stList_append(pathUp, cur);
            cur = cur->parent;
        }
        // node2->mrca path, exclusive of mrca, which should be
        // reversed and added to the existing half-path.
        stList *pathDown = stList_construct();
        cur = node2;
        while (cur != mrca) {
            stList_append(pathDown, cur);
            cur = cur->parent;
        }
        stList_reverse(pathDown);
        // Fake node1->node2 edge.
        stCactusTreeEdge *extraEdge = calloc(1, sizeof(extraEdge));
        extraEdge->child = node1;
        // Create a new chain.
        printf("pathUp = {");
        for (int64_t i = 0; i < stList_length(pathUp); i++) {
            printf("%p,", stList_get(pathUp, i));
        }
        printf("}\n");
        printf("pathDown = {");
        for (int64_t i = 0; i < stList_length(pathDown); i++) {
            printf("%p,", stList_get(pathDown, i));
        }
        printf("}\n");
        printf("mrca = %p\n", (void *) mrca);
        pathToChain(pathUp, pathDown, mrca, extraEdge);
        printf("before fake edge contraction\n");
        stOnlineCactus_print(cactus);
        stList_destruct(pathUp);
        stList_destruct(pathDown);
        // Contract the nodes.
        mergeNets(node1, node2, cactus->endToNode);
    }
}

void stOnlineCactus_deleteEdge(stOnlineCactus *cactus, void *end1, void *end2, void *block) {
    stCactusTree *node1 = stHash_search(cactus->endToNode, end1);
    assert(node1 != NULL);
    stCactusTree *node2 = stHash_search(cactus->endToNode, end2);
    assert(node2 != NULL);
    assert(stCactusTree_root(node1) == stCactusTree_root(node2));
    unmapEndToNode(cactus->endToNode, end1, node1);
    unmapEndToNode(cactus->endToNode, end2, node2);
    if (node1->parent == node2 || node2->parent == node1) {
        // a bridge edge
        assert(stCactusTree_type(node1) == NET);
        assert(stCactusTree_type(node2) == NET);
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
        stCactusTreeEdge *edge = stHash_search(cactus->blockToEdge, block);
        (void) edge;
        assert(edge->child == child);
        stHash_remove(cactus->blockToEdge, block);
        free(child->parentEdge);
        child->parentEdge = NULL;
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
        // part of a chain. Delete the relevant edge and unravel the chain from either end.
        stCactusTreeEdge *edge = stHash_search(cactus->blockToEdge, block);
        stCactusTree *tree = edge->child;
        if (stCactusTree_type(tree) == CHAIN) {
            // This is the "parent edge" of the chain.
            stHash_remove(cactus->blockToEdge, block);
            free(edge);
            tree->parentEdge = NULL;
            stCactusTree *prev = tree->parent;
            stCactusTree *cur = tree->firstChild;
            tree->firstChild = NULL;
            while (cur != NULL) {
                cur->parent = prev;
                stCactusTree *next = cur->next;
                cur->prev = NULL;
                cur->next = prev->firstChild;
                prev->firstChild = cur;
                prev = cur;
                cur = next;
            }
            stCactusTree_removeChild(tree->parent, tree);
        } else {
            // Internal edge
            stCactusTree *chain = tree->parent;
            assert(stCactusTree_type(chain) == CHAIN);
            // Unravel from the "left" direction.
            stCactusTree *cur = chain->firstChild;
            stCactusTree *prev = chain->parent;
            chain->firstChild = NULL;
            while (cur != tree) {
                cur->parent = prev;
                stCactusTree *next = cur->next;
                cur->prev = NULL;
                cur->next = prev->firstChild;
                prev->firstChild = cur;
                prev = cur;
                cur = next;
            }
            // Unravel from the "right" direction.
            stHash_remove(cactus->blockToEdge, block);
            free(tree->parentEdge);
            while (cur->next != NULL) {
                reassignParentEdge(cur->next->parentEdge, cur);
                cur->parent = cur->next;
                stCactusTree *next = cur->next;
                cur->prev = NULL;
                cur->next = next->firstChild;
                next->firstChild = cur;
                cur = next;
            }
            reassignParentEdge(chain->parentEdge, cur);
            cur->parent = chain->parent;
            stCactusTree_removeChild(chain->parent, chain);
            cur->prev = NULL;
            cur->next = cur->parent->firstChild;
            cur->parent->firstChild = cur;
        }
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
            if (child->parentEdge != NULL) {
                printf(")BLOCK%p", child->parentEdge->block);
            } else {
                printf(")BLOCK_INVALID");
            }
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
    default:
        st_errAbort("dangling pointer");
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
        if (tree->parentEdge->child != tree) {
            st_errAbort("parentEdge<->child link is broken");
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
        if (tree->type == CHAIN && tree->parent->type == CHAIN) {
            st_errAbort("chain-chain edge");
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
