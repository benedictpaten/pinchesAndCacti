#ifndef STPINCHPHYLOGENY_H_
#define STPINCHPHYLOGENY_H_
#include "sonLib.h"

// Data structure for storing information about a node in a
// neighbor-joined tree.
typedef struct {
    int64_t matrixIndex;    // = -1 if an internal node, index into
                            // distance matrix if a leaf.
    int64_t *leavesBelow;   // leavesBelow[i] = 1 if leaf i is present
                            // below this node, 0 otherwise. Could be a
                            // bit array, which would make things much
                            // faster.
    int64_t numBootstraps;  // Number of bootstraps that support this split.
    double bootstrapSupport;// Fraction of bootstraps that support this split.
    int64_t totalNumLeaves; // Total number of leaves overall in the
                            // tree (which is the size of
                            // leavesBelow). Not strictly necessary,
                            // but convenient
} stPhylogenyInfo;

// Create a tree using neighbor-joining, from a
// (numSequences*numSequences) distance matrix. The leaves of the tree
// are labeled by their index in the distance matrix. A
// stPhylogenyInfo instance is stored in the clientData field of each
// node, which must be freed.
stTree *neighborJoin(double **distances, int64_t numSequences);

// Free a stPhylogenyInfo struct.
void stPhylogenyInfo_destruct(stPhylogenyInfo *info);

// Free stPhylogenyInfo data on all nodes in the tree.
void stPhylogenyInfo_destructOnTree(stTree *tree);

// Return a new tree which has its partitions scored by how often they
// appear in the bootstrap. This fills in the numBootstraps and
// bootstrapSupport fields of each node. All trees must have valid
// stPhylogenyInfo.
stTree *scoreFromBootstrap(stTree *tree, stTree *bootstrap);

// Return a new tree which has its partitions scored by how often they
// appear in the bootstraps. This fills in the numBootstraps and
// bootstrapSupport fields of each node. All trees must have valid
// stPhylogenyInfo.
stTree *scoreFromBootstraps(stTree *tree, stList *bootstraps);

// Set (and allocate) the leavesBelow and totalNumLeaves attribute in
// the phylogenyInfo for the given tree and all subtrees. The
// phylogenyInfo structure (in the clientData field) must already be
// allocated!
void setLeavesBelow(stTree *tree, int64_t totalNumLeaves);

// Returns a new tree with poorly-supported partitions removed. The
// tree must have bootstrap-support information for every node.
stTree *removePoorlySupportedPartitions(stTree *tree,
                                        double threshold);


#endif
