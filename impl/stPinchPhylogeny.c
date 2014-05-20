/*
 * stPinchGraphs.c
 *
 *  Created on: 19th May 2014
 *      Author: benedictpaten
 *
 *      Algorithms to build phylogenetic trees for blocks.
 */

#include <stdlib.h>
#include <ctype.h>
#include "sonLib.h"
#include "stPinchGraphs.h"

/*
 * Represents a segment for the purposes of building trees from SNPs and breakpoints, making
 * it easy to extract these differences/similarities, collectively features, from the matrix.
 */
typedef struct _stFeatureSegment stFeatureSegment;
struct _stFeatureSegment {
    const char *string; //The bases of the actual underlying string. This is a pointer to the original sequence or its reverse complement.
    int64_t stringLength; //The length of the string
    bool reverseComplement; //If reverse complement then the start is the end (exclusive), and the base must be reverse complemented to read.
    stPinchEnd nPinchEnd; //The next adjacent block.
    stPinchEnd pPinchEnd; //The previous adjacent block.
    int64_t distance; //The distance of the first base in the segment from the mid-point of the chosen segment.
    stFeatureSegment *nFeatureSegment; //The next tree segment.
    int64_t segmentIndex; //The index of the segment in the distance matrix.
    stPinchSegment *segment; //The underlying pinch segment.
};

void featureSegment_destruct(stFeatureSegment *featureSegment) {
    if(featureSegment->nFeatureSegment) {
        featureSegment_destruct(featureSegment->nFeatureSegment);
    }
    free(featureSegment);
}


static char reverseComplement(char base) {
    /*
     * Why isn't this in sonlib some place!
     */
    base = toupper(base);
    switch(base) {
        case 'A':
                return 'T';
        case 'T':
                return 'A';
        case 'C':
                return 'G';
        case 'G':
                return 'C';
        default:
            return base;
    }
}

/*
 * Get the base at a given location in a segment.
 */
char stFeatureSegment_getBase(stFeatureSegment *featureSegment, int64_t columnIndex) {
    assert(columnIndex < featureSegment->stringLength);
    return featureSegment->reverseComplement ? reverseComplement(featureSegment->string[featureSegment->stringLength - 1 - columnIndex]) : featureSegment->string[columnIndex];
}

/*
 * Returns non-zero if the bases of the two segments at the given index are equal.
 */
bool stFeatureSegment_basesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2, int64_t columnIndex) {
    return stFeatureSegment_getBase(fSegment1, columnIndex) == stFeatureSegment_getBase(fSegment2, columnIndex);
}

/*
 * Returns non-zero if the left adjacency of the segments is equal.
 */
bool stFeatureSegment_leftAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2, int64_t columnIndex) {
    return columnIndex > 0 ? 1 : (fSegment1->pPinchEnd.block == fSegment2->pPinchEnd.block && fSegment1->pPinchEnd.orientation == fSegment2->pPinchEnd.orientation);
}

/*
 * Returns non-zero if the right adjacency of the segments is equal.
 */
bool stFeatureSegment_rightAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2, int64_t columnIndex) {
    return columnIndex < fSegment1->stringLength - 1 ? 1 : (fSegment1->pPinchEnd.block == fSegment2->pPinchEnd.block && fSegment1->pPinchEnd.orientation == fSegment2->pPinchEnd.orientation);
}

/*
 * Represents a block for the purposes of tree building, composed of a set of featureSegments.
 */
typedef struct _stFeatureBlock {
    int64_t length; //The length of the block.
    stFeatureSegment *head; //The first feature segment in the block.
} stFeatureBlock;

void stFeatureBlock_destruct(stFeatureBlock *featureBlock) {
    featureSegment_destruct(featureBlock->head);
    free(featureBlock);
}

/*
 * The returned list is the set of greater than degree 1 blocks that are within baseDistance and blockDistance of the segments in the given block.
 * Each block is represented as a FeatureBlock.
 * Strings is a hash of pinchThreads to actual DNA strings.
 */
stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t baseDistance,
        int64_t blockDistance, bool ignoreUnalignedBases, stHash *strings) {
    stList *featureBlocks = stList_construct3(0, (void (*)(void *))stFeatureBlock_destruct);

    /*TODO*/

    return featureBlocks;
}

/*
 * Represents one column of a tree block.
 */
typedef struct _stFeatureColumn {
    int64_t columnIndex; //The offset in the block of the column.
    stFeatureBlock *featureBlock; //The block in question.
} stFeatureColumn;

stFeatureColumn *stFeatureColumn_construct(stFeatureBlock *featureBlock, int64_t columnIndex) {
    stFeatureColumn *featureColumn = st_malloc(sizeof(stFeatureColumn));
    featureColumn->featureBlock = featureBlock;
    featureColumn->columnIndex = columnIndex;
    return featureColumn;
}

void stFeatureColumn_destruct(stFeatureColumn *featureColumn) {
    free(featureColumn); //Does not own the block.
}

/*
 * Gets a list of feature columns for the blocks in the input list of featureBlocks,
 * to allow sampling with replacement for bootstrapping. The ordering of the columns
 * follows the ordering of the feature blocks.
 */
stList *stFeatureColumn_getFeatureColumns(stList *featureBlocks, stPinchBlock *block, bool onlyIncludeCompleteColumns) {
    stList *featureColumns = stList_construct3(0, (void (*)(void *))stFeatureColumn_destruct);
    for(int64_t i=0; i<stList_length(featureBlocks); i++) {
        stFeatureBlock *featureBlock = stList_get(featureBlocks, i);
        if(!onlyIncludeCompleteColumns || 0) { /*todo*/
            for(int64_t j=0; j<featureBlock->length; j++) {
                stList_append(featureColumns, stFeatureColumn_construct(featureBlock, j));
            }
        }
    }
    return featureColumns;
}

/*
 * Represents counts of similarities and differences between a set of sequences.
 *
 * This is so basic it should probably be a set of functions in sonLib in some form or other.
 */
typedef struct _stFeatureMatrix {
    int64_t n; //Matrix is n x n.
    int64_t *featureMatrix; //Matrix containing counts of places where sequences agree and are different.
} stFeatureMatrix;

stFeatureMatrix *stFeatureMatrix_construct(int64_t n) {
    stFeatureMatrix *featureMatrix = st_malloc(sizeof(featureMatrix));
    featureMatrix->n = n;
    featureMatrix->featureMatrix = st_calloc(n * n, sizeof(int64_t));
    return featureMatrix;
}

void stFeatureMatrix_destruct(stFeatureMatrix *featureMatrix) {
    free(featureMatrix->featureMatrix);
    free(featureMatrix);
}

static int64_t *stFeatureMatrix_offsetFn(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {
    assert(index1 >= 0 && index1 < matrix->n);
    assert(index2 >= 0 && index2 < matrix->n);
    return &matrix->featureMatrix[index1 * matrix->n + index2];
}

/*
 * Increase similarity count between two indices.
 */
void stFeatureMatrix_increaseIdentityCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {
    assert(index1 < index2);
    (*stFeatureMatrix_offsetFn(matrix, index1, index2))++;
}

/*
 * Increase difference count between two indices.
 */
void stFeatureMatrix_increaseDifferenceCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {
    assert(index1 < index2);
    (*stFeatureMatrix_offsetFn(matrix, index2, index1))++;
}

/*
 * Get number of sites where two indices agree.
 */
int64_t stFeatureMatrix_getIdentityCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {
    assert(index1 < index2);
    return *stFeatureMatrix_offsetFn(matrix, index1, index2);
}

/*
 * Get number of sites where two indices disagree.
 */
int64_t stFeatureMatrix_getDifferenceCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2) {
    assert(index1 < index2);
    return *stFeatureMatrix_offsetFn(matrix, index2, index1);
}

/*
 * Gets a symmetric distance matrix representing the feature matrix.
 */
