/*
 * stPinchGraphs.c
 *
 *  Created on: 19th May 2014
 *      Author: benedictpaten
 *
 *      Algorithms to build phylogenetic trees for blocks.
 */



#include <stdlib.h>
#include "sonLib.h"
#include "stPinchGraphs.h"


/*
 * Represents a segment for the purposes of building trees from SNPs and breakpoints, making
 * it easy to extract these differences/similarities, collectively features, from the matrix.
 */
typedef struct _stFeatureSegment stFeatureSegment;
typedef struct _stFeatureSegment {
    const char *string; //The bases of the actual underlying string. This is a pointer to the original sequence or its reverse complement.
    bool reverseComplement; //If reverse complement then the start is the end (exclusive), and the base must be reverse complemented to read.
    stPinchEnd *nPinchEnd; //The next adjacent block.
    stPinchEnd *pPinchEnd; //The previous adjacent block.
    int64_t distance; //The distance of the first base in the segment from the mid-point of the chosen segment.
    stFeatureSegment *nTreeSegment; //The next tree segment.
    int64_t segmentIndex; //The index of the segment in the distance matrix.
    stPinchSegment *segment; //The underlying pinch segment.
};

void featureSegment_destruct(stFeatureSegment *featureSegment) {

}

/*
 * Represents a block for the purposes of tree building, composed of a set of featureSegments.
 */
static typedef struct _stFeatureBlock {
    int64_t length; //The length of the block.
    stFeatureSegment *head; //The first feature segment in the block.
} stFeatureBlock;

void stFeatureBlock_destruct(stFeatureBlock *featurBlock) {

}

/*
 * The returned list is the set of greater than degree 1 blocks that are within baseDistance and blockDistance of the segments in the given block.
 * Each block is represented as a FeatureBlock.
 * Strings is a hash of pinchThreads to actual DNA strings.
 */
stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t baseDistance,
        int64_t blockDistance, bool ignoreUnalignedBases, stHash *strings) {
}

/*
 * Represents one column of a tree block.
 */
typedef struct _stFeatureColumn {
    int64_t columnIndex; //The offset in the block of the column.
    stFeatureBlock *treeBlock; //The block in question.
} stFeatureColumn;

/*
 * Gets a list of feature columns for the blocks in the input list of featureBlocks,
 * to allow sampling with replacement for bootstrapping. The ordering of the columns
 * follows the ordering of the feature blocks.
 */
stList *stFeatureColumn_getFeatureColumns(stList *featureBlocks, stPinchBlock *block, bool onlyIncludeCompleteColumns) {
}

/*
 * Represents counts of similarities and differences between a set of sequences.
 */
typedef struct _stFeatureMatrix {
    int64_t n; //Matrix is n x n.
    int64_t *featureMatrix; //Matrix containing counts of places where sequences agree and are different.
} stFeatureMatrix;

/*
 * Increase similarity count between two indices.
 */
void stFeatureMatrix_increaseIdentityCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {

}

/*
 * Increase difference count between two indices.
 */
void stFeatureMatrix_increaseDifferenceCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {

}

/*
 * Get number of sites where two indices agree.
 */
int64_t stFeatureMatrix_getIdentityCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {

}

/*
 * Get number of sites where two indices disagree.
 */
int64_t stFeatureMatrix_getDifferenceCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {

}

/*
 * Gets a symmetric distance matrix representing the feature matrix.
 */
double *stFeatureMatrix_getSymmetricDistanceMatrix(stFeatureMatrix *featureMatrix) {

}

/*
 * Gets a feature matrix representing SNPs
 */
stFeatureMatrix *stFeatureMatrix_constructFromSubstitutions(stList *treeColumns, stPinchBlock *block,
                                             double distanceWeighting, bool sampleColumns) {
}

/*
 * Gets a feature matrix representing breakpoints.
 */
stFeatureMatrix *stFeatureMatrix_constructFromBreakPoints(stList *treeColumns, stPinchBlock *block,
                                             double distanceWeighting, bool sampleColumns) {
}

/*
 * Merges together two feature matrices, weighting them by the given integers. This
 * allows substitution and breakpoint matrices to be combined.
 */
stFeatureMatrix *stFeatureMatrix_merge(stFeatureMatrix *featureMatrix1, int64_t weight1, stFeatureMatrix *featureMatrix2, int64_t weight2) {

}

stFeatureMatrix_destruct(stFeatureMatrix *featureMatrix) {

}

