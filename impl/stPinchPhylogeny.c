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
 * Gets the pinch-end adjacent of the next block connected to the given segment, traversing either 5' or 3' depending on _5PrimeTraversal.
 */
static inline stPinchEnd getAdjacentPinchEnd(stPinchSegment *segment, bool _5PrimeTraversal) {
    while(1) {
        segment = _5PrimeTraversal ? stPinchSegment_get5Prime(segment) : stPinchSegment_get3Prime(segment);
        if(segment == NULL) {
            break;
        }
        stPinchBlock *block = stPinchSegment_getBlock(segment);
        if(block != NULL) {
            return stPinchEnd_constructStatic(block, stPinchEnd_endOrientation(_5PrimeTraversal, segment));
        }
    }
    return stPinchEnd_constructStatic(NULL, 1);
}

stFeatureSegment *stFeatureSegment_construct(stPinchSegment *segment, stHash *pinchThreadsToStrings, int64_t index, int64_t distance) {
    stFeatureSegment *featureSegment = st_malloc(sizeof(stFeatureSegment));
    const char *threadString = stHash_search(pinchThreadsToStrings, stPinchSegment_getThread(segment));
    assert(threadString != NULL);
    featureSegment->string = threadString + stPinchSegment_getStart(segment) - stPinchThread_getStart(stPinchSegment_getThread(segment));
    featureSegment->length = stPinchSegment_getLength(segment);
    featureSegment->reverseComplement = !stPinchSegment_getBlockOrientation(segment);
    featureSegment->pPinchEnd = getAdjacentPinchEnd(segment, !featureSegment->reverseComplement);
    featureSegment->nPinchEnd = getAdjacentPinchEnd(segment, featureSegment->reverseComplement);
    featureSegment->distance = distance;
    featureSegment->nFeatureSegment = NULL;
    featureSegment->segment = segment;
    featureSegment->segmentIndex = index;
    return featureSegment;
}

void featureSegment_destruct(stFeatureSegment *featureSegment) {
    if (featureSegment->nFeatureSegment) {
        featureSegment_destruct(featureSegment->nFeatureSegment);
    }
    free(featureSegment);
}

static char reverseComplement(char base) {
    /*
     * Why isn't this in sonlib some place!
     */
    base = toupper(base);
    switch (base) {
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
    assert(columnIndex < featureSegment->length);
    return featureSegment->reverseComplement ?
            reverseComplement(featureSegment->string[featureSegment->length - 1 - columnIndex]) :
            featureSegment->string[columnIndex];
}

int64_t stFeatureSegment_getColumnDistance(stFeatureSegment *featureSegment, int64_t columnIndex) {
    assert(columnIndex < featureSegment->length);
    return featureSegment->reverseComplement ?
                featureSegment->distance + featureSegment->length - 1 - columnIndex :
                featureSegment->distance + columnIndex;
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
bool stFeatureSegment_leftAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2,
        int64_t columnIndex) {
    return columnIndex > 0 ?
            1 :
            (fSegment1->pPinchEnd.block == fSegment2->pPinchEnd.block
                    && fSegment1->pPinchEnd.orientation == fSegment2->pPinchEnd.orientation);
}

/*
 * Returns non-zero if the right adjacency of the segments is equal.
 */
bool stFeatureSegment_rightAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2,
        int64_t columnIndex) {
    return columnIndex < fSegment1->length - 1 ?
            1 :
            (fSegment1->pPinchEnd.block == fSegment2->pPinchEnd.block
                    && fSegment1->pPinchEnd.orientation == fSegment2->pPinchEnd.orientation);
}

/*
 * Represents a block for the purposes of tree building, composed of a set of featureSegments.
 */
typedef struct _stFeatureBlock {
    int64_t length; //The length of the block.
    stFeatureSegment *head; //The first feature segment in the block.
    stFeatureSegment *tail; //The last feature segment in the block.
} stFeatureBlock;

stFeatureBlock *stFeatureBlock_construct(stFeatureSegment *firstSegment, stPinchBlock *block) {
    stFeatureBlock *featureBlock = st_malloc(sizeof(featureBlock));
    featureBlock->head = firstSegment;
    featureBlock->tail = firstSegment;
    firstSegment->nFeatureSegment = NULL; //Defensive assignment.
    featureBlock->length = stPinchBlock_getLength(block);
    return featureBlock;
}

void stFeatureBlock_destruct(stFeatureBlock *featureBlock) {
    featureSegment_destruct(featureBlock->head);
    free(featureBlock);
}

/*
 * Finds the 5' most segment in the thread that contains bases within maxBaseDistance of the mid point of the segment, and within
 * maxBlockDistance.
 */
