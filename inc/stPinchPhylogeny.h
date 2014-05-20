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
    int64_t bootstraps;     // Number of bootstraps that support this split.
    int64_t totalNumLeaves; // Total number of leaves overall in the
                            // tree (which is the size of
                            // leavesBelow). Not strictly necessary,
                            // but convenient
} stPhylogenyInfo;

stTree *neighborJoin(double **distances, int64_t numSequences);

#endif