double *stFeatureMatrix_getSymmetricDistanceMatrix(stFeatureMatrix *featureMatrix) {
    double *matrix = st_calloc(featureMatrix->n * featureMatrix->n, sizeof(double));
    for(int64_t i=0; i<featureMatrix->n; i++) {
        matrix[i * featureMatrix->n] = 0.0;
        for(int64_t j=i+1; j<featureMatrix->n; j++) {
            int64_t similarities = stFeatureMatrix_getIdentityCount(featureMatrix, i, j);
            int64_t differences = stFeatureMatrix_getDifferenceCount(featureMatrix, i, j);
            matrix[i * featureMatrix->n + j] = ((double)differences) / (similarities + differences);
            matrix[j * featureMatrix->n + i] = matrix[i * featureMatrix->n + j];
        }
    }
    return matrix;
}

/*
 * Merges together two feature matrices, weighting them by the given integers. This
 * allows substitution and breakpoint matrices to be combined.
 */
stFeatureMatrix *stFeatureMatrix_merge(stFeatureMatrix *featureMatrix1, int64_t weight1, stFeatureMatrix *featureMatrix2, int64_t weight2) {
    assert(featureMatrix1->n == featureMatrix2->n);
    stFeatureMatrix *mergedMatrix = stFeatureMatrix_construct(featureMatrix1->n);
    for(int64_t i=0; i<featureMatrix1->n * featureMatrix1->n; i++) {
        mergedMatrix->featureMatrix[i] = featureMatrix1->featureMatrix[i] * weight1 + featureMatrix2->featureMatrix[i] * weight2;
    }
    return mergedMatrix;
}

/*
 * Gets a feature matrix representing SNPs
 */
void stFeatureMatrix_add(stFeatureMatrix *matrix, stList *featureColumns, stPinchBlock *block,
                                             double distanceWeighting, bool sampleColumns, bool (*equalFn)(stFeatureSegment *, stFeatureSegment *, int64_t)) {
    for(int64_t i=0; i<stList_length(featureColumns); i++) {
        stFeatureColumn *featureColumn = stList_get(featureColumns, sampleColumns ? st_randomInt(0, stList_length(featureColumns)): i);
        stFeatureSegment *fSegment = featureColumn->featureBlock->head;
        while(fSegment != NULL) {
            stFeatureSegment *nSegment = fSegment->nFeatureSegment;
            while(nSegment != NULL) {
                if(equalFn(fSegment, nSegment, i)) {
                    stFeatureMatrix_increaseIdentityCount(matrix, fSegment->segmentIndex, nSegment->segmentIndex);
                }
                else {
                    stFeatureMatrix_increaseDifferenceCount(matrix, fSegment->segmentIndex, nSegment->segmentIndex);
                }
                nSegment = nSegment->nFeatureSegment;
            }
            fSegment = fSegment->nFeatureSegment;
        }
    }
}

stFeatureMatrix *stFeatureMatrix_constructFromSubstitutions(stList *featureColumns, stPinchBlock *block,
                                             double distanceWeighting, bool sampleColumns) {
    stFeatureMatrix *matrix = stFeatureMatrix_construct(stPinchBlock_getDegree(block));
    stFeatureMatrix_add(matrix, featureColumns, block, distanceWeighting, sampleColumns, stFeatureSegment_basesEqual);
    return matrix;
}

/*
 * Gets a feature matrix representing breakpoints.
 */
stFeatureMatrix *stFeatureMatrix_constructFromBreakPoints(stList *featureColumns, stPinchBlock *block,
                                             double distanceWeighting, bool sampleColumns) {
    stFeatureMatrix *matrix = stFeatureMatrix_construct(stPinchBlock_getDegree(block));
    stFeatureMatrix_add(matrix, featureColumns, block, distanceWeighting, sampleColumns, stFeatureSegment_leftAdjacenciesEqual);
    stFeatureMatrix_add(matrix, featureColumns, block, distanceWeighting, sampleColumns, stFeatureSegment_rightAdjacenciesEqual);
    return matrix;
}