static stPinchSegment *get5PrimeMostSegment(stPinchSegment *segment, int64_t *baseDistance, int64_t *blockDistance,
        int64_t maxBaseDistance, int64_t maxBlockDistance,
        bool ignoreUnalignedBases) {
    *baseDistance = -stPinchSegment_getLength(segment) / 2;
    *blockDistance = 0;
    while (stPinchSegment_get5Prime(segment) != NULL && -*baseDistance < maxBaseDistance
            && -*blockDistance < maxBlockDistance) {
        segment = stPinchSegment_get5Prime(segment);
        if (stPinchSegment_getBlock(segment) != NULL || !ignoreUnalignedBases) {
            *baseDistance -= stPinchSegment_getLength(segment);
            (*blockDistance)--;
        }
    }
    return segment;
}

/*
 * Gets the number of distinct segment indices the featureBlock contains. Relies on the indices of segments being sorted
 * (as they are by stFeatureBlock_getContextualFeatureBlocks)
 */
static int64_t countDistinctIndices(stFeatureBlock *featureBlock) {
    stFeatureSegment *segment = featureBlock->head;
    int64_t i=1, j=segment->segmentIndex;
    while((segment = segment->nFeatureSegment) != NULL) {
      if(segment->segmentIndex != j) {
          i++;
          segment->segmentIndex = j;
      }
    }
    return i;
}

/*
 * The returned list is the set of greater than degree 1 blocks that are within baseDistance and blockDistance of the segments in the given block.
 * Each block is represented as a FeatureBlock.
 * Strings is a hash of pinchThreads to actual DNA strings.
 */
stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t maxBaseDistance,
        int64_t maxBlockDistance,
        bool ignoreUnalignedBases, bool onlyIncludeCompleteFeatureBlocks, stHash *strings) {
    /*
     * First build the set of feature blocks, not caring if this produces trivial blocks.
     */
    stHash *blocksToFeatureBlocks = stHash_construct(); //Hash to store allow featureBlocks to be progressively added to.
    int64_t i = 0; //Index of segments in block.
    stPinchBlockIt it = stPinchBlock_getSegmentIterator(block);
    stPinchSegment *segment;
    while ((segment = stPinchBlockIt_getNext(&it)) != NULL) { //For each segment build the feature segment.
        int64_t blockDistance, baseDistance; //Distances from segment2 to segment
        //Base distance is the difference from the start coordinate of segment2 to the mid point of segment.
        stPinchSegment *segment2 = get5PrimeMostSegment(segment, &blockDistance, &baseDistance, maxBaseDistance,
                maxBlockDistance, ignoreUnalignedBases);
        do {
            stPinchBlock *block2;
            if ((block2 = stPinchSegment_getBlock(segment2)) != NULL) {
                //Make a new feature segment
                stFeatureSegment *fSegment = stFeatureSegment_construct(segment2, strings, i, baseDistance);
                //Attach it to a new block.
                stFeatureBlock *featureBlock = stHash_search(blocksToFeatureBlocks, block2);
                if (featureBlock == NULL) { //Create a new feature block
                    featureBlock = stFeatureBlock_construct(fSegment, block2);
                    stHash_insert(blocksToFeatureBlocks, block2, featureBlock);
                } else { //Link it to the existing feature block
                    featureBlock->tail->nFeatureSegment = fSegment;
                    featureBlock->tail = fSegment;
                }
                baseDistance += stPinchSegment_getLength(segment2);
                blockDistance++;
            } else if (!ignoreUnalignedBases) { //Only add the unaligned segment if not ignoring such segments.
                baseDistance += stPinchSegment_getLength(segment2);
                blockDistance++;
            }
            segment2 = stPinchSegment_get3Prime(segment2);
        } while (segment2 != NULL && baseDistance <= maxBaseDistance && blockDistance <= maxBlockDistance);
        i++; //Increase segment block index.
    }
    /*
     * Now filter the set of blocks so that only blocks containing at least two sequences are present.
     */
    stList *unfilteredFeatureBlocks = stHash_getValues(blocksToFeatureBlocks);
    stList *featureBlocks = stList_construct3(0, (void (*)(void *)) stFeatureBlock_destruct);
    stHash_destruct(blocksToFeatureBlocks); //Cleanup now, as no longer needed, and values freed.
    for (int64_t i = 0; i < stList_length(unfilteredFeatureBlocks); i++) {
        stFeatureBlock *featureBlock = stList_get(unfilteredFeatureBlocks, i);
        if (featureBlock->head != featureBlock->tail && (!onlyIncludeCompleteFeatureBlocks ||
               countDistinctIndices(featureBlock) == stPinchBlock_getDegree(block))) { //Is non-trivial
            stList_append(featureBlocks, featureBlock);
        } else {
            stFeatureBlock_destruct(featureBlock); //This is a trivial/unneeded feature block, so remove.
        }
    }
    stList_destruct(unfilteredFeatureBlocks);

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
stList *stFeatureColumn_getFeatureColumns(stList *featureBlocks, stPinchBlock *block) {
    stList *featureColumns = stList_construct3(0, (void (*)(void *)) stFeatureColumn_destruct);
    for (int64_t i = 0; i < stList_length(featureBlocks); i++) {
        stFeatureBlock *featureBlock = stList_get(featureBlocks, i);
        for (int64_t j = 0; j < featureBlock->length; j++) {
            stList_append(featureColumns, stFeatureColumn_construct(featureBlock, j));
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
void stFeatureMatrix_increaseIdentityCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2, int64_t increment) {
    assert(index1 < index2);
    (*stFeatureMatrix_offsetFn(matrix, index1, index2)) += increment;
}

/*
 * Increase difference count between two indices.
 */
void stFeatureMatrix_increaseDifferenceCount(stFeatureMatrix *matrix, int64_t index1, int64_t index2, int64_t increment) {
    assert(index1 < index2);
    (*stFeatureMatrix_offsetFn(matrix, index2, index1)) += increment;
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
    for (int64_t i = 0; i < featureMatrix->n; i++) {
        matrix[i * featureMatrix->n] = 0.0;
        for (int64_t j = i + 1; j < featureMatrix->n; j++) {
            int64_t similarities = stFeatureMatrix_getIdentityCount(featureMatrix, i, j);
            int64_t differences = stFeatureMatrix_getDifferenceCount(featureMatrix, i, j);
            matrix[i * featureMatrix->n + j] = ((double) differences) / (similarities + differences);
            matrix[j * featureMatrix->n + i] = matrix[i * featureMatrix->n + j];
        }
    }
    return matrix;
}

/*
 * Merges together two feature matrices, weighting them by the given integers. This
 * allows substitution and breakpoint matrices to be combined.
 */
stFeatureMatrix *stFeatureMatrix_merge(stFeatureMatrix *featureMatrix1, int64_t weight1,
        stFeatureMatrix *featureMatrix2, int64_t weight2) {
    assert(featureMatrix1->n == featureMatrix2->n);
    stFeatureMatrix *mergedMatrix = stFeatureMatrix_construct(featureMatrix1->n);
    for (int64_t i = 0; i < featureMatrix1->n * featureMatrix1->n; i++) {
        mergedMatrix->featureMatrix[i] = featureMatrix1->featureMatrix[i] * weight1
                + featureMatrix2->featureMatrix[i] * weight2;
    }
    return mergedMatrix;
}

/*
 * Adds to a feature matrix by iterating through the feature columns and adding in features of the column, according to the given function.
 */
void stFeatureMatrix_add(stFeatureMatrix *matrix, stList *featureColumns,
        int64_t distanceWeightFn(int64_t, int64_t),
        bool sampleColumns, bool (*equalFn)(stFeatureSegment *, stFeatureSegment *, int64_t)) {
    for (int64_t i = 0; i < stList_length(featureColumns); i++) {
        stFeatureColumn *featureColumn = stList_get(featureColumns,
                sampleColumns ? st_randomInt(0, stList_length(featureColumns)) : i);
        stFeatureSegment *fSegment = featureColumn->featureBlock->head;
        int64_t distance1 = stFeatureSegment_getColumnDistance(fSegment, featureColumn->columnIndex);
        while (fSegment != NULL) {
            stFeatureSegment *nSegment = fSegment->nFeatureSegment;
            while (nSegment != NULL) {
                int64_t distance2 = stFeatureSegment_getColumnDistance(nSegment, featureColumn->columnIndex);
                if (equalFn(fSegment, nSegment, featureColumn->columnIndex)) {
                    stFeatureMatrix_increaseIdentityCount(matrix, fSegment->segmentIndex,
                            nSegment->segmentIndex, distanceWeightFn(distance1, distance2));
                } else {
                    stFeatureMatrix_increaseDifferenceCount(matrix, fSegment->segmentIndex,
                            nSegment->segmentIndex, distanceWeightFn(distance1, distance2));
                }
                nSegment = nSegment->nFeatureSegment;
            }
            fSegment = fSegment->nFeatureSegment;
        }
    }
}

/*
 * Gets a feature matrix representing SNPs
 */
stFeatureMatrix *stFeatureMatrix_constructFromSubstitutions(stList *featureColumns, stPinchBlock *block,
        int64_t distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    stFeatureMatrix *matrix = stFeatureMatrix_construct(stPinchBlock_getDegree(block));
    stFeatureMatrix_add(matrix, featureColumns, distanceWeightFn, sampleColumns, stFeatureSegment_basesEqual);
    return matrix;
}

/*
 * Gets a feature matrix representing breakpoints.
 */
stFeatureMatrix *stFeatureMatrix_constructFromBreakPoints(stList *featureColumns, stPinchBlock *block,
        int64_t distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    stFeatureMatrix *matrix = stFeatureMatrix_construct(stPinchBlock_getDegree(block));
    stFeatureMatrix_add(matrix, featureColumns, distanceWeightFn, sampleColumns,
            stFeatureSegment_leftAdjacenciesEqual);
    stFeatureMatrix_add(matrix, featureColumns, distanceWeightFn, sampleColumns,
            stFeatureSegment_rightAdjacenciesEqual);
    return matrix;
}

