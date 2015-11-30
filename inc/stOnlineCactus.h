#ifndef __ST_ONLINE_CACTUS_H_
#define __ST_ONLINE_CACTUS_H_

/*
 * The online cactus implementation. Keeps track of
 * 3-edge-connectivity relationships among nodes in a "base graph", by
 * creating a "cactus forest" where the nodes are either
 * 3-edge-connected components or "chain nodes" which keep track of
 * cycles (2-edge-connected components) in the base graph. Base edges
 * are sometimes called "blocks."
 *
 * The 3-edge-connectivity relationships are maintained through
 * callbacks for basic operations on the base graph: edge insertion,
 * edge deletion, node creation, node split, and node merge.
 *
 * Edges in the base graph are assumed to have two ends, arbitrarily
 * called the + and - ends (represented by 1 and 0). Both ends of an
 * edge may be in the same node. Currently, these ends are grouped
 * into the nodes of the base graph by means of connectivity in a
 * separate graph--if two ends are connected, then they are in the
 * same node (i.e. the nodes are defined by the connected components
 * in this graph). There may be extra non-end nodes in these connected
 * components for convenience. This connectivity graph *must* be
 * updated before calling the callbacks.
 */

typedef enum {
    NET,
    CHAIN
} cactusNodeType;

typedef struct _stOnlineCactus stOnlineCactus;

typedef struct _stCactusTree stCactusTree;

typedef struct _stCactusTreeEdge stCactusTreeEdge;

typedef struct _stCactusTreeIt stCactusTreeIt;

// Create a new online cactus, given functions mapping from an edge to
// its ends and back again.
stOnlineCactus *stOnlineCactus_construct(void *(*edgeToEnd)(void *, bool),
                                         void *(*endToEdge)(void *));

// Free the online cactus properly.
void stOnlineCactus_destruct(stOnlineCactus *cactus);

// Set the function that determines the weight of each edge (for
// finding the total weight of chains and bridge paths). By default,
// each edge has weight 1. Negative weights are not allowed, sorry.
void stOnlineCactus_setWeightFn(stOnlineCactus *cactus, uint64_t (*getEdgeWeight)(const void *));

// Get the type (NET or CHAIN) of this cactus-forest node.
cactusNodeType stCactusTree_type(const stCactusTree *tree);

// Get an iterator over this node's children.
stCactusTreeIt *stCactusTree_getIt(stCactusTree *tree);

// Get the next child from the iterator.
stCactusTree *stCactusTreeIt_getNext(stCactusTreeIt *it);

// Free a cactus-tree-node iterator properly.
void stCactusTreeIt_destruct(stCactusTreeIt *it);

// Get the base graph nodes contained in this cactus node.
// Returns NULL if the node is a chain node.
// The caller must not free this set, and it may be invalidated with
// any subsequent cactus graph operations.
stSet *stCactusTree_getContainedNodes(stCactusTree *tree);

// Create a new node in the base graph.
void stOnlineCactus_createNode(stOnlineCactus *cactus, void *node);

// Add a new edge between two new ends in two nodes (which may be the
// same node) in the base graph.
void stOnlineCactus_addEdge(stOnlineCactus *cactus,
                            void *node1, void *node2,
                            void *end1, void *end2,
                            void *edge);

// Remove an edge in the base graph, and delete its two ends. If a base
// node ends up having no ends afterward, the node is considered
// deleted.
void stOnlineCactus_deleteEdge(stOnlineCactus *cactus, void *end1, void *end2, void *block);

// Merge two nodes in the base graph. This transfers all edges in
// "node1" to "node2" and deletes "node1".
void stOnlineCactus_nodeMerge(stOnlineCactus *cactus, void *node1, void *node2);

// Partition some of the ends in a particular node in the base graph,
// removing the ends in "endsToRemove" and placing them in a new node
// in the base graph. There may be extra ends not associated with any
// net in endsToRemove; they will be ignored.
// Does not take ownership of endsToRemove.
void stOnlineCactus_nodeCleave(stOnlineCactus *cactus, void *node, void *newNode, stSet *endsToRemove);

// Delete an *isolated* node from the base graph. The node must not have any associated ends/edges.
void stOnlineCactus_deleteNode(stOnlineCactus *cactus, void *node);

// Get the maximal (according to scoreFn) path of base edges including
// "block" in the cactus forest, such that if the edge is in a chain
// the path is the edges of the chain, and if the edge is a bridge,
// the path is the maximal path of bridge edges in the forest.
// Do not free the returned list.
stList *stOnlineCactus_getMaximalChainOrBridgePath(stOnlineCactus *cactus, void *block);

// Get the *smallest* maximal path (see definition above) in the cactus forest.
// Do not free the returned list.
stList *stOnlineCactus_getGloballyWorstMaximalChainOrBridgePath(stOnlineCactus *cactus);

// Print the cactus forest to stdout.
void stOnlineCactus_print(const stOnlineCactus *cactus);

// Get the newick strings for an online cactus tree, for debugging
// purposes. Uses pointer values to name the nodes, so the output will
// be inscrutable.
char *stCactusTree_getNewickString(const stCactusTree *cactus);

// Get the trees in the cactus forest (as a list of stCactusTree *).
//
// Caller is forbidden from freeing the list; any cactus graph
// operations may invalidate the list and/or any trees contained in
// it.
stList *stOnlineCactus_getTrees(const stOnlineCactus *cactus);

// Check to make sure Joel hasn't fucked anything up. Exits the
// program with an error message on finding a flaw in the cactus
// forest.
void stOnlineCactus_check(stOnlineCactus *cactus);

// Get a particular edge in the cactus forest, given an edge in the base graph.
stCactusTreeEdge *stOnlineCactus_getEdge(stOnlineCactus *cactus, void *block);

// Get the total number of operations performed on the cactus graph so far.
int64_t stOnlineCactus_getTotalNumOps(const stOnlineCactus *cactus);

// Get the number of node-merge operations performed on the cactus graph so far.
int64_t stOnlineCactus_getNumMergeOps(const stOnlineCactus *cactus);

// Get the number of node-split operations performed on the cactus graph so far.
int64_t stOnlineCactus_getNumSplitOps(const stOnlineCactus *cactus);

// Get the number of edge-addition operations performed on the cactus graph so far.
int64_t stOnlineCactus_getNumEdgeAddOps(const stOnlineCactus *cactus);

// Get the number of edge-deletion operations performed on the cactus graph so far.
int64_t stOnlineCactus_getNumEdgeDeleteOps(const stOnlineCactus *cactus);

// Get the number of node-addition operations performed on the cactus graph so far.
int64_t stOnlineCactus_getNumNodeAddOps(const stOnlineCactus *cactus);

// Get the number of node-deletion operations performed on the cactus graph so far.
int64_t stOnlineCactus_getNumNodeDeleteOps(const stOnlineCactus *cactus);

#endif
