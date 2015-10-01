#include <stdlib.h>

#include "sonLib.h"
#include "3_Absorb3edge2x.h"
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
        ret->nodes = stSet_construct();
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
    if (tree->parentEdge != NULL) {
        free(tree->parentEdge);
    }
    if (tree->prev != NULL) {
        tree->prev->next = tree->next;
    } else if (tree->parent != NULL) {
        tree->parent->firstChild = tree->next;
    }
    if (tree->next != NULL) {
        tree->next->prev = tree->prev;
    }
    if (tree->nodes) {
        stSet_destruct(tree->nodes);
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
stOnlineCactus *stOnlineCactus_construct(void *(*edgeToEnd)(void *, bool),
                                         void *(*endToEdge)(void *)) {
    stOnlineCactus *ret = calloc(1, sizeof(stOnlineCactus));
    ret->edgeToEnd = edgeToEnd;
    ret->endToEdge = endToEdge;
    ret->trees = stList_construct3(0, (void (*)(void *)) stCactusTree_destruct);
    ret->endToNode = stHash_construct();
    ret->nodeToEnds = stHash_construct2(NULL, (void (*)(void *)) stSet_destruct);
    ret->nodeToNet = stHash_construct();
    ret->blockToEdge = stHash_construct();
    return ret;
}

void stOnlineCactus_destruct(stOnlineCactus *cactus) {
    stHash_destruct(cactus->endToNode);
    stHash_destruct(cactus->blockToEdge);
    stHash_destruct(cactus->nodeToNet);
    stHash_destruct(cactus->nodeToEnds);
    stList_destruct(cactus->trees);
    free(cactus);
}

stList *stOnlineCactus_getCactusTrees(stOnlineCactus *cactus) {
    return cactus->trees;
}

static stCactusTree *getNetFromEnd(stOnlineCactus *cactus, void *end) {
    return stHash_search(cactus->nodeToNet, stHash_search(cactus->endToNode, end));
}

// Get the block end that belongs to this net. Undefined behavior if both ends are in the net.
static void *getBlockEndCorrespondingToNet(stOnlineCactus *cactus, void *block, stCactusTree *net) {
    assert(block != NULL);
    void *end1 = cactus->edgeToEnd(block, 0);
    void *end2 = cactus->edgeToEnd(block, 1);
    void *end = NULL;
    if (getNetFromEnd(cactus, end1) == net) {
        end = end1;
    }
    if (getNetFromEnd(cactus, end2) == net) {
        assert(end == NULL);
        end = end2;
    }
    assert(end != NULL);
    return end;
}

static void getEndsInChildChainAffectingParentNode(stOnlineCactus *cactus, stCactusTree *parent, stCactusTree *child, void **end1, void **end2) {
    assert(child->parent == parent);
    if (child->firstChild == NULL) {
        // Self-loop.
        *end1 = cactus->edgeToEnd(child->parentEdge->block, 0);
        *end2 = cactus->edgeToEnd(child->parentEdge->block, 1);
    } else {
        // Chain with at least one child.
        *end1 = getBlockEndCorrespondingToNet(cactus, child->parentEdge->block, parent);
        *end2 = getBlockEndCorrespondingToNet(cactus, child->firstChild->parentEdge->block, parent);
    }
}

static void getEndsInParentChainAffectingChildNode(stOnlineCactus *cactus, stCactusTree *tree,
                                                   void **parentEnd, void **neighborEnd) {
    // Get the end in this net corresponding to the parent edge.
    assert(tree->parentEdge != NULL);
    *parentEnd = getBlockEndCorrespondingToNet(cactus, tree->parentEdge->block, tree);

    // Get the end in this net corresponding to the next neighbor's parent.
    stCactusTree *neighbor;
    if (tree->next == NULL) {
        neighbor = tree->parent;
    } else {
        neighbor = tree->next;
    }
    assert(neighbor->parentEdge != NULL);
    *neighborEnd = getBlockEndCorrespondingToNet(cactus, neighbor->parentEdge->block, tree);
}

// Get a net graph as an adjacency list (a list of lists of stIntTuples).
static stList* stCactusTree_getNetGraph(stOnlineCactus *cactus, stCactusTree *tree, stHash **nodeToIndex) {
    // Find the end graph (the bipartite graph representing
    // block-edge-connectivity between ends).
    //
    // There are exactly two ways for two ends in the same net to be
    // block-edge-connected:
    //
    // 1) The net has a chain parent--in which case the two ends in
    // the net corresponding to its parent edge and its next
    // neighbor's parent edge (or the chain parent edge if the last
    // node) are block-edge-connected.
    //
    // 2) For each chain child, the two ends in the net corresponding
    // to either the sole block edge (if a self-loop chain) or the
    // parent->chain block edge and chain->firstChild block edge
    // (otherwise) are block-edge-connected.
    //
    // This follows from the definition of 3-edge-connectivity and
    // chain/bridge edges. These two cases are only one case in
    // unrooted trees, of course.
    //
    // The end graph is just represented as a hash from end to end, as
    // each end has exactly one edge.
    stHash *endGraph = stHash_construct();

    // First we consider 1).
    if (tree->parent != NULL && stCactusTree_type(tree->parent) == CHAIN) {
        void *parentEnd;
        void *neighborEnd;
        getEndsInParentChainAffectingChildNode(cactus, tree, &parentEnd, &neighborEnd);
        // Mark down that parentEnd and neighborEnd are block-edge-connected.
        stHash_insert(endGraph, parentEnd, neighborEnd);
        stHash_insert(endGraph, neighborEnd, parentEnd);
    }

    // Next we consider 2).
    stCactusTree *curChild = tree->firstChild;
    while (curChild != NULL) {
        if (stCactusTree_type(curChild) == CHAIN) {
            void *end1;
            void *end2;
            getEndsInChildChainAffectingParentNode(cactus, tree, curChild, &end1, &end2);
            // Mark down that end1 and end2 are block-edge-connected.
            stHash_insert(endGraph, end1, end2);
            stHash_insert(endGraph, end2, end1);
        }
        curChild = curChild->next;
    }

    stList *netGraph = stList_construct3(0, (void (*)(void *)) stList_destruct);
    *nodeToIndex = stHash_construct2(NULL, (void (*)(void *)) stIntTuple_destruct);
    // Create the vertices of the net graph, i.e. the nodes in this net.
    stSetIterator *nodeIt = stSet_getIterator(tree->nodes);
    void *node;
    while ((node = stSet_getNext(nodeIt)) != NULL) {
        // Need to make a new list for the adjacencies of this node.
        stIntTuple *i = stIntTuple_construct1(stList_length(netGraph));
        stHash_insert(*nodeToIndex, node, i);
        stList_append(netGraph, stList_construct3(0, (void (*)(void *)) stIntTuple_destruct));
    }
    stSet_destructIterator(nodeIt);
    // Collapse the end graph by merging together ends in the same
    // node, then add the edges to the net graph.
    stHashIterator *endIt = stHash_getIterator(endGraph);
    void *end;
    while ((end = stHash_getNext(endIt)) != NULL) {
        void *node = stHash_search(cactus->endToNode, end);
        stIntTuple *i = stHash_search(*nodeToIndex, node);
        void *otherEnd = stHash_search(endGraph, end);
        void *otherNode = stHash_search(cactus->endToNode, otherEnd);
        stIntTuple *j = stHash_search(*nodeToIndex, otherNode);
        stList *componentEdges = stList_get(netGraph, stIntTuple_get(i, 0));
        stList_append(componentEdges, stIntTuple_construct1(stIntTuple_get(j, 0)));
    }
    stHash_destructIterator(endIt);
    stHash_destruct(endGraph);
    return netGraph;
}

static stList *get3ECComponents(stOnlineCactus *cactus, stCactusTree *tree, stHash **nodeToIndex) {
    stList *netGraph = stCactusTree_getNetGraph(cactus, tree, nodeToIndex);
    stList *components = computeThreeEdgeConnectedComponents(netGraph);
    stList_destruct(netGraph);
    return components;
}

static bool stOnlineCactus_check3EC(stOnlineCactus *cactus, stCactusTree *tree) {
    if (tree->parent == NULL && tree->firstChild == NULL) {
        // Isolated nodes are trivially 3-edge-connected.
        return true;
    }
    stHash *nodeToIndex;
    stList *components = get3ECComponents(cactus, tree, &nodeToIndex);
    stHash_destruct(nodeToIndex);
    bool ret = stList_length(components) == 1;
    stList_destruct(components);
    return ret;
}

static void mapEndToNode(stOnlineCactus *cactus, void *end, void *node) {
    stHash_insert(cactus->endToNode, end, node);
    stSet *ends;
    if ((ends = stHash_search(cactus->nodeToEnds, node)) == NULL) {
        ends = stSet_construct();
        stHash_insert(cactus->nodeToEnds, node, ends);
    }
    stSet_insert(ends, end);
}

static void unmapEndToNode(stOnlineCactus *cactus, void *end, void *node) {
    stSet *ends = stHash_search(cactus->nodeToEnds, node);
    assert(stSet_search(ends, end));
    stSet_remove(ends, end);
    assert(stHash_search(cactus->endToNode, end));
    stHash_remove(cactus->endToNode, end);
}

void stOnlineCactus_createNode(stOnlineCactus *cactus, void *node) {
    assert(stHash_search(cactus->nodeToNet, node) == NULL);

    stCactusTree *new = stCactusTree_construct(NULL, NULL, NET, NULL);
    stSet_insert(new->nodes, node);
    stHash_insert(cactus->nodeToNet, node, new);
    stHash_insert(cactus->nodeToEnds, node, stSet_construct());
    stList_append(cactus->trees, new);
}

void stOnlineCactus_deleteNode(stOnlineCactus *cactus, void *node) {
    stCactusTree *net = stHash_search(cactus->nodeToNet, node);
    assert(net != NULL);

    assert(stSet_size(net->nodes) == 1);

    stSet *ends = stHash_search(cactus->nodeToEnds, node);
    assert(stSet_size(ends) == 0);
    stSet_destruct(ends);

    stHash_remove(cactus->nodeToNet, node);

    assert(stList_contains(cactus->trees, node));
    stList_removeItem(cactus->trees, node);

    stCactusTree_destruct(net);
}

static void reassignParentEdge(stCactusTreeEdge *edge, stCactusTree *node) {
    node->parentEdge = edge;
    if (edge->child != NULL && edge->child != node) {
        edge->child->parentEdge = NULL;
    }
    edge->child = node;
}

// Unravel a chain by deleting a given edge. The edge *must* be part of a chain.
static void unravelChain(stOnlineCactus *cactus, stCactusTreeEdge *edge) {
    stCactusTree *tree = edge->child;
    if (stCactusTree_type(tree) == CHAIN) {
        // This is the "parent edge" of the chain.
        stHash_remove(cactus->blockToEdge, edge->block);
        free(edge);
        tree->parentEdge = NULL;
        stCactusTree *prev = tree->parent;
        stCactusTree_removeChild(tree->parent, tree);
        stCactusTree *cur = tree->firstChild;
        tree->firstChild = NULL;
        while (cur != NULL) {
            cur->parent = prev;
            stCactusTree *next = cur->next;
            cur->prev = NULL;
            cur->next = prev->firstChild;
            if (cur->next != NULL) {
                cur->next->prev = cur;
            }
            prev->firstChild = cur;
            prev = cur;
            cur = next;
        }
        stCactusTree_destruct(tree);
    } else {
        // Internal edge
        stCactusTree *chain = tree->parent;
        assert(stCactusTree_type(chain) == CHAIN);
        // Unravel from the "left" direction.
        stCactusTree *cur = chain->firstChild;
        stCactusTree *chainParent = chain->parent;
        stCactusTree *prev = chainParent;
        stCactusTree_removeChild(chain->parent, chain);
        chain->firstChild = NULL;
        while (cur != tree) {
            cur->parent = prev;
            stCactusTree *next = cur->next;
            cur->prev = NULL;
            cur->next = prev->firstChild;
            if (cur->next != NULL) {
                cur->next->prev = cur;
            }
            prev->firstChild = cur;
            prev = cur;
            cur = next;
        }
        // Unravel from the "right" direction.
        stHash_remove(cactus->blockToEdge, edge->block);
        free(tree->parentEdge);
        while (cur->next != NULL) {
            reassignParentEdge(cur->next->parentEdge, cur);
            cur->parent = cur->next;
            stCactusTree *next = cur->next;
            cur->prev = NULL;
            cur->next = next->firstChild;
            if (cur->next != NULL) {
                cur->next->prev = cur;
            }
            next->firstChild = cur;
            cur = next;
        }
        reassignParentEdge(chain->parentEdge, cur);
        cur->parent = chainParent;
        stCactusTree_destruct(chain);
        cur->prev = NULL;
        cur->next = cur->parent->firstChild;
        if (cur->next != NULL) {
            cur->next->prev = cur;
        }
        cur->parent->firstChild = cur;
    }
}

// Nets in the chain, starting from the parent.
stList *chainToList(stCactusTree *chain) {
    stList *chainNodes = stList_construct();
    stList_append(chainNodes, chain->parent);
    stCactusTree *curChild = chain->firstChild;
    while (curChild != NULL) {
        stList_append(chainNodes, curChild);
        curChild = curChild->next;
    }
    return chainNodes;
}

static void fix3EC(stOnlineCactus *cactus, stCactusTree *tree);

static stSet *getEndsInNodeSet(stOnlineCactus *cactus, stSet *nodes) {
    stSet *ret = stSet_construct();
    stSetIterator *nodeIt = stSet_getIterator(nodes);
    void *node;
    while ((node = stSet_getNext(nodeIt)) != NULL) {
        stSet *ends = stHash_search(cactus->nodeToEnds, node);
        stSetIterator *endIt = stSet_getIterator(ends);
        void *end;
        while ((end = stSet_getNext(endIt)) != NULL) {
            stSet_insert(ret, end);
        }
        stSet_destructIterator(endIt);
    }
    stSet_destructIterator(nodeIt);
    return ret;
}

// Cleave a net into two, with nodes in endsToRemove moving to a new
// node. If the two nets would be 3-edge-connected after partition,
// does nothing.
// Does not check the 3-edge-connectivity of the resulting net graphs.
static void netCleave(stOnlineCactus *cactus, stCactusTree *tree, stSet *nodesToRemove, stCactusTree **remainingTree, stCactusTree **partitionedTree) {
    stSet *nodes1 = stSet_getDifference(tree->nodes, nodesToRemove);
    stSet *nodes2 = nodesToRemove;
    if (stSet_size(nodes1) == 0 || stSet_size(nodes2) == 0) {
        // Nothing to partition.
        if (remainingTree != NULL) {
            *remainingTree = tree;
        }
        if (partitionedTree != NULL) {
            *partitionedTree = NULL;
        }
        stSet_destruct(nodes1);
        stSet_destruct(nodes2);
        return;
    }
    // TODO: This may be a bottleneck. Rather than collecting all the
    // ends here and querying these sets, we could instead query
    // endToNode and the node set, which will end up being much faster
    // on very large nets.
    stSet *ends1 = getEndsInNodeSet(cactus, nodes1);
    stSet *ends2 = getEndsInNodeSet(cactus, nodes2);

    bool tree2IsPartitionedTree = true;
    // Without loss of generality we call ends1 the partition that contains the parent edge.
    if (tree->parent && stSet_search(ends2, getBlockEndCorrespondingToNet(cactus, tree->parentEdge->block, tree))) {
        stSet *tmp = ends1;
        ends1 = ends2;
        ends2 = tmp;
        tmp = nodes1;
        nodes1 = nodes2;
        nodes2 = tmp;
        tree2IsPartitionedTree = false;
    }

    // The chains with ends in different partitions.
    stList *connectingChains = stList_construct();
    // Keep a list of the children that wholly belong to the new node "tree2".
    stList *childrenToMove = stList_construct();
    // Parent chain
    if (tree->parent != NULL && stCactusTree_type(tree->parent) == CHAIN) {
        void *parentEnd, *neighborEnd;
        getEndsInParentChainAffectingChildNode(cactus, tree, &parentEnd, &neighborEnd);
        if ((stSet_search(ends1, parentEnd) != NULL) != (stSet_search(ends1, neighborEnd) != NULL)) {
            stList_append(connectingChains, tree->parent);
        }
    }
    // Child chains
    stCactusTree *curChild = tree->firstChild;
    while (curChild != NULL) {
        if (stCactusTree_type(curChild) == CHAIN) {
            void *end1, *end2;
            getEndsInChildChainAffectingParentNode(cactus, tree, curChild, &end1, &end2);
            // These sets form a partition of the ends, so we can reduce
            // the case that we're interested in (two ends in different
            // partitions) to exactly two set searches.
            if ((stSet_search(ends1, end1) != NULL) != (stSet_search(ends1, end2) != NULL)) {
                // Found a chain with ends in different partitions.
                stList_append(connectingChains, curChild);
                if (stList_length(connectingChains) > 2) {
                    // More than 2 connecting chains, so the 2 partitions are 3EC.
                    stList_destruct(childrenToMove);
                    stList_destruct(connectingChains);
                    stSet_destruct(ends1);
                    stSet_destruct(ends2);
                    if (remainingTree != NULL) {
                        *remainingTree = tree;
                    }
                    if (partitionedTree != NULL) {
                        *partitionedTree = NULL;
                    }
                    return;
                }
            } else if (stSet_search(ends2, end1)) {
                // This chain wholly belongs to the new tree.
                stList_append(childrenToMove, curChild);
            }
        } else {
            // Bridge edge--just check which partition the block end belongs to.
            void *end = getBlockEndCorrespondingToNet(cactus, curChild->parentEdge->block, tree);
            if (stSet_search(ends2, end)) {
                stList_append(childrenToMove, curChild);
            }
        }
        curChild = curChild->next;
    }

    // If we're here then there are 2 or fewer connecting chains.
    assert(stList_length(connectingChains) <= 2);

    // Split the nodes.
    stCactusTree *tree2 = stCactusTree_construct(NULL, NULL, NET, NULL);

    // Partition the bridges and whole chains.
    for (int64_t i = 0; i < stList_length(childrenToMove); i++) {
        stCactusTree *child = stList_get(childrenToMove, i);
        stCactusTree_removeChild(tree, child);
        stCactusTreeEdge *edge = child->parentEdge;
        stCactusTree_prependChild(tree2, child, NULL);
        free(child->parentEdge);
        child->parentEdge = edge;
    }

    // We may alter the 3-edge-connectivity of some nodes as a side
    // effect of unraveling a chain. Note that we never check the
    // 3-edge-connectivity of the partitioned nodes, even though they
    // are part of the unraveled chain. That's the caller's job--we
    // only handle the side effects on other nodes.
    stList *nodesToCheck3ECOf = stList_construct();
    if (stList_length(connectingChains) == 1) {
        stCactusTree *chain = stList_get(connectingChains, 0);

        // Save the nodes in the chain so we can check 3EC later.
        stList *otherNodesInChain = chainToList(chain);
        stList_removeItem(otherNodesInChain, tree);

        // Unravel the connecting chain by removing the edge in ends2.
        void *end1, *end2;
        if (chain == tree->parent) {
            getEndsInParentChainAffectingChildNode(cactus, tree, &end1, &end2);
        } else {
            assert(chain->parent == tree);
            getEndsInChildChainAffectingParentNode(cactus, tree, chain, &end1, &end2);
        }
        void *end; // the end that belongs to tree2
        if (stSet_search(ends2, end1)) {
            end = end1;
        } else {
            end = end2;
        }
        void *block = cactus->endToEdge(end);
        stCactusTreeEdge *edge = stHash_search(cactus->blockToEdge, block);
        assert(edge != NULL);
        unravelChain(cactus, edge);
        // Add tree2 as a child of the newly unraveled path.
        void *otherEnd = end == cactus->edgeToEnd(block, 0) ? cactus->edgeToEnd(block, 1) : cactus->edgeToEnd(block, 0);
        void *otherEndNode = stHash_search(cactus->endToNode, otherEnd);
        stCactusTree *attachmentPoint = stHash_search(cactus->nodeToNet, otherEndNode);
        stCactusTree_prependChild(attachmentPoint, tree2, block);
        stHash_insert(cactus->blockToEdge, block, tree2->parentEdge);

        // We may have changed the 3EC of the unraveled nodes.
        stList_appendAll(nodesToCheck3ECOf, otherNodesInChain);
        stList_destruct(otherNodesInChain);
    } else if (stList_length(connectingChains) == 2) {
        // Create a new chain from the connecting chains.
        stCactusTree *chain1 = stList_get(connectingChains, 0);
        stCactusTree *chain2 = stList_get(connectingChains, 1);

        // TODO: this isn't necessary but eliminates 1 special case in
        // a path that already has several. Check if it slows things
        // down appreciably.
        stCactusTree_reroot(tree, cactus->trees);
        assert(chain1->parent == tree);
        assert(chain2->parent == tree);
        stList *chainNodes1 = chainToList(chain1);
        stList *chainNodes2 = chainToList(chain2);
        stList_remove(chainNodes1, 0);
        stList_remove(chainNodes2, 0);
        // Arrange the chains so chain 1 starts at "tree" and ends at
        // "tree2", and chain 2 starts at "tree2" and ends at "tree".

        // chain 1
        void *end1, *end2;
        getEndsInChildChainAffectingParentNode(cactus, tree, chain1, &end1, &end2);
        if (stSet_search(ends1, end1)) {
            // parent edge (back-edge) is in ends1.
            stList *parentEdges = stList_construct();
            for (int64_t i = 0; i < stList_length(chainNodes1); i++) {
                stCactusTree *node = stList_get(chainNodes1, i);
                stList_append(parentEdges, node->parentEdge);
                node->parentEdge->child = NULL;
            }
            stList_append(parentEdges, chain1->parentEdge);
            stList_reverse(chainNodes1);
            stList_reverse(parentEdges);
            for (int64_t i = 0; i < stList_length(chainNodes1); i++) {
                stCactusTree *node = stList_get(chainNodes1, i);
                reassignParentEdge(stList_get(parentEdges, i), node);
            }
            reassignParentEdge(stList_get(parentEdges, stList_length(parentEdges) - 1), chain1);
            stList_destruct(parentEdges);
        } else {
            // first child edge is in ends1.
        }

        // chain 2
        getEndsInChildChainAffectingParentNode(cactus, tree, chain2, &end1, &end2);
        if (stSet_search(ends1, end1)) {
            // parent edge (back-edge) is in ends1.
        } else {
            // first child edge is in ends1.
            stList *parentEdges = stList_construct();
            for (int64_t i = 0; i < stList_length(chainNodes2); i++) {
                stCactusTree *node = stList_get(chainNodes2, i);
                stList_append(parentEdges, node->parentEdge);
                node->parentEdge->child = NULL;
            }
            stList_append(parentEdges, chain2->parentEdge);
            stList_reverse(chainNodes2);
            stList_reverse(parentEdges);
            for (int64_t i = 0; i < stList_length(chainNodes2); i++) {
                stCactusTree *node = stList_get(chainNodes2, i);
                reassignParentEdge(stList_get(parentEdges, i), node);
            }
            reassignParentEdge(stList_get(parentEdges, stList_length(parentEdges) - 1), chain2);
            stList_destruct(parentEdges);
        }

        // Now join the two chains, in the current order, into one big
        // happy chain as a child of "tree". The order should go:
        // chain1, tree2, chain2.

        stCactusTree *chain = stCactusTree_construct(tree, NULL, CHAIN, NULL);
        chain->firstChild = stList_length(chainNodes1) > 0 ? stList_get(chainNodes1, 0) : tree2;
        stCactusTree *prev = NULL;
        for (int64_t i = 0; i < stList_length(chainNodes1); i++) {
            stCactusTree *node = stList_get(chainNodes1, i);
            node->parent = chain;
            node->prev = prev;
            if (prev != NULL) {
                prev->next = node;
            }
            prev = node;
        }
        tree2->parent = chain;
        tree2->prev = prev;
        if (prev != NULL) {
            prev->next = tree2;
        }
        prev = tree2;
        tree2->next = NULL;
        reassignParentEdge(chain1->parentEdge, tree2);
        for (int64_t i = 0; i < stList_length(chainNodes2); i++) {
            stCactusTree *node = stList_get(chainNodes2, i);
            node->parent = chain;
            node->prev = prev;
            prev->next = node;
            node->next = NULL;
            prev = node;
        }
        reassignParentEdge(chain2->parentEdge, chain);
        stCactusTree_removeChild(tree, chain1);
        stCactusTree_removeChild(tree, chain2);
        chain1->firstChild = NULL;
        chain2->firstChild = NULL;
        stCactusTree_destruct(chain1);
        stCactusTree_destruct(chain2);
        stList_destruct(chainNodes1);
        stList_destruct(chainNodes2);
    } else {
        // Created a new tree.
        assert(!stList_contains(cactus->trees, stCactusTree_root(tree2)));
        stList_append(cactus->trees, stCactusTree_root(tree2));
    }

    stSet_destruct(ends1);
    stSet_destruct(ends2);

    // Split the nodes.
    stSet_destruct(tree2->nodes);
    tree2->nodes = nodes2;
    // Remap the nodes.
    stSetIterator *it = stSet_getIterator(tree2->nodes);
    void *node;
    while ((node = stSet_getNext(it)) != NULL) {
        stHash_insert(cactus->nodeToNet, node, tree2);
    }
    stSet_destructIterator(it);

    stSet_destruct(tree->nodes);
    tree->nodes = nodes1;

    stList_destruct(connectingChains);
    stList_destruct(childrenToMove);

    // If we unraveled a chain, we need to check the 3EC of the
    // affected nodes, as we've destroyed a potential source of
    // connectivity.
    for (int64_t i = 0; i < stList_length(nodesToCheck3ECOf); i++) {
        stCactusTree *cur = stList_get(nodesToCheck3ECOf, i);
        fix3EC(cactus, cur);
    }
    stList_destruct(nodesToCheck3ECOf);

    // Return whatever tree doesn't have the partitioned ends.
    if (tree2IsPartitionedTree) {
        if (remainingTree != NULL) {
            *remainingTree = tree;
        }
        if (partitionedTree != NULL) {
            *partitionedTree = tree2;
        }
    } else {
        if (remainingTree != NULL) {
            *remainingTree = tree2;
        }
        if (partitionedTree != NULL) {
            *partitionedTree = tree;
        }
    }
}

void stOnlineCactus_nodeCleave(stOnlineCactus *cactus, void *node, void *newNode, stSet *endsToRemove) {
    stCactusTree *tree = stHash_search(cactus->nodeToNet, node);

    // First things first, partition the ends belonging to the base graph node.
    stSet *oldEnds = stHash_search(cactus->nodeToEnds, node);
    assert(oldEnds != NULL);
    stSet *removedEnds = stSet_getIntersection(oldEnds, endsToRemove);
    stSet *keptEnds = stSet_getDifference(oldEnds, removedEnds); // The ends that stay in the old node.
    stSet_destruct(oldEnds);
    stHash_insert(cactus->nodeToEnds, node, keptEnds);
    // Create the new base node and stick it into the net.
    stHash_insert(cactus->nodeToEnds, newNode, removedEnds);
    stSetIterator *endIt = stSet_getIterator(removedEnds);
    void *end;
    while ((end = stSet_getNext(endIt)) != NULL) {
        mapEndToNode(cactus, end, newNode);
    }
    stSet_destructIterator(endIt);
    stSet_insert(tree->nodes, newNode);
    stHash_insert(cactus->nodeToNet, newNode, tree);

    // Finally, check to see if this requires a cleave in the net to
    // maintain 3-edge-connectivity.
    fix3EC(cactus, tree);
}

static stCactusTree *stCactusTree_getMRCA(stCactusTree *node1, stCactusTree *node2) {
    stSet *node1Anc = stSet_construct();
    while (node1 != NULL) {
        stSet_insert(node1Anc, node1);
        node1 = node1->parent;
    }
    stCactusTree *ret = NULL;
    while (node2 != NULL) {
        if (stSet_search(node1Anc, node2) != NULL) {
            ret = node2;
            break;
        }
        node2 = node2->parent;
    }
    stSet_destruct(node1Anc);
    return ret;
}

stCactusTreeEdge *stOnlineCactus_getEdge(stOnlineCactus *cactus, void *block) {
    return stHash_search(cactus->blockToEdge, block);
}

// NB: ordering of nodes in the input list MUST reflect the ordering in their parent chain.
static void moveToNewChain(stList *nodes, stCactusTreeEdge *chainParentEdge,
                           stCactusTree *netToAttachNewChainTo) {
    stCactusTree *chain = stCactusTree_construct(netToAttachNewChainTo, NULL, CHAIN, NULL);
    reassignParentEdge(chainParentEdge, chain);
    stCactusTree *prev = NULL;
    for (int64_t i = 0; i < stList_length(nodes); i++) {
        stCactusTree *node = stList_get(nodes, i);
        stCactusTree *next = node->next;
        stCactusTree_removeChild(node->parent, node);
        if (i == 0) {
            chain->firstChild = node;
        }
        node->prev = prev;
        if (i < stList_length(nodes) - 1) {
            node->next = next;
        }
        node->parent = chain;
        prev = node;
    }
}

// Merge net1 "into" net2, i.e. net1 will be deleted while net2
// will remain with all of net1's children.
static void mergeNets(stOnlineCactus *cactus, stCactusTree *net1, stCactusTree *net2) {
    stCactusTree *first = net1->firstChild;
    stCactusTree *cur = first;
    net1->firstChild = NULL;
    while (cur != NULL) {
        cur->parent = net2;
        if (cur->next == NULL) {
            cur->next = net2->firstChild;
            if (net2->firstChild != NULL) {
                cur->next->prev = cur;
            }
            break;
        }
        cur = cur->next;
    }
    if (first != NULL) {
        net2->firstChild = first;
    }

    // Remap every node of net1 to point to net2.
    stSetIterator *nodeIt = stSet_getIterator(net1->nodes);
    void *node;
    while ((node = stSet_getNext(nodeIt)) != NULL) {
        stHash_insert(cactus->nodeToNet, node, net2);
        stSet_insert(net2->nodes, node);
    }
    stSet_destructIterator(nodeIt);
    stCactusTree_destruct(net1);
}

static stCactusTree *collapse3ECNetsBetweenChain(stOnlineCactus *cactus, stCactusTree *net1, stCactusTree *net2) {
    stCactusTree *chain = net1->parent;
    if (chain == net2->parent) {
        // Create lists of the nodes between net1 and net2 in the chain ordering.
        stList *between = stList_construct();

        // Figure out what direction net2 is in relation to net1.
        // TODO: this is pretty slow, and there is probably a more clever way of doing this
        stCactusTree *cur = net1;
        while (cur != NULL) {
            if (cur == net2) {
                break;
            }
            cur = cur->next;
        }
        if (cur == NULL) {
            // swap net1 and net2 to ensure that we always move right
            stCactusTree *tmp = net1;
            net1 = net2;
            net2 = tmp;
        }

        cur = net1->next;
        while (cur != net2) {
            stList_append(between, cur);
            cur = cur->next;
        }
        stCactusTree_removeChild(net2->parent, net2);
        moveToNewChain(between, net2->parentEdge, net1);
        stList_destruct(between);
        mergeNets(cactus, net2, net1);
        return net1;
    } else {
        // Create lists of the nodes before and after net1 in the chain ordering.
        stList *before = stList_construct();
        stCactusTree *cur = net1->prev;
        while (cur != NULL) {
            stList_append(before, cur);
            cur = cur->prev;
        }
        stList_reverse(before);
        stList *after = stList_construct();
        cur = net1->next;
        while (cur != NULL) {
            stList_append(after, cur);
            cur = cur->next;
        }

        // Detach all nodes in these lists from their parent and
        // create a new chain node for each. Their ordering must
        // be preserved.
        moveToNewChain(before, net1->parentEdge, net2);
        moveToNewChain(after, chain->parentEdge, net2);

        stList_destruct(before);
        stList_destruct(after);
        assert(chain->firstChild == net1);
        assert(net1->next == NULL);
        assert(net1->prev == NULL);

        // Merge the two nets and remove the now isolated chain node.
        mergeNets(cactus, net1, net2);
        stCactusTree_destruct(chain);
        return net2;
    }
}

void collapse3ECNets(stOnlineCactus *cactus,
                     stCactusTree *net1, stCactusTree *net2,
                     stCactusTree **newNet1, stCactusTree **newNet2) {
    stCactusTree *mrca = stCactusTree_getMRCA(net1, net2);
    // Net1 -> mrca path
    stCactusTree *cur1 = net1;
    while (cur1 != mrca && cur1->parent != NULL && cur1->parent != mrca) {
        if (stCactusTree_type(cur1->parent) == CHAIN) {
            stCactusTree *newNode = collapse3ECNetsBetweenChain(cactus, cur1, cur1->parent->parent);
            if (cur1 == net1) {
                net1 = newNode;
            }
            cur1 = newNode;
        } else {
            // parent is a bridge
            cur1 = cur1->parent;
        }
    }

    // Re-find the mrca in case the mrca is no longer a valid pointer.
    mrca = stCactusTree_getMRCA(net1, net2);

    // Net2->mrca path
    stCactusTree *cur2 = net2;
    while (cur2 != mrca && cur2->parent != NULL && cur2->parent != mrca) {
        if (stCactusTree_type(cur2->parent) == CHAIN) {
            stCactusTree *newNode = collapse3ECNetsBetweenChain(cactus, cur2, cur2->parent->parent);
            if (cur2 == net2) {
                net2 = newNode;
            }
            cur2 = newNode;
        } else {
            // parent is a bridge
            cur2 = cur2->parent;
        }
    }

    // Re-find the mrca in case the mrca is no longer a valid pointer.
    mrca = stCactusTree_getMRCA(net1, net2);

    // check the path between the two now in case the mrca is a chain.
    if (stCactusTree_type(mrca) == CHAIN) {
        stCactusTree *newNode;
        newNode = collapse3ECNetsBetweenChain(cactus, cur1, cur2);
        if (net1 == cur1) {
            net1 = cur1 = newNode;
        }
        if (net2 == cur2) {
            net2 = cur2 = newNode;
        }
    }
    *newNet1 = net1;
    *newNet2 = net2;
}

// pathUp is "left" of pathDown. extraEdge connects the first member
// of pathUp to the last of pathDown. if pathUp is empty, then the
// edge is from the last member of pathDown to the mrca.
static void pathToChain(stList *pathUp, stList *pathDown, stCactusTree *mrca, stCactusTreeEdge *extraEdge) {
    stCactusTree *chain = stCactusTree_construct(mrca, NULL, CHAIN, NULL);
    // Assign the correct parent edge to the new chain node.
    if (stList_length(pathUp) > 0) {
        reassignParentEdge(((stCactusTree *) stList_get(pathUp, stList_length(pathUp) - 1))->parentEdge, chain);
    } else {
        reassignParentEdge(extraEdge, chain);
    }
    chain->firstChild = stList_get(pathDown, 0);
    for (int64_t i = 0; i < stList_length(pathDown); i++) {
        stCactusTree *tree = stList_get(pathDown, i);
        stCactusTree_removeChild(tree->parent, tree);
        if (i != stList_length(pathDown) - 1) {
            tree->next = stList_get(pathDown, i + 1);
        }
        tree->parent = chain;
        if (i > 0) {
            tree->prev = stList_get(pathDown, i - 1);
        } else {
            tree->prev = NULL;
        }
    }

    stCactusTreeEdge *prevEdge = NULL;
    for (int64_t i = 0; i < stList_length(pathUp); i++) {
        stCactusTree *tree = stList_get(pathUp, i);
        stCactusTree_removeChild(tree->parent, tree);
        tree->parent = chain;
        if (i < stList_length(pathUp) - 1) {
            tree->next = stList_get(pathUp, i + 1);
        } else {
            tree->next = NULL;
        }
        if (i > 0) {
            tree->prev = stList_get(pathUp, i - 1);
            stCactusTreeEdge *tmp = tree->parentEdge;
            reassignParentEdge(prevEdge, tree);
            prevEdge = tmp;
        } else {
            tree->prev = stList_get(pathDown, stList_length(pathDown) - 1);
            tree->prev->next = tree;
            prevEdge = tree->parentEdge;
            reassignParentEdge(extraEdge, tree);
        }
        if (prevEdge != NULL) {
            prevEdge->child = NULL;
        }
    }
}

void stCactusTree_reroot(stCactusTree *newRoot, stList *trees) {
    stCactusTree *root = stCactusTree_root(newRoot);
    if (root == newRoot) {
        // Already done.
        return;
    }

    stList_removeItem(trees, root);
    stList_append(trees, newRoot);

    stList *parentEdges = stList_construct();
    stList *ancestors = stList_construct();
    // cutPoints[i] is the *first* node in the list of nodes after the
    // "flipped" node ancestors[i] in the child ordering. The list of
    // nodes including cutPoints[i] and after, plus ancestors[i], gets
    // moved to the *beginning* of the child ordering for a proper
    // reroot. (This way, all nodes can keep their original parent
    // edge, except the flipped nodes.)
    stList *cutPoints = stList_construct();
    stCactusTree *cur = newRoot;
    while (cur->parent != NULL) {
        stCactusTree *parent = cur->parent;
        stList_append(ancestors, parent);
        stList_append(parentEdges, cur->parentEdge);
        cur->parentEdge = NULL;
        stList_append(cutPoints, cur->next);
        stCactusTree_removeChild(parent, cur);
        cur = parent;
    }

    assert(stList_length(parentEdges) == stList_length(ancestors));
    assert(stList_length(ancestors) == stList_length(cutPoints));
    stCactusTree *newParent = newRoot;
    for (int64_t i = 0; i < stList_length(ancestors); i++) {
        stCactusTree *ancestor = stList_get(ancestors, i);
        stCactusTree *cutPoint = i == 0 ? NULL : stList_get(cutPoints, i - 1);
        stCactusTreeEdge *edge = stList_get(parentEdges, i);
        assert(ancestor->next == NULL);
        assert(ancestor->prev == NULL);
        assert(ancestor->parent == NULL);
        assert(ancestor->parentEdge == NULL);
        ancestor->parentEdge = edge;
        edge->child = ancestor;
        ancestor->parent = newParent;
        if (cutPoint != NULL) {
            assert(cutPoint->parent == newParent);
            if (newParent->firstChild == cutPoint) {
                newParent->firstChild = NULL;
            }
            if (cutPoint->prev != NULL) {
                cutPoint->prev->next = NULL;
            }
            cutPoint->prev = NULL;
            cur = cutPoint;
            while (cur->next != NULL) {
                cur = cur->next;
            }
            cur->next = ancestor;
            ancestor->prev = cur;
        }
        ancestor->next = newParent->firstChild;
        if (newParent->firstChild != NULL) {
            ancestor->next->prev = ancestor;
        }
        newParent->firstChild = (cutPoint == NULL) ? ancestor : cutPoint;
        newParent = ancestor;
    }
    stList_destruct(parentEdges);
    stList_destruct(ancestors);
    stList_destruct(cutPoints);
}

// Returns true if the node1->mrca path is always "left" of the node2->mrca path.
static bool stCactusTree_leftOf(stCactusTree *node1, stCactusTree *node2) {
    stCactusTree *mrca = stCactusTree_getMRCA(node1, node2);
    if (node1 == mrca || node2 == mrca) {
        return false;
    }
    while (node1->parent != mrca) {
        node1 = node1->parent;
    }
    while (node2->parent != mrca) {
        node2 = node2->parent;
    }
    while (node1 != NULL) {
        if (node1 == node2) {
            return true;
        }
        node1 = node1->next;
    }
    return false;
}

static void createChainOnPathConnectingNets(stCactusTree *node1, stCactusTree *node2, void *block,
                                            stHash *blockToEdge) {
    // Find the path between node1 and node2.
    stCactusTree *mrca = stCactusTree_getMRCA(node1, node2);
    if (node2 == mrca) {
        // We assume node2 is below node1.
        stCactusTree *tmp = node2;
        node2 = node1;
        node1 = tmp;
    }
    if (node1 != mrca && !stCactusTree_leftOf(node1, node2)) {
        // pathUp must be left of pathDown.
        stCactusTree *tmp = node2;
        node2 = node1;
        node1 = tmp;
    }

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

    // node1->node2 edge.
    stCactusTreeEdge *extraEdge = calloc(1, sizeof(stCactusTreeEdge));
    extraEdge->block = block;
    if (block != NULL) {
        stHash_insert(blockToEdge, block, extraEdge);
    }

    // Create a new chain.
    pathToChain(pathUp, pathDown, mrca, extraEdge);
    stList_destruct(pathUp);
    stList_destruct(pathDown);    
}

// Return true if a is ancestral to b (or equal to b), and false otherwise.
static bool stCactusTree_isAncestralTo(stCactusTree *a, stCactusTree *b) {
    while (b != NULL) {
        if (b == a) {
            return true;
        }
        b = b->parent;
    }
    return false;
}

void stOnlineCactus_nodeMerge(stOnlineCactus *cactus, void *node1, void *node2) {
    stCactusTree *net1 = stHash_search(cactus->nodeToNet, node1);
    assert(net1 != NULL);
    stCactusTree *net2 = stHash_search(cactus->nodeToNet, node2);
    assert(net2 != NULL);

    // Remap all ends belonging to node1 to node2, and delete node1.
    stSet *node1Ends = stHash_search(cactus->nodeToEnds, node1);
    stSetIterator *endIt = stSet_getIterator(node1Ends);
    void *end;
    while ((end = stSet_getNext(endIt)) != NULL) {
        mapEndToNode(cactus, end, node2);
    }
    stSet_destructIterator(endIt);
    stSet_remove(net1->nodes, node1);
    stSet_destruct(stHash_remove(cactus->nodeToEnds, node1));
    stHash_remove(cactus->nodeToNet, node1);

    // Now update the cactus forest.
    if (net1 == net2) {
        return;
    } else if (stCactusTree_root(net1) != stCactusTree_root(net2)) {
        stCactusTree_reroot(net1, cactus->trees);
        // We can do a trivial merge since the two nodes are not in the same tree.
        mergeNets(cactus, net1, net2);
        stList_removeItem(cactus->trees, net1);
    } else {
        // They're in the same tree so we have to do the tricky bit.
        collapse3ECNets(cactus, net1, net2, &net1, &net2);
        if (net1 == net2) {
            // All done.
            return;
        }
        createChainOnPathConnectingNets(net1, net2, NULL, cactus->blockToEdge);
        if (stCactusTree_isAncestralTo(net1, net2) || stCactusTree_leftOf(net1, net2)) {
            // We always merge net1 into net2, keeping net2's
            // original pointer. So for simplicity, we assume that
            // net2 is either an ancestor of net1 or "left" of net1
            // in the child ordering. This simplifies the logic behind
            // placing the extra, temporary net1->net2 chain edge.
            stCactusTree *tmp = net2;
            net2 = net1;
            net1 = tmp;
        }
        if (stCactusTree_isAncestralTo(net2, net1)) {
            // While this looks a bit weird, this is a simple swap of
            // the chain ordering so that the "real" net1->chain
            // edge, instead of the "fake" net1->net2 edge, is the
            // back-edge of the new chain. We want to end up losing
            // the fake edge during the merge.
            stCactusTree *newChain = net1->parent;
            free(newChain->parentEdge);
            reassignParentEdge(net1->parentEdge, newChain);
        }
        // Contract the nodes.
        mergeNets(cactus, net1, net2);
    }
}

void stOnlineCactus_addEdge(stOnlineCactus *cactus,
                            void *node1, void *node2,
                            void *end1, void *end2,
                            void *edge) {
    stCactusTree *net1 = stHash_search(cactus->nodeToNet, node1);
    assert(net1 != NULL);
    stCactusTree *net2 = stHash_search(cactus->nodeToNet, node2);
    assert(net2 != NULL);

    // Add the ends to their proper nodes.
    mapEndToNode(cactus, end1, node1);
    mapEndToNode(cactus, end2, node2);

    assert(stHash_search(cactus->blockToEdge, edge) == NULL);

    // Are net1 and net2 in the same tree?
    if (stCactusTree_root(net1) == stCactusTree_root(net2)) {
        collapse3ECNets(cactus, net1, net2, &net1, &net2);
        if (net1 == net2) {
            // Simplest case -- the two nodes are identical and we
            // need to add a self-loop chain.
            stCactusTree *chain = stCactusTree_construct(net1, NULL, CHAIN, edge);
            stHash_insert(cactus->blockToEdge, edge, chain->parentEdge);
        } else {
            // Guaranteed to create a chain connecting these two nets.
            createChainOnPathConnectingNets(net1, net2, edge, cactus->blockToEdge);
        }
    } else {
        // Simply add net2 as a child of net1.
        assert(stList_contains(cactus->trees, stCactusTree_root(net1)));
        stCactusTree_reroot(net2, cactus->trees);
        stList_removeItem(cactus->trees, stCactusTree_root(net2));
        stCactusTree_prependChild(net1, net2, edge);
        assert(stList_contains(cactus->trees, stCactusTree_root(net2)));
        stHash_insert(cactus->blockToEdge, edge, net2->parentEdge);
    }
}

// Fix a net that might no longer be 3-edge-connected.
static void fix3EC(stOnlineCactus *cactus, stCactusTree *tree) {
    stHash *nodeToIndex;
    stList *components = get3ECComponents(cactus, tree, &nodeToIndex);
    if (stList_length(components) != 1) {
        stHash *indexToNode = stHash_invert(nodeToIndex,
                                                    (uint64_t (*)(const void *)) stIntTuple_hashKey,
                                                    (int (*)(const void *, const void *)) stIntTuple_equalsFn,
                                                    NULL,
                                                    NULL);
        for (int64_t i = 0; i < stList_length(components) - 1; i++) {
            stList *component = stList_get(components, i);
            stSet *nodesToRemove = stSet_construct();
            for (int64_t j = 0; j < stList_length(component); j++) {
                stIntTuple *index = stList_get(component, j);
                void *node = stHash_search(indexToNode, index);
                if (stSet_search(tree->nodes, node)) {
                    stSet_insert(nodesToRemove, node);
                }
            }
            stCactusTree *tree2;
            netCleave(cactus, tree, nodesToRemove, &tree, &tree2);
            assert(tree2 != NULL); // If this is a separate 3EC component, the cleave must succeed!!
        }
        stHash_destruct(indexToNode);
    }
    stList_destruct(components);
    stHash_destruct(nodeToIndex);
}

void stOnlineCactus_deleteEdge(stOnlineCactus *cactus, void *end1, void *end2, void *block) {
    void *node1 = stHash_search(cactus->endToNode, end1);
    void *node2 = stHash_search(cactus->endToNode, end2);

    stCactusTree *net1 = stHash_search(cactus->nodeToNet, node1);
    assert(net1 != NULL);
    stCactusTree *net2 = stHash_search(cactus->nodeToNet, node2);
    assert(net2 != NULL);
    assert(stCactusTree_root(net1) == stCactusTree_root(net2));
    unmapEndToNode(cactus, end1, node1);
    unmapEndToNode(cactus, end2, node2);
    stCactusTreeEdge *edge = stHash_search(cactus->blockToEdge, block);
    if (net1->parent == net2 || net2->parent == net1) {
        // a bridge edge
        assert(stCactusTree_type(net1) == NET);
        assert(stCactusTree_type(net2) == NET);
        stCactusTree *child;
        stCactusTree *parent;
        if (net1->parent == net2) {
            child = net1;
            parent = net2;
        } else {
            child = net2;
            parent = net1;
        }
        stList_append(cactus->trees, child);
        (void) edge;
        assert(edge->child == child);
        stHash_remove(cactus->blockToEdge, block);
        free(child->parentEdge);
        child->parentEdge = NULL;
        stCactusTree_removeChild(parent, child);
    } else if (net1 == net2) {
        // endpoints are in the same net, we need to delete a chain
        stCactusTree *chain = edge->child;
        assert(stCactusTree_type(chain) == CHAIN);
        assert(chain->firstChild == NULL);
        stCactusTree_removeChild(net1, chain);
        stHash_remove(cactus->blockToEdge, chain->parentEdge->block);
        stCactusTree_destruct(chain);
        fix3EC(cactus, net1);
    } else {
        // We save the chain nodes for checking 3EC later on.
        stCactusTree *chain;
        if (stCactusTree_type(edge->child) == CHAIN) {
            chain = edge->child;
        } else {
            chain = edge->child->parent;
        }
        stList *chainNodes = chainToList(chain);
        // part of a chain. Delete the relevant edge and unravel the chain from either end.
        unravelChain(cactus, edge);

        for (int64_t i = 0; i < stList_length(chainNodes); i++) {
            stCactusTree *node = stList_get(chainNodes, i);
            fix3EC(cactus, node);
        }
        stList_destruct(chainNodes);
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

int64_t scoreBlocks(stList *blocks, int64_t scoreFn(void *)) {
    int64_t accum = 0;
    for (int64_t i = 0; i < stList_length(blocks); i++) {
        accum += scoreFn(stList_get(blocks, i));
    }
    return accum;
}

stList *getBlocksFromBestBridgePathBelow(stCactusTree *node, stCactusTree *except, int64_t scoreFn(void *)) {
    stCactusTree *child = node->firstChild;
    stList *blockss = stList_construct3(0, (void (*)(void *)) stList_destruct);
    while (child != NULL) {
        if (stCactusTree_type(child) == NET && child != except) {
            stList *bestPath = getBlocksFromBestBridgePathBelow(child, NULL, scoreFn);
            stList_append(bestPath, child->parentEdge->block);
            stList_append(blockss, bestPath);
        }
        child = child->next;
    }
    stList *bestBlocks = NULL;
    int64_t bestScore = INT64_MIN;
    // Find the best path.
    for (int64_t i = 0; i < stList_length(blockss); i++) {
        int64_t score = scoreBlocks(stList_get(blockss, i), scoreFn);
        if (score > bestScore) {
            bestBlocks = stList_get(blockss, i);
            bestScore = score;
        }
    }
    stList_removeItem(blockss, bestBlocks);
    stList_destruct(blockss);
    if (bestBlocks == NULL) {
        bestBlocks = stList_construct();
    }
    return bestBlocks;
}

stList *getBlocksFromBestBridgePathAbove(stCactusTree *node, stCactusTree *prev, int64_t scoreFn(void *)) {
    stList *abovePath;
    if (node->parent != NULL && stCactusTree_type(node->parent) != CHAIN) {
        abovePath = getBlocksFromBestBridgePathAbove(node->parent, node, scoreFn);
        stList_append(abovePath, node->parentEdge->block);
    } else {
        abovePath = stList_construct();
    }
    stList *belowPath = getBlocksFromBestBridgePathBelow(node, prev, scoreFn);
    if (scoreBlocks(abovePath, scoreFn) > scoreBlocks(belowPath, scoreFn)) {
        stList_destruct(belowPath);
        return abovePath;
    } else {
        stList_destruct(abovePath);
        return belowPath;
    }
}

stList *stOnlineCactus_getMaximalChainOrBridgePath(stOnlineCactus *cactus, void *block, int64_t scoreFn(void *)) {
    stCactusTreeEdge *edge = stHash_search(cactus->blockToEdge, block);
    if (stCactusTree_type(edge->child) == CHAIN || stCactusTree_type(edge->child->parent) == CHAIN) {
        // return the only chain path.
        stCactusTree *chain;
        if (stCactusTree_type(edge->child) == CHAIN) {
            chain = edge->child;
        } else {
            chain = edge->child->parent;
        }
        stList *path = chainToList(chain);
        stList *blocks = stList_construct();
        // Ignore the parent node of the chain, but get every other member's parent edges.
        for (int64_t i = 1; i < stList_length(path); i++) {
            stCactusTree *node = stList_get(path, i);
            stList_append(blocks, node->parentEdge->block);
        }
        stList_destruct(path);

        // Finally, append the parent edge of the chain node itself.
        stList_append(blocks, chain->parentEdge->block);
        return blocks;
    } else {
        // Bridge edge. Return the best scoring path of bridge edges,
        // according to the sum of the scoring function on the path.
        stCactusTree *node = edge->child;
        stList *below = getBlocksFromBestBridgePathBelow(node, NULL, scoreFn);
        stList *above = getBlocksFromBestBridgePathAbove(node->parent, node, scoreFn);
        stList *blocks = stList_construct();
        stList_append(blocks, edge->block);
        stList_appendAll(blocks, below);
        stList_appendAll(blocks, above);
        stList_destruct(below);
        stList_destruct(above);
        return blocks;
    }
}

stList *stOnlineCactus_getGloballyWorstMaximalChainOrBridgePath(stOnlineCactus *cactus, int64_t scoreFn(void *)) {
    stHashIterator *blockIt = stHash_getIterator(cactus->blockToEdge);
    stSet *visitedBlocks = stSet_construct();
    stList *worstPath = NULL;
    int64_t worstScore = INT64_MAX;
    void *block;
    while ((block = stHash_getNext(blockIt)) != NULL) {
        if (!stSet_search(visitedBlocks, block)) {
            stList *blockPath = stOnlineCactus_getMaximalChainOrBridgePath(cactus, block, scoreFn);
            for (int64_t i = 0; i < stList_length(blockPath); i++) {
                stSet_insert(visitedBlocks, stList_get(blockPath, i));
            }
            int64_t score = scoreBlocks(blockPath, scoreFn);
            if (score < worstScore) {
                worstScore = score;
                stList_destruct(worstPath);
                worstPath = blockPath;
            } else {
                stList_destruct(blockPath);
            }
        }
    }
    stHash_destructIterator(blockIt);
    stSet_destruct(visitedBlocks);
    return worstPath;
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
    if (tree->nodes != NULL) {
        if (stCactusTree_type(tree) == CHAIN) {
            st_errAbort("chain has registered nodes");
        }
        if (stSet_size(tree->nodes) == 0) {
            st_errAbort("dangling net with no nodes");
        }
        stSetIterator *it = stSet_getIterator(tree->nodes);
        void *node;
        while ((node = stSet_getNext(it)) != NULL) {
            if (stHash_search(cactus->nodeToNet, node) != tree) {
                st_errAbort("node not mapped to correct net");
            }
            // Check that each of the ends in the node belong to the
            // node.
            stSet *ends = stHash_search(cactus->nodeToEnds, node);
            stSetIterator *endIt = stSet_getIterator(ends);
            void *end;
            while ((end = stSet_getNext(endIt)) != NULL) {
                void *node2 = stHash_search(cactus->endToNode, end);
                if (node != node2) {
                    st_errAbort("node contains end in different node");
                }
            }
            stSet_destructIterator(endIt);
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
        if (stHash_search(cactus->blockToEdge, block) != tree->parentEdge) {
            st_errAbort("block edge not correctly registered in blockToEdge");
        }
        void *end1 = cactus->edgeToEnd(block, 0);
        void *end2 = cactus->edgeToEnd(block, 1);
        if (tree->type == NET) {
            bool end1present = getNetFromEnd(cactus, end1) == tree;
            bool end2present = getNetFromEnd(cactus, end2) == tree;
            if (!end1present && !end2present) {
                st_errAbort("block edge is not even attached to correct node");
            }
            if (tree->parent->type == CHAIN) {
                // Chain edge.
                if (tree->prev == NULL) {
                    // This edge connects the current net to the grandparent
                    bool end1presentGP = getNetFromEnd(cactus, end1) == tree->parent->parent;
                    bool end2presentGP = getNetFromEnd(cactus, end2) == tree->parent->parent;
                    if (!((end1present && end2presentGP) || (end2present && end1presentGP))) {
                        st_errAbort("first chain (front-edge) not placed correctly");
                    }
                } else {
                    // This edge connects the current net to the one previous.
                    bool end1presentPrev = getNetFromEnd(cactus, end1) == tree->prev;
                    bool end2presentPrev = getNetFromEnd(cactus, end2) == tree->prev;
                    if (!((end1present && end2presentPrev) || (end2present && end1presentPrev))) {
                        st_errAbort("internal chain link not placed correctly");
                    }
                }
            } else {
                // Bridge edge.
                assert(tree->parent->type == NET);
                bool end1presentParent = getNetFromEnd(cactus, end1) == tree->parent;
                bool end2presentParent = getNetFromEnd(cactus, end2) == tree->parent;
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
                if (!(getNetFromEnd(cactus, end1) == tree->parent) || !(getNetFromEnd(cactus, end2) == tree->parent)) {
                    st_errAbort("parent edge of self-loop chain is incorrect");
                }
            } else {
                // The parent edge connects the last child to the parent net.
                bool end1presentChild = getNetFromEnd(cactus, end1) == child;
                bool end2presentChild = getNetFromEnd(cactus, end2) == child;
                bool end1presentParent = getNetFromEnd(cactus, end1) == tree->parent;
                bool end2presentParent = getNetFromEnd(cactus, end2) == tree->parent;
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

    if (stCactusTree_type(tree) == NET && !stOnlineCactus_check3EC(cactus, tree)) {
        st_errAbort("net graph is not 3-edge-connected");
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
    // Check that the end<->node mappings are correct.
    stHashIterator *it = stHash_getIterator(cactus->endToNode);
    void *end;
    while ((end = stHash_getNext(it)) != NULL) {
        void *node = stHash_search(cactus->endToNode, end);
        stSet *ends = stHash_search(cactus->nodeToEnds, node);
        if (stSet_search(ends, end) == NULL) {
            st_errAbort("node does not contain correct end");
        }
    }
    stHash_destructIterator(it);

    // Check that the node<->net mappings are correct.
    it = stHash_getIterator(cactus->nodeToNet);
    void *node;
    while ((node = stHash_getNext(it)) != NULL) {
        stCactusTree *net = stHash_search(cactus->nodeToNet, node);
        if (stSet_search(net->nodes, node) == NULL) {
            st_errAbort("net does not contain correct node");
        }
    }
    stHash_destructIterator(it);

    // Check that the block (i.e. edge) mappings are correct.
    it = stHash_getIterator(cactus->blockToEdge);
    void *block;
    while ((block = stHash_getNext(it)) != NULL) {
        stCactusTreeEdge *edge = stHash_search(cactus->blockToEdge, block);
        if (edge->block != block) {
            st_errAbort("Block->edge mapping broken");
        }
    }
    stHash_destructIterator(it);

    for (int64_t i = 0; i < stList_length(cactus->trees); i++) {
        stCactusTree_check(stList_get(cactus->trees, i), cactus);
    }
}
