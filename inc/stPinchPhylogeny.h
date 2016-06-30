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
    stList *segments; // Indexed by segmentIndex--with NULL if a
                      // segment with that index is not in the column.
} stFeatureBlock;

/*
 * Represents an addition/subtraction operation on a matrix.
 */
typedef struct _stMatrixOp {
    int64_t row;
    int64_t col;
    int64_t diff;
} stMatrixOp;

/*
 * A list of "diffs" that is created from a set of feature
 * columns. Sampling can be done from this list of diffs instead of
 * from the feature columns themselves to save a bit of time.
 */
typedef struct _stMatrixDiffs {
    int64_t rows; // Size of rows of matrix
    int64_t cols; // Size of cols of matrix
    stList *diffs; // list of lists of stMatrixOps
} stMatrixDiffs;

/*
 * The returned list is the set of blocks that are within baseDistance and blockDistance of the segments in the given block.
 * Each block is represented as a FeatureBlock.
 * Strings is a hash of pinchThreads to actual DNA strings.
 * If ignoreUnalignedBases is true, only blocks with degree greater than 1 are added as features.
 */
stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t maxBaseDistance,
        int64_t maxBlockDistance,
        bool ignoreUnalignedBases, bool onlyIncludeCompleteFeatureBlocks, stHash *strings);

/*
 * As above, but given a "chain" of blocks: a list of equal degree,
 * sequentially adjacent blocks running from 5' to 3' along the block
 * orientation.
 */
stList *stFeatureBlock_getContextualFeatureBlocksForChainedBlocks(
    stList *blocks, int64_t maxBaseDistance,
    int64_t maxBlockDistance, bool ignoreUnalignedBases,
    bool onlyIncludeCompleteFeatureBlocks, stHash *strings);

/*
 * Like stFeatureBlock_getContextualFeatureBlocks, but rather than
 * feature blocks, returns a list of the blocks that would have been
 * used to create the feature blocks.
 */
stList *stFeatureBlock_getContextualBlocks(
    stPinchBlock *block, int64_t maxBaseDistance,
    int64_t maxBlockDistance, bool ignoreUnalignedBases,
    bool onlyIncludeCompleteFeatureBlocks, stHash *strings);

/*
 * Like stFeatureBlock_getContextualFeatureBlocksForChainedBlocks, but
 * rather than feature blocks, returns a list of the blocks that would
 * have been used to create the feature blocks.
 */
stList *stFeatureBlock_getContextualBlocksForChainedBlocks(
    stList *blocks, int64_t maxBaseDistance,
    int64_t maxBlockDistance, bool ignoreUnalignedBases,
    bool onlyIncludeCompleteFeatureBlocks, stHash *strings);

/*
 * Free the given feature block
 */
void stFeatureBlock_destruct(stFeatureBlock *featureBlock);

/*
 * Get the base at a given location in a segment.
 */
char stFeatureSegment_getBase(stFeatureSegment *featureSegment, int64_t columnIndex);

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
stList *stFeatureColumn_getFeatureColumns(stList *featureBlocks);

/*
 * Constructs a feature matrix from a list of diffs.
 */
stMatrix *stPinchPhylogeny_constructMatrixFromDiffs(stMatrixDiffs *matrixDiffs,
                                                    bool sample, unsigned int *seed);

void stMatrixDiffs_destruct(stMatrixDiffs *diffs);

/*
 * Gets a feature matrix representing SNPs.
 * The degree is the eventual degree of the returned matrix.
 * The distance weight function is the amount of pairwise weight to place on a given feature, where its inputs
 * are, for each thread involved in the pair,  the distance between the midpoint of the thread and the location of the feature.
 * If distanceWeightFn = null then stPinchPhylogeny_constantDistanceWeightFn is used.
 */
stMatrix *stPinchPhylogeny_getMatrixFromSubstitutions(stList *featureColumns, int64_t degree,
                                                      double distanceWeightFn(int64_t, int64_t), bool sampleColumns);

/*
 * Same as above, but saves as a diff list for more efficient bootstraps.
 */
stMatrixDiffs *stPinchPhylogeny_getMatrixDiffsFromSubstitutions(stList *featureColumns, int64_t degree,
        double distanceWeightFn(int64_t, int64_t));

/*
 * Gets a matrix representing breakpoints.
 * If distanceWeightFn = null then stPinchPhylogeny_constantDistanceWeightFn is used.
 */
stMatrix *stPinchPhylogeny_getMatrixFromBreakpoints(stList *featureColumns, int64_t degree,
                                                    double distanceWeightFn(int64_t, int64_t), bool sampleColumns);

/*
 * Same as above, but saves as a diff list for more efficient bootstraps.
 */
stMatrixDiffs *stPinchPhylogeny_getMatrixDiffsFromBreakpoints(stList *featureColumns, int64_t degree,
        double distanceWeightFn(int64_t, int64_t));

// Do a pseudo-bootstrapping of a list of matrix diffs using the binomial
// distribution to determine whether to add/subtract a diff or not
// from the canonical matrix. Should be faster than using "sample" in
// constructMatrixFromDiffs.
stMatrix *stPinchPhylogeny_bootstrapMatrixWithDiffs(stMatrix *canonicalMatrix,
                                                    stMatrixDiffs *matrixDiffs);

/*
 * Returns 1 for any pair of distances, used to weight all features, no matter separation, constantly.
 */
double stPinchPhylogeny_constantDistanceWeightFn(int64_t i, int64_t j);

/*
 * Gets a symmetric distance matrix representing the feature matrix.
 */
stMatrix *stPinchPhylogeny_getSymmetricDistanceMatrix(stMatrix *matrix);

// Returns a new tree with poorly-supported partitions removed. The
// tree must have bootstrap-support information for every node.
stTree *stPinchPhylogeny_removePoorlySupportedPartitions(stTree *tree,
                                                         double threshold);

// Split tree into subtrees such that no ingroup has a path to another
// ingroup that includes its MRCA with an outgroup
// Returns leaf sets (as stLists of length-1 stIntTuples) from the subtrees
// Outgroups should be a list of length-1 stIntTuples representing matrix indices
stList *stPinchPhylogeny_splitTreeOnOutgroups(stTree *tree, stList *outgroups);


// Gives the likelihood of the tree given the feature columns.
double stPinchPhylogeny_likelihood(stTree *tree, stList *featureColumns);

// For a tree with stReconciliationInfo on the internal nodes, assigns
// a log-likelihood to the events in the tree. Uses an approximation that
// considers only duplication likelihood, rather than the full
// birth-death process.
// Dup-rate is per species-tree branch length unit.
double stPinchPhylogeny_reconciliationLikelihood(stTree *tree, stTree *speciesTree, double dupRate);

#endif
