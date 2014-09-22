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
 * The returned list is the set of greater than degree 1 blocks that are within baseDistance and blockDistance of the segments in the given block.
 * Each block is represented as a FeatureBlock.
 * Strings is a hash of pinchThreads to actual DNA strings.
 */
stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t maxBaseDistance,
        int64_t maxBlockDistance,
        bool ignoreUnalignedBases, bool onlyIncludeCompleteFeatureBlocks, stHash *strings);

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
stList *stFeatureColumn_getFeatureColumns(stList *featureBlocks, stPinchBlock *block);

/*
 * Gets a feature matrix representing SNPs.
 * The distance weight function is the amount of pairwise weight to place on a given feature, where its inputs
 * are, for each thread involved in the pair,  the distance between the midpoint of the thread and the location of the feature.
 * If distanceWeightFn = null then stPinchPhylogeny_constantDistanceWeightFn is used.
 */
stMatrix *stPinchPhylogeny_getMatrixFromSubstitutions(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns);

/*
 * Gets a matrix representing breakpoints.
 * If distanceWeightFn = null then stPinchPhylogeny_constantDistanceWeightFn is used.
 */
stMatrix *stPinchPhylogeny_getMatrixFromBreakpoints(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns);

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

// Gets a list of disjoint leaf sets (ingroup clades separated by
// outgroups) from a set of feature columns. The leaves are stIntTuples
// corresponding to the index of their respective feature column
//
// NB: "leaf set" is somewhat a misnomer since this just returns an
// stList of stLists of stIntTuples
//
// TODO: add distance weight function and matrix-merge function as
// parameters if necessary
stList *stPinchPhylogeny_getLeafSetsFromFeatureColumns(stList *featureColumns,
                                                       stPinchBlock *block,
                                                       int64_t numBootstraps,
                                                       double confidenceThreshold,
                                                       stList *outgroups);


// Gives the likelihood of the tree given the feature columns.
double stPinchPhylogeny_likelihood(stTree *tree, stList *featureColumns);

stTree *stPinchPhylogeny_buildTreeFromFeatureColumns(stList *featureColumns,
                                                     stPinchBlock *block,
                                                     bool bootstrap);

// For a tree with stReconciliationInfo on the internal nodes, assigns
// a log-likelihood to the events in the tree. Uses an approximation that
// considers only duplication likelihood, rather than the full
// birth-death process.
// Dup-rate is per species-tree branch length unit.
double stPinchPhylogeny_reconciliationLikelihood(stTree *tree, stTree *speciesTree, double dupRate);

#endif
