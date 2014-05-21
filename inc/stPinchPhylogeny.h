#ifndef STPINCHPHYLOGENY_H_
#define STPINCHPHYLOGENY_H_
#include "sonLib.h"
#include "stPinchGraphs.h"

/*
 * Represents a segment for the purposes of building trees from SNPs and breakpoints, making
 * it easy to extract these differences/similarities, collectively features, from the matrix.
 */
typedef struct _stFeatureSegment stFeatureSegment;
struct _stFeatureSegment {
    const char *string; //The bases of the actual underlying string. This is a pointer to the original sequence, starting from the first position of the underlying segment.
    int64_t length; //The length of the segment
    bool reverseComplement; //If reverse complement then the string must be reverse complemented to read.
    stPinchEnd pPinchEnd; //The previous (5 prime) adjacent block.
    stPinchEnd nPinchEnd; //The next (3 prime) adjacent block.
    int64_t distance; //The distance of the first base in the segment from the mid-point of the chosen segment.
    stFeatureSegment *nFeatureSegment; //The next tree segment in the featureBlock.
    int64_t segmentIndex; //The index of the segment in the distance matrix.
    stPinchSegment *segment; //The underlying pinch segment.
};

/*
 * Represents a block for the purposes of tree building, composed of a set of featureSegments.
 */
typedef struct _stFeatureBlock {
    int64_t length; //The length of the block.
    stFeatureSegment *head; //The first feature segment in the block.
    stFeatureSegment *tail; //The last feature segment in the block.
} stFeatureBlock;

/*
 * The returned list is the set of greater than degree 1 blocks that are within baseDistance and blockDistance of the segments in the given block.
 * Each block is represented as a FeatureBlock.
 * Strings is a hash of pinchThreads to actual DNA strings.
 */
stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t maxBaseDistance,
        int64_t maxBlockDistance,
        bool ignoreUnalignedBases, bool onlyIncludeCompleteFeatureBlocks, stHash *strings);

/*
 * Represents one column of a tree block.
 */
typedef struct _stFeatureColumn {
    int64_t columnIndex; //The offset in the block of the column.
    stFeatureBlock *featureBlock; //The block in question.
} stFeatureColumn;

/*
 * Gets a list of feature columns for the blocks in the input list of featureBlocks,
 * to allow sampling with replacement for bootstrapping. The ordering of the columns
 * follows the ordering of the feature blocks.
 */
stList *stFeatureColumn_getFeatureColumns(stList *featureBlocks, stPinchBlock *block);

/*
 * Gets a feature matrix representing SNPs.
 * The distance weight function is the amount of pairwise weight to place on a given feature, where its inputs
 * are, for each thread involved in the pair,  the distance between the midpoint of the thread and the location of the feature.
 */
stMatrix *stPinchPhylogeny_getMatrixFromSubstitutions(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns);

/*
 * Gets a matrix representing breakpoints.
 */
stMatrix *stPinchPhylogeny_getMatrixFromBreakPoints(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns);

/*
 * Gets a symmetric distance matrix representing the feature matrix.
 */
stMatrix *stPinchPhylogeny_getSymmetricDistanceMatrix(stMatrix *matrix);

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
#endif
