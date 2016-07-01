/*
 * stPinchGraphs.c
 *
 *  Created on: 19th May 2014
 *      Author: benedictpaten
 *
 *      Algorithms to build phylogenetic trees for blocks.
 */

// For rand_r.
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "sonLib.h"
#include "stPinchGraphs.h"
#include "stPinchPhylogeny.h"
#include "stPhylogeny.h"
#include "stSpimapLayer.h"

#define BINOM_CEILING 10

/*
 * Gets the pinch-end adjacent of the next block connected to the given segment, traversing either 5' or 3' depending on _5PrimeTraversal.
 */
static inline stPinchEnd getAdjacentPinchEnd(stPinchSegment *segment, bool _5PrimeTraversal) {
    while (1) {
        segment = _5PrimeTraversal ? stPinchSegment_get5Prime(segment) : stPinchSegment_get3Prime(segment);
        if (segment == NULL) {
            break;
        }
        stPinchBlock *block = stPinchSegment_getBlock(segment);
        if (block != NULL) {
            return stPinchEnd_constructStatic(block, stPinchEnd_endOrientation(_5PrimeTraversal, segment));
        }
    }
    return stPinchEnd_constructStatic(NULL, 1);
}

static stFeatureSegment *stFeatureSegment_construct(stPinchSegment *segment, stHash *pinchThreadsToStrings,
        int64_t index, int64_t distance) {
    stFeatureSegment *featureSegment = st_malloc(sizeof(stFeatureSegment));
    const char *threadString = stHash_search(pinchThreadsToStrings, stPinchSegment_getThread(segment));
    assert(threadString != NULL);
    featureSegment->string = threadString + stPinchSegment_getStart(segment)
            - stPinchThread_getStart(stPinchSegment_getThread(segment));
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

static void stFeatureSegment_destruct(stFeatureSegment *featureSegment) {
    if (featureSegment->nFeatureSegment) {
        stFeatureSegment_destruct(featureSegment->nFeatureSegment);
    }
    free(featureSegment);
}

char stFeatureSegment_getBase(stFeatureSegment *featureSegment, int64_t columnIndex) {
    assert(columnIndex < featureSegment->length);
    return featureSegment->reverseComplement ?
            stString_reverseComplementChar(featureSegment->string[featureSegment->length - 1 - columnIndex]) :
            featureSegment->string[columnIndex];
}

/*
 * Gets distance of given column index from mid point of the segment in the chosen block, in terms of the counted base distance calculated
 * in stFeatureBlock_getContextualFeatureBlocks.
 */
int64_t stFeatureSegment_getColumnDistance(stFeatureSegment *featureSegment, int64_t columnIndex) {
    assert(columnIndex < featureSegment->length);
    return featureSegment->reverseComplement ?
            featureSegment->distance + featureSegment->length - 1 - columnIndex : featureSegment->distance + columnIndex;
}

/*
 * Returns non-zero if the bases of the two segments at the given index are equal.
 */
bool stFeatureSegment_basesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2, int64_t columnIndex) {
    return toupper(stFeatureSegment_getBase(fSegment1, columnIndex)) == toupper(stFeatureSegment_getBase(fSegment2, columnIndex));
}

/*
 * Returns nonzero if the base is a wild card.
 */
bool stFeatureSegment_baseIsWildCard(stFeatureSegment *fSegment, int64_t columnIndex) {
    char b = toupper(stFeatureSegment_getBase(fSegment, columnIndex));
    return !(b == 'A' || b == 'C' || b == 'G' || b == 'T');
}

/*
 * Returns non-zero if the left adjacency of the segments is equal.
 */
static bool stFeatureSegment_leftAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2,
        int64_t columnIndex) {
    return columnIndex > 0 ? 1 : stPinchEnd_equalsFn(&(fSegment1->pPinchEnd), &(fSegment2->pPinchEnd));
}

static bool stFeatureSegment_leftAdjacencyIsWildCard(stFeatureSegment *fSegment, int64_t columnIndex) {
    return columnIndex > 0 ? 0 : fSegment->pPinchEnd.block == NULL;
}

/*
 * Returns non-zero if the right adjacency of the segments is equal.
 */
static bool stFeatureSegment_rightAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2,
        int64_t columnIndex) {
    return columnIndex < fSegment1->length - 1 ?
            1 : stPinchEnd_equalsFn(&(fSegment1->nPinchEnd), &(fSegment2->nPinchEnd));
}

static bool stFeatureSegment_rightAdjacencyIsWildCard(stFeatureSegment *fSegment, int64_t columnIndex) {
    return columnIndex < fSegment->length - 1 ? 0 : fSegment->nPinchEnd.block == NULL;
}

static stFeatureBlock *stFeatureBlock_construct(stFeatureSegment *firstSegment, stPinchBlock *block, int64_t totalNumSegments) {
    stFeatureBlock *featureBlock = st_malloc(sizeof(stFeatureBlock));
    featureBlock->segments = stList_construct2(totalNumSegments);
    stList_set(featureBlock->segments, firstSegment->segmentIndex,
               firstSegment);
    featureBlock->head = firstSegment;
    featureBlock->tail = firstSegment;
    firstSegment->nFeatureSegment = NULL; //Defensive assignment.
    featureBlock->length = stPinchBlock_getLength(block);
    return featureBlock;
}

void stFeatureBlock_destruct(stFeatureBlock *featureBlock) {
    stFeatureSegment_destruct(featureBlock->head);
    stList_destruct(featureBlock->segments);
    free(featureBlock);
}

/*
 * Finds the 5' most segment in the thread that contains bases within maxBaseDistance of the mid point of the segment, and within
 * maxBlockDistance.
 */
static stPinchSegment *get5PrimeMostSegment(stPinchSegment *segment, int64_t *baseDistance, int64_t *blockDistance,
        int64_t maxBaseDistance, int64_t maxBlockDistance,
        bool ignoreUnalignedBases, stPinchBlock *endBlock) {
    *baseDistance = -stPinchSegment_getLength(segment) / 2;
    *blockDistance = 0;
    while (stPinchSegment_get5Prime(segment) != NULL && -*baseDistance < maxBaseDistance
           && -*blockDistance < maxBlockDistance
           && (endBlock == NULL || stPinchSegment_getBlock(segment) != endBlock)) {
        segment = stPinchSegment_get5Prime(segment);
        if (stPinchSegment_getBlock(segment) != NULL || !ignoreUnalignedBases) {
            *baseDistance -= stPinchSegment_getLength(segment);
            (*blockDistance)--;
        }
    }
    assert(segment != NULL);
    return segment;
}

/*
 * Finds the 3' most segment in the thread that contains bases within maxBaseDistance of the mid point of the segment, and within
 * maxBlockDistance.
 */
static stPinchSegment *get3PrimeMostSegment(stPinchSegment *segment, int64_t *baseDistance, int64_t *blockDistance,
        int64_t maxBaseDistance, int64_t maxBlockDistance,
        bool ignoreUnalignedBases,
        stPinchBlock *endBlock) {
    *baseDistance = stPinchSegment_getLength(segment) / 2;
    *blockDistance = 0;
    while (stPinchSegment_get3Prime(segment) != NULL && *baseDistance < maxBaseDistance
           && *blockDistance < maxBlockDistance
           && (endBlock == NULL || stPinchSegment_getBlock(segment) != endBlock)) {
        segment = stPinchSegment_get3Prime(segment);
        if (stPinchSegment_getBlock(segment) != NULL || !ignoreUnalignedBases) {
            *baseDistance += stPinchSegment_getLength(segment);
            (*blockDistance)++;
        }
    }
    assert(segment != NULL);
    return segment;
}

/*
 * Gets the number of distinct segment indices the featureBlock contains. Relies on the indices of segments being sorted
 * (as they are by stFeatureBlock_getContextualFeatureBlocks)
 */
static int64_t countDistinctIndices(stFeatureBlock *featureBlock) {
    stFeatureSegment *segment = featureBlock->head;
    int64_t i = 1, j = segment->segmentIndex;
    while ((segment = segment->nFeatureSegment) != NULL) {
        if (segment->segmentIndex != j) {
            i++;
            j = segment->segmentIndex;
        }
    }
    return i;
}

// Appends a segment to a feature block.
static void stFeatureBlock_appendSegment(stFeatureBlock *featureBlock, stFeatureSegment *fSegment) {
    stList_set(featureBlock->segments, fSegment->segmentIndex, fSegment);
    featureBlock->tail->nFeatureSegment = fSegment;
    featureBlock->tail = fSegment;
}

// Gets a hash mapping from the segments within the list of chained
// blocks to the segment index within the reference (first) block.
static stHash *getSegmentToReferenceBlockIndex(stList *blocks) {
    stHash *segmentToReferenceBlockIndex = stHash_construct2(NULL, (void (*)(void *)) stIntTuple_destruct);
    stPinchBlock *referenceBlock = stList_get(blocks, 0);
    stPinchBlock *lastBlock = stList_get(blocks, stList_length(blocks) - 1);
    stPinchBlockIt it = stPinchBlock_getSegmentIterator(referenceBlock);
    stPinchSegment *segment;
    int64_t i = 0;
    while ((segment = stPinchBlockIt_getNext(&it)) != NULL) {
        bool orientation = stPinchSegment_getBlockOrientation(segment);
        for (;;) {
            stHash_insert(segmentToReferenceBlockIndex, segment, stIntTuple_construct1(i));
            if (stPinchSegment_getBlock(segment) == lastBlock) {
                // We've traversed this segment's path through the chain.
                break;
            }
            segment = orientation ? stPinchSegment_get3Prime(segment) : stPinchSegment_get5Prime(segment);
            // If the segment is NULL we've hit the end of the thread
            // before we hit the end of the chain--there is an error
            // in the input somewhere.
            assert(segment != NULL);
        }
        i++;
    }
    return segmentToReferenceBlockIndex;
}

// Construct feature blocks from a list of chained blocks.
static void addFeatureBlocksFromBlocks(stList *blocks,
                                       stHash *segmentToReferenceBlockIndex,
                                       stHash *blocksToFeatureBlocks,
                                       stHash *segmentsToFeatureSegments,
                                       stHash *strings) {
    for (int64_t i = 0; i < stList_length(blocks); i++) {
        stPinchBlock *block = stList_get(blocks, i);
        stPinchBlockIt it = stPinchBlock_getSegmentIterator(block);
        stPinchSegment *segment;
        while ((segment = stPinchBlockIt_getNext(&it)) != NULL) {
            stIntTuple *segmentIndex = stHash_search(segmentToReferenceBlockIndex, segment);
            assert(segmentIndex != NULL);
            stFeatureSegment *fSegment = stHash_search(segmentsToFeatureSegments, segment);
            if (fSegment == NULL) {
                //Make a new feature segment
                fSegment = stFeatureSegment_construct(segment, strings, stIntTuple_get(segmentIndex, 0), 0);
                //Attach it to the related block.
                stFeatureBlock *featureBlock = stHash_search(blocksToFeatureBlocks, block);
                if (featureBlock == NULL) { //Create a new feature block
                    featureBlock = stFeatureBlock_construct(fSegment, block, stPinchBlock_getDegree(block));
                    stHash_insert(blocksToFeatureBlocks, block, featureBlock);
                } else { //Link it to the existing feature block
                    stFeatureBlock_appendSegment(featureBlock, fSegment);
                }
                stHash_insert(segmentsToFeatureSegments, segment, fSegment);
            } else {
                assert(segment == fSegment->segment);
                assert(stPinchSegment_getLength(segment) == fSegment->length);
                assert(fSegment->distance != 0); //It is not possible for these two distances to be equal.
                //If the midpoint of segment is closer to the mid point of segment than the midpoint of featureSegment->segment, then switch
                if (llabs(fSegment->distance + fSegment->length / 2)
                    > llabs(fSegment->length / 2)) {
                    fSegment->segmentIndex = stIntTuple_get(segmentIndex, 0);
                    fSegment->distance = 0;
                }
            }
        }
    }
}

// Construct feature blocks from the blocks near this block, in the
// "left" direction if orientation is false and in the "right"
// direction if orientation is true.
static void addFeatureBlocksExtendingFromBlock(
    stPinchBlock *block,
    stHash *segmentToReferenceBlockIndex,
    stHash *blocksToFeatureBlocks,
    stHash *segmentsToFeatureSegments,
    int64_t maxBaseDistance,
    int64_t maxBlockDistance,
    bool ignoreUnalignedBases,
    stHash *strings,
    stPinchBlock *endBlock,
    bool orientation) {
    stPinchBlockIt it = stPinchBlock_getSegmentIterator(block);
    stPinchSegment *segment;

    while ((segment = stPinchBlockIt_getNext(&it)) != NULL) { //For each segment build the feature segment.
        int64_t blockDistance, baseDistance; //Distances from curSegment to segment
        //Base distance is the difference from the start coordinate of curSegment to the mid point of the segment.

        // Find the limiting segment, which is at the maximal allowed
        // distance from the original segment, and work back toward
        // the original segment from there.
        stPinchSegment *curSegment;
        if (stPinchSegment_getBlockOrientation(segment) ^ orientation) {
            curSegment = get5PrimeMostSegment(segment, &baseDistance,
                                              &blockDistance, maxBaseDistance,
                                              maxBlockDistance,
                                              ignoreUnalignedBases, endBlock);
        } else {
            curSegment = get3PrimeMostSegment(segment, &baseDistance,
                                              &blockDistance, maxBaseDistance,
                                              maxBlockDistance,
                                              ignoreUnalignedBases, endBlock);
        }
        while (curSegment != segment) {
            stPinchBlock *curBlock = stPinchSegment_getBlock(curSegment);
            if (curBlock != NULL) {
                stIntTuple *segmentIndex = stHash_search(segmentToReferenceBlockIndex, segment);
                assert(segmentIndex != NULL);
                stFeatureSegment *fSegment = stHash_search(segmentsToFeatureSegments, curSegment);
                if (fSegment == NULL) {
                    //Make a new feature segment
                    fSegment = stFeatureSegment_construct(curSegment, strings, stIntTuple_get(segmentIndex, 0), baseDistance);
                    //Attach it to the related block.
                    stFeatureBlock *featureBlock = stHash_search(blocksToFeatureBlocks, curBlock);
                    if (featureBlock == NULL) { //Create a new feature block
                        featureBlock = stFeatureBlock_construct(fSegment, curBlock, stPinchBlock_getDegree(block));
                        stHash_insert(blocksToFeatureBlocks, curBlock, featureBlock);
                    } else { //Link it to the existing feature block
                        stFeatureBlock_appendSegment(featureBlock, fSegment);
                    }
                    stHash_insert(segmentsToFeatureSegments, curSegment, fSegment);
                } else {
                    assert(curSegment == fSegment->segment);
                    assert(stPinchSegment_getLength(curSegment) == fSegment->length);
                    assert(fSegment->distance != baseDistance); //It is not possible for these two distances to be equal.
                    //If the midpoint of curSegment is closer to the mid point of segment than the midpoint of featureSegment->segment, then switch
                    if (llabs(fSegment->distance + fSegment->length / 2)
                            > llabs(baseDistance + fSegment->length / 2)) {
                        fSegment->segmentIndex = stIntTuple_get(segmentIndex, 0);
                        fSegment->distance = baseDistance;
                    }
                }
            }
            if (stPinchSegment_getBlock(curSegment) != NULL || !ignoreUnalignedBases) { //Only add unaligned segments if not ignoring such segments.
                if (stPinchSegment_getBlockOrientation(segment) ^ orientation) {
                    baseDistance += stPinchSegment_getLength(curSegment);
                    blockDistance++;
                } else {
                    baseDistance -= stPinchSegment_getLength(curSegment);
                    blockDistance--;
                }
            }
            if (stPinchSegment_getBlockOrientation(segment) ^ orientation) {
                curSegment = stPinchSegment_get3Prime(curSegment);
            } else {
                curSegment = stPinchSegment_get5Prime(curSegment);
            }
        }
    }
}

static stList *stFeatureBlock_getContextualFeatureBlocksForChainedBlocks_private(
    stList *blocks, int64_t maxBaseDistance,
    int64_t maxBlockDistance, bool ignoreUnalignedBases,
    bool onlyIncludeCompleteFeatureBlocks, stHash *strings,
    stHash *blocksToFeatureBlocks) {
    /*
     * First build the set of feature blocks, not caring if this produces trivial blocks.
     */
    stHash *segmentsToFeatureSegments = stHash_construct(); //Hash to map segments to featureSegments, to remove overlaps.
    stHash *segmentToReferenceBlockIndex = getSegmentToReferenceBlockIndex(blocks);
    // Get the feature blocks within the chain.
    addFeatureBlocksFromBlocks(blocks,
                               segmentToReferenceBlockIndex,
                               blocksToFeatureBlocks,
                               segmentsToFeatureSegments,
                               strings);

    // Within the chain, get the feature blocks extending from the end and beginning of each block.
    for (int64_t i = 0; i < stList_length(blocks); i++) {
        stPinchBlock *block = stList_get(blocks, i);
        stPinchBlock *prevBlock = i == 0 ? NULL : stList_get(blocks, i - 1);
        stPinchBlock *nextBlock = i == stList_length(blocks) - 1 ? NULL : stList_get(blocks, i + 1);
        // Get the feature blocks to the left of the first block in the chain.
        addFeatureBlocksExtendingFromBlock(block,
                                           segmentToReferenceBlockIndex,
                                           blocksToFeatureBlocks,
                                           segmentsToFeatureSegments,
                                           maxBaseDistance,
                                           maxBlockDistance,
                                           ignoreUnalignedBases,
                                           strings,
                                           prevBlock,
                                           false);
        // Get the feature blocks to the right of the last block in the chain.
        addFeatureBlocksExtendingFromBlock(block,
                                           segmentToReferenceBlockIndex,
                                           blocksToFeatureBlocks,
                                           segmentsToFeatureSegments,
                                           maxBaseDistance,
                                           maxBlockDistance,
                                           ignoreUnalignedBases,
                                           strings,
                                           nextBlock,
                                           true);
    }
    stHash_destruct(segmentsToFeatureSegments);
    stHash_destruct(segmentToReferenceBlockIndex);
    /*
     * Now filter the set of blocks so that only desired blocks are present.
     */
    stList *unfilteredFeatureBlocks = stHash_getValues(blocksToFeatureBlocks);
    stList *featureBlocks = stList_construct3(0, (void (*)(void *)) stFeatureBlock_destruct);
    for (int64_t i = 0; i < stList_length(unfilteredFeatureBlocks); i++) {
        stFeatureBlock *featureBlock = stList_get(unfilteredFeatureBlocks, i);
        assert(featureBlock != NULL);
        if (!onlyIncludeCompleteFeatureBlocks || countDistinctIndices(featureBlock) == stPinchBlock_getDegree(stList_get(blocks, 0))) {
            stList_append(featureBlocks, featureBlock);
        } else {
            stFeatureBlock_destruct(featureBlock); //This is a trivial/unneeded feature block, so remove.
        }
    }
    stList_destruct(unfilteredFeatureBlocks);

    return featureBlocks;
}

stList *stFeatureBlock_getContextualBlocksForChainedBlocks(
    stList *blocks, int64_t maxBaseDistance,
    int64_t maxBlockDistance, bool ignoreUnalignedBases,
    bool onlyIncludeCompleteFeatureBlocks, stHash *strings) {
    stHash *blocksToFeatureBlocks = stHash_construct();
    stList *featureBlocks = stFeatureBlock_getContextualFeatureBlocksForChainedBlocks_private(
        blocks, maxBaseDistance, maxBlockDistance, ignoreUnalignedBases,
        onlyIncludeCompleteFeatureBlocks, strings, blocksToFeatureBlocks);
    stList_destruct(featureBlocks);
    stList *contextualBlocks = stHash_getKeys(blocksToFeatureBlocks);
    stHash_destruct(blocksToFeatureBlocks);
    return contextualBlocks;
}

stList *stFeatureBlock_getContextualFeatureBlocksForChainedBlocks(
    stList *blocks, int64_t maxBaseDistance,
    int64_t maxBlockDistance, bool ignoreUnalignedBases,
    bool onlyIncludeCompleteFeatureBlocks, stHash *strings) {
    stHash *blocksToFeatureBlocks = stHash_construct();
    stList *ret = stFeatureBlock_getContextualFeatureBlocksForChainedBlocks_private(
        blocks, maxBaseDistance, maxBlockDistance, ignoreUnalignedBases,
        onlyIncludeCompleteFeatureBlocks, strings, blocksToFeatureBlocks);
    stHash_destruct(blocksToFeatureBlocks);
    return ret;
}

stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t maxBaseDistance,
        int64_t maxBlockDistance,
        bool ignoreUnalignedBases, bool onlyIncludeCompleteFeatureBlocks, stHash *strings) {
    stList *blocks = stList_construct();
    stList_append(blocks, block);
    stList *featureBlocks = stFeatureBlock_getContextualFeatureBlocksForChainedBlocks(blocks, maxBaseDistance, maxBlockDistance, ignoreUnalignedBases, onlyIncludeCompleteFeatureBlocks, strings);
    stList_destruct(blocks);
    return featureBlocks;
}

stList *stFeatureBlock_getContextualBlocks(stPinchBlock *block, int64_t maxBaseDistance,
        int64_t maxBlockDistance,
        bool ignoreUnalignedBases, bool onlyIncludeCompleteFeatureBlocks, stHash *strings) {
    stList *blocks = stList_construct();
    stList_append(blocks, block);
    stList *contextualBlocks = stFeatureBlock_getContextualBlocksForChainedBlocks(blocks, maxBaseDistance, maxBlockDistance, ignoreUnalignedBases, onlyIncludeCompleteFeatureBlocks, strings);
    stList_destruct(blocks);
    return contextualBlocks;
}

stFeatureColumn *stFeatureColumn_construct(stFeatureBlock *featureBlock, int64_t columnIndex) {
    stFeatureColumn *featureColumn = st_malloc(sizeof(stFeatureColumn));
    featureColumn->featureBlock = featureBlock;
    featureColumn->columnIndex = columnIndex;
    return featureColumn;
}

void stFeatureColumn_destruct(stFeatureColumn *featureColumn) {
    free(featureColumn); //Does not own the block.
}

stList *stFeatureColumn_getFeatureColumns(stList *featureBlocks) {
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
 * Gets a list of matrix diffs (one per column) by iterating through
 * the feature columns and adding in features of the column, according
 * to the given function.
 */
static void addMatrixDiffs(stMatrixDiffs *matrixDiffs, stList *featureColumns,
                           double (*distanceWeightFn)(int64_t, int64_t),
                           bool (*equalFn)(stFeatureSegment *, stFeatureSegment *, int64_t),
                           bool (*nullFeature)(stFeatureSegment *, int64_t)) {
    for (int64_t i = 0; i < stList_length(featureColumns); i++) {
        stFeatureColumn *featureColumn = stList_get(featureColumns, i);
        stFeatureSegment *fSegment = featureColumn->featureBlock->head;
        int64_t distance1 = stFeatureSegment_getColumnDistance(fSegment, featureColumn->columnIndex);
        stList *ops = stList_construct3(0, free);
        while (fSegment != NULL) {
            if (!nullFeature(fSegment, featureColumn->columnIndex)) {
                stFeatureSegment *nSegment = fSegment->nFeatureSegment;
                while (nSegment != NULL) {
                    if (!nullFeature(nSegment, featureColumn->columnIndex)) {
                        int64_t j = fSegment->segmentIndex, k = nSegment->segmentIndex;
                        if (j != k) {
                            if (j > k) { //Swap the ordering of the indices so that j < k.
                                int64_t l = j;
                                j = k;
                                k = l;
                            }
                            assert(j < k);
                            int64_t distance2 = stFeatureSegment_getColumnDistance(nSegment,
                                    featureColumn->columnIndex);
                            stMatrixOp *op = st_malloc(sizeof(stMatrixOp));
                            if (equalFn(fSegment, nSegment, featureColumn->columnIndex)) {
                                op->row = j;
                                op->col = k;
                                op->diff = distanceWeightFn(distance1, distance2);
                            } else {
                                op->row = k;
                                op->col = j;
                                op->diff = distanceWeightFn(distance1, distance2);
                            }
                            stList_append(ops, op);
                        }
                    }
                    nSegment = nSegment->nFeatureSegment;
                }
            }
            fSegment = fSegment->nFeatureSegment;
        }
        stList_append(matrixDiffs->diffs, ops);
    }
}

double stPinchPhylogeny_constantDistanceWeightFn(int64_t i, int64_t j) {
    return 1;
}

static stMatrixDiffs *stMatrixDiffs_construct(int64_t rows, int64_t cols) {
    stMatrixDiffs *ret = st_malloc(sizeof(stMatrixDiffs));
    ret->rows = rows;
    ret->cols = cols;
    stList *diffs = stList_construct3(0, (void (*)(void *)) stList_destruct);
    ret->diffs = diffs;
    return ret;
}

// Do a pseudo-bootstrapping of a list of matrix diffs using the binomial
// distribution to determine whether to add/subtract a diff or not
// from the canonical matrix. Should be faster than using "sample" in
// constructMatrixFromDiffs.
stMatrix *stPinchPhylogeny_bootstrapMatrixWithDiffs(stMatrix *canonicalMatrix,
                                                    stMatrixDiffs *matrixDiffs)
{
    stMatrix *ret = stMatrix_clone(canonicalMatrix);
    assert(matrixDiffs->rows == matrixDiffs->cols);
    assert(matrixDiffs->rows == stMatrix_m(canonicalMatrix));
    assert(stMatrix_m(canonicalMatrix) == stMatrix_n(canonicalMatrix));
    // Initialize the inverted distribution. We have the advantage of
    // knowing that values higher than about 8 or so are going to be
    // exceedingly rare.
    int64_t n = matrixDiffs->rows;
    double p = 1.0/n;
    double *invertedBinom = st_malloc(BINOM_CEILING * sizeof(double));
    double prev = 0.0;
    double choose = 1.0;
    for (int64_t i = 0; i < BINOM_CEILING; i++) {
        if (i > 0) {
            choose *= ((double) (n + 1 - i))/i;
        }
        if (i <= n) {
            invertedBinom[i] = prev + choose * pow(n, i) * pow(1 - p, n - i);
        } else {
            invertedBinom[i] = 1.0;
        }
        prev = invertedBinom[i];
    }
    for (int64_t i = 0; i < stList_length(matrixDiffs->diffs); i++) {
        // Sample from the inverted distribution to see how many times
        // this column appears in the bootstrap. This is
        // O(BINOM_CEILING), but we don't do a binary search as
        // BINOM_CEILING is likely to be very small and this loop can be
        // very easily unrolled.
        double invertedProb = st_random();
        int64_t numAppearances = BINOM_CEILING;
        for (int64_t j = 0; j < BINOM_CEILING; j++) {
            if (invertedProb <= invertedBinom[j]) {
                numAppearances = j;
            }
        }
        if (numAppearances != 1) {
            // We have to add (or subtract) the influence from this
            // column in the correct proportions.
            stList *ops = stList_get(matrixDiffs->diffs, i);
            int64_t numOps = stList_length(ops);
            for (int64_t j = 0; j < numOps; j++) {
                stMatrixOp *op = stList_get(ops, j);
                assert(op->row < matrixDiffs->rows);
                assert(op->col < matrixDiffs->cols);
                *stMatrix_getCell(ret, op->row, op->col) += op->diff * (numAppearances - 1);
            }
        }
    }
    return ret;
}

stMatrix *stPinchPhylogeny_constructMatrixFromDiffs(stMatrixDiffs *matrixDiffs,
                                                    bool sample,
                                                    unsigned int *seed) {
    stMatrix *matrix = stMatrix_construct(matrixDiffs->rows, matrixDiffs->cols);
    int64_t numDiffs = stList_length(matrixDiffs->diffs);
    for (int64_t i = 0; i < numDiffs; i++) {
        stList *ops = stList_get(matrixDiffs->diffs, sample ? rand_r(seed) % numDiffs : i);
        for (int64_t j = 0; j < stList_length(ops); j++) {
            stMatrixOp *op = stList_get(ops, j);
            assert(op->row < matrixDiffs->rows);
            assert(op->col < matrixDiffs->cols);
            *stMatrix_getCell(matrix, op->row, op->col) += op->diff;
        }
    }
    return matrix;
}

void stMatrixDiffs_destruct(stMatrixDiffs *diffs) {
    stList_destruct(diffs->diffs);
    free(diffs);
}

stMatrix *stPinchPhylogeny_getMatrixFromSubstitutions(stList *featureColumns, int64_t degree,
                                                      double distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    unsigned int mySeed = rand();
    stMatrixDiffs *diffs = stPinchPhylogeny_getMatrixDiffsFromSubstitutions(featureColumns, degree, distanceWeightFn);
    stMatrix *matrix = stPinchPhylogeny_constructMatrixFromDiffs(diffs, sampleColumns, &mySeed);
    stMatrixDiffs_destruct(diffs);
    return matrix;
}

stMatrixDiffs *stPinchPhylogeny_getMatrixDiffsFromSubstitutions(stList *featureColumns, int64_t degree,
        double distanceWeightFn(int64_t, int64_t)) {
    stMatrixDiffs *diffs = stMatrixDiffs_construct(degree, degree);
    addMatrixDiffs(diffs, featureColumns, distanceWeightFn == NULL ? stPinchPhylogeny_constantDistanceWeightFn : distanceWeightFn, stFeatureSegment_basesEqual,
            stFeatureSegment_baseIsWildCard);
    return diffs;
}

stMatrix *stPinchPhylogeny_getMatrixFromBreakpoints(stList *featureColumns, int64_t degree,
                                                    double distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    unsigned int mySeed = rand();
    stMatrixDiffs *diffs = stPinchPhylogeny_getMatrixDiffsFromBreakpoints(featureColumns, degree, distanceWeightFn);
    stMatrix *matrix = stPinchPhylogeny_constructMatrixFromDiffs(diffs, sampleColumns, &mySeed);
    stMatrixDiffs_destruct(diffs);
    return matrix;
}

stMatrixDiffs *stPinchPhylogeny_getMatrixDiffsFromBreakpoints(stList *featureColumns, int64_t degree,
        double distanceWeightFn(int64_t, int64_t)) {
    stMatrixDiffs *diffs = stMatrixDiffs_construct(degree,
                                                   degree);
    addMatrixDiffs(diffs, featureColumns, distanceWeightFn == NULL ? stPinchPhylogeny_constantDistanceWeightFn : distanceWeightFn, stFeatureSegment_leftAdjacenciesEqual,
            stFeatureSegment_leftAdjacencyIsWildCard);
    addMatrixDiffs(diffs, featureColumns, distanceWeightFn == NULL ? stPinchPhylogeny_constantDistanceWeightFn : distanceWeightFn, stFeatureSegment_rightAdjacenciesEqual,
            stFeatureSegment_rightAdjacencyIsWildCard);
    return diffs;
}

/*
 * Gets a symmetric distance matrix representing the feature matrix.
 */
stMatrix *stPinchPhylogeny_getSymmetricDistanceMatrix(stMatrix *matrix) {
    assert(stMatrix_n(matrix) == stMatrix_m(matrix));
    stMatrix *distanceMatrix = stMatrix_construct(stMatrix_n(matrix), stMatrix_m(matrix));
    for (int64_t i = 0; i < stMatrix_n(matrix); i++) {
        for (int64_t j = i + 1; j < stMatrix_n(matrix); j++) {
            double similarities = *stMatrix_getCell(matrix, i, j); //The similarity count
            double differences = *stMatrix_getCell(matrix, j, i); //The difference count
            double count = similarities + differences;
            *stMatrix_getCell(distanceMatrix, i, j) = (count != 0.0) ? differences / count : INT64_MAX;
            *stMatrix_getCell(distanceMatrix, j, i) = *stMatrix_getCell(distanceMatrix, i, j);
        }
    }
    return distanceMatrix;
}

// Returns a new tree with poorly-supported partitions removed. The
// tree must have bootstrap-support information for every node.
stTree *stPinchPhylogeny_removePoorlySupportedPartitions(stTree *tree, double threshold) {
    int64_t i, j;
    stTree *ret = stTree_cloneNode(tree);
    stPhylogenyInfo *info = stPhylogenyInfo_clone(stTree_getClientData(tree));
    stTree_setClientData(ret, info);
    for (i = 0; i < stTree_getChildNumber(tree); i++) {
        stTree *child = stTree_getChild(tree, i);
        stTree_setParent(stPinchPhylogeny_removePoorlySupportedPartitions(child, threshold), ret);
    }
    for (i = 0; i < stTree_getChildNumber(ret); i++) {
        stTree *child = stTree_getChild(ret, i);
        stPhylogenyInfo *childInfo = stTree_getClientData(child);
        if (childInfo->index->bootstrapSupport < threshold) {
            int64_t numGrandChildren = stTree_getChildNumber(child);
            // The bootstrap support for this child is poor, so make
            // all grandchildren (if any) into children instead.
            for (j = 0; j < stTree_getChildNumber(child); j++) {
                stTree *grandChild = stTree_getChild(child, j);
                stTree_setParent(grandChild, ret);
                // hacky, but the grandchild number has decreased by 1
                j--;
            }
            // If there were any grandchildren, then get rid of the
            // useless node entirely. If it's a leaf, it must be kept.
            if (numGrandChildren != 0) {
                stTree_setParent(child, NULL);
                // hacky, but the child number has decreased by 1
                i--;
                stPhylogenyInfo_destructOnTree(child);
                stTree_destruct(child);
            }
        }
    }
    return ret;
}

// Returns a list of maximal subtrees that contain only ingroups
static stList *findIngroupClades(stTree *tree, stList *outgroups) {
    int64_t i, j;
    stList *ret = stList_construct();
    bool noChildrenHaveOutgroups = stTree_getChildNumber(tree) != 0;
    for(i = 0; i < stTree_getChildNumber(tree); i++) {
        // check if this node has any outgroups below it.
        stTree *child = stTree_getChild(tree, i);
        stPhylogenyInfo *childInfo = stTree_getClientData(child);
        bool hasOutgroup = false;
        for(j = 0; j < stList_length(outgroups); j++) {
            int64_t outgroup = stIntTuple_get(stList_get(outgroups, j), 0);
            assert(outgroup < childInfo->index->totalNumLeaves);
            if(childInfo->index->leavesBelow[outgroup]) {
                hasOutgroup = true;
                break;
            }
        }
        if(hasOutgroup) {
            stList *subTrees = findIngroupClades(child, outgroups);
            stList_appendAll(ret, subTrees);
            stList_destruct(subTrees);
            noChildrenHaveOutgroups = false;
        } else {
            stList_append(ret, child);
        }
    }
    // A little screwy, but it's possible for the tree to have no
    // children that have outgroups if this is the root and there are
    // no outgroups in the tree. The contents of ret will be
    // partitioned incorrectly in this case, so it's fixed here.
    if(noChildrenHaveOutgroups) {
        assert(stTree_getParent(tree) == NULL);
        // Safe to leave the contents unfreed since the tree's memory
        // is accounted for by the root
        stList_destruct(ret);
        ret = stList_construct();
        stList_append(ret, tree);
    }
    return ret;
}

// Split tree into subtrees such that no ingroup has a path to another
// ingroup that includes its MRCA with an outgroup
// Returns leaf sets (as stLists of length-1 stIntTuples) from the subtrees
// Outgroups should be a list of length-1 stIntTuples representing matrix indices
stList *stPinchPhylogeny_splitTreeOnOutgroups(stTree *tree, stList *outgroups) {
    int64_t i, j;
    stList *clades = findIngroupClades(tree, outgroups);
    stList *ret = stList_construct3(0, (void (*)(void *))stList_destruct);
    stPhylogenyInfo *info = stTree_getClientData(tree);

    if(stList_length(outgroups) == info->index->totalNumLeaves) {
        // Pathological case -- all leaves are outgroups.
        stList *inner = stList_construct3(0, (void (*)(void *))stIntTuple_destruct);
        for(i = 0; i < stList_length(outgroups); i++) {
            stList_append(inner, stIntTuple_construct1(i));
        }
        stList_append(ret, inner);
        return ret;
    }

    // Create the initial leaf sets from the ingroup clades.
    for(i = 0; i < stList_length(clades); i++) {
        stTree *clade = stList_get(clades, i);
        stPhylogenyInfo *cladeInfo = stTree_getClientData(clade);
        stList *leafSet = stList_construct3(0, (void (*)(void *))stIntTuple_destruct);
        for(j = 0; j < cladeInfo->index->totalNumLeaves; j++) {
            if(cladeInfo->index->leavesBelow[j]) {
                stIntTuple *jTuple = stIntTuple_construct1(j);
                stList_append(leafSet, jTuple);
            }
        }
        stList_append(ret, leafSet);
    }

    for(i = 0; i < stList_length(outgroups); i++) {
        int64_t outgroup = stIntTuple_get(stList_get(outgroups, i), 0);
        stTree *outgroupNode = stPhylogeny_getLeafByIndex(tree, outgroup);
        // Find the closest clade to this outgroup and attach the
        // outgroup to its returned leaf set
        double minDistance = -1.0;
        stList *closestSet = NULL;
        for(j = 0; j < stList_length(clades); j++) {
            stTree *clade = stList_get(clades, j);
            stList *returnedSet = stList_get(ret, j);
            double dist = stPhylogeny_distanceBetweenNodes(outgroupNode, clade);
            if(dist < minDistance || minDistance == -1.0) {
                    closestSet = returnedSet;
                    minDistance = dist;
            }
        }
        assert(closestSet != NULL);
        stList_append(closestSet, stIntTuple_construct1(outgroup));
    }

    stList_destruct(clades);
    return ret;
}

// get an index from an unambiguous base for felsenstein likelihood
static int64_t dnaToIndex(char dna) {
    switch(dna) {
    case 'A':
    case 'a':
        return 0;
    case 'T':
    case 't':
        return 1;
    case 'G':
    case 'g':
        return 2;
    case 'C':
    case 'c':
        return 3;
    default:
        // Should not get here
        return INT64_MAX;
    }
}

static stFeatureSegment *stFeatureBlock_getFeatureSegment(stFeatureBlock *block, int64_t index) {
    return stList_get(block->segments, index);
}

// Jukes-Cantor probability of a mutation along a branch
static double jukesCantor(int64_t dnaIndex1, int64_t dnaIndex2,
                          double branchLength) {
    if(dnaIndex1 == dnaIndex2) {
        return 0.25+0.75*exp(-branchLength);
    } else {
        return 0.25-0.25*exp(-branchLength);
    }
}

// Calculate the nucleotide portion of the likelihood of a given column.
static double likelihoodNucColumn(stTree *tree, stFeatureColumn *column) {
    // Need to modify the client info, so we need to clone the tree
    tree = stTree_clone(tree);

    // breadth-first (bottom-up)
    stList *bfQueue = stList_construct();

    // populate queue
    stList_append(bfQueue, tree);
    for(int64_t i = 0; i < stList_length(bfQueue); i++) {
        stTree *node = stList_get(bfQueue, i);
        for(int64_t j = 0; j < stTree_getChildNumber(node); j++) {
            stList_append(bfQueue, stTree_getChild(node, j));
        }
    }

    assert(stList_length(bfQueue) == stTree_getNumNodes(tree));

    // pop off the end
    while(stList_length(bfQueue) != 0) {
        stTree *node = stList_pop(bfQueue);

        stPhylogenyInfo *info = stTree_getClientData(node);
        if(stTree_getChildNumber(node) == 0) {
            // Leaf node -- fill in its probability from its base at
            // this location
            double *pLeaves = calloc(4, sizeof(double));
            assert(info->index->matrixIndex != -1);
            stFeatureSegment *segment = stFeatureBlock_getFeatureSegment(column->featureBlock, info->index->matrixIndex);
            if(segment == NULL || stFeatureSegment_baseIsWildCard(segment, column->columnIndex)) {
                // leaf base is a wild card (or is not in the column
                // at all), all possibilities are equal
                for(int64_t dnaIndex = 0; dnaIndex < 4; dnaIndex++) {
                    pLeaves[dnaIndex] = 0.25;
                }
            } else {
                // P(actual leaf base) = 1, all other bases 0
                char dna = stFeatureSegment_getBase(segment, column->columnIndex);
                int64_t dnaIndex = dnaToIndex(dna);
                pLeaves[dnaIndex] = 1.0;
            }
            stTree_setClientData(node, pLeaves);
        } else {
            // Not a leaf node--calculate its probability from its
            // children
            assert(info->index->matrixIndex == -1);
            double *pLeaves = calloc(4, sizeof(double));
            for(int64_t dnaIndex = 0; dnaIndex < 4; dnaIndex++) {
                double prob = 0.25; // Probability for this dna assignment
                for(int64_t i = 0; i < stTree_getChildNumber(node); i++) {
                    double probChild = 0.0;
                    stTree *child = stTree_getChild(node, i);
                    double *childpLeaves = stTree_getClientData(child);
                    assert(childpLeaves != NULL);
                    for(int64_t childDnaIndex = 0; childDnaIndex < 4; childDnaIndex++) {
                        probChild += childpLeaves[childDnaIndex]*jukesCantor(dnaIndex, childDnaIndex, stTree_getBranchLength(child));
                    }
                    prob *= probChild;
                }
                pLeaves[dnaIndex] = prob;
            }
            stTree_setClientData(node, pLeaves);

            // Clean up the unneeded children data
            for(int64_t i = 0; i < stTree_getChildNumber(node); i++) {
                free(stTree_getClientData(stTree_getChild(node, i)));
            }
        }
    }

    // Get p(tree)
    double *rootProbs = stTree_getClientData(tree);
    double ret = rootProbs[0] + rootProbs[1] + rootProbs[2] + rootProbs[3];

    // Clean up
    free(rootProbs);
    stTree_destruct(tree); // Delete our copy
    stList_destruct(bfQueue);
    return ret;
}

// Gives the (log)likelihood of the tree given the feature columns.
double stPinchPhylogeny_likelihood(stTree *tree, stList *featureColumns) {
    double ret = 0.0;

    // Likelihood of each column -- nucleotide portion
    for(int64_t i = 0; i < stList_length(featureColumns); i++) {
        ret += log(likelihoodNucColumn(tree, stList_get(featureColumns, i)))/log(10);
    }

    return ret;
}

double poisson(int numOccurrences, double rate) {
    return pow(numOccurrences, rate)*exp(-rate)/tgamma(numOccurrences + 1);
}

// For a tree with stReconciliationInfo on the internal nodes, assigns
// a log-likelihood to the events in the tree. Uses an approximation that
// considers only duplication likelihood, rather than the full
// birth-death process.
// Dup-rate is per species-tree branch length unit.
double stPinchPhylogeny_reconciliationLikelihood(stTree *tree, stTree *speciesTree, double dupRate) {
    // Create a species->duplication events mapping so we can get the number of
    // duplications per (species) lineage.
    stHash *speciesToDups = stHash_construct2(NULL, (void (*)(void *)) stList_destruct);
    stList *bfQueue = stList_construct();
    stList_append(bfQueue, tree);
    while (stList_length(bfQueue) != 0) {
        stTree *gene = stList_pop(bfQueue);
        for (int64_t i = 0; i < stTree_getChildNumber(gene); i++) {
            stList_append(bfQueue, stTree_getChild(gene, i));
        }
        stPhylogenyInfo *info = stTree_getClientData(gene);
        assert(info != NULL);
        stReconciliationInfo *recon = info->recon;
        assert(recon != NULL);
        stTree *species = recon->species;
        assert(species != NULL);
        stList *dupList = stHash_search(speciesToDups, species);
        if (dupList == NULL) {
            // Initialize dup list for a particular species.
            dupList = stList_construct();
            stHash_insert(speciesToDups, species, dupList);
        }
        if (recon->event == DUPLICATION) {
            // Duplication event, we should add it to the list.
            stList_append(dupList, gene);
        }
    }

    // Get the species tree rooted at the MRCA of the genes so we
    // don't account for lineages that don't actually have the gene in
    // question.
    stPhylogenyInfo *rootInfo = stTree_getClientData(tree);
    assert(rootInfo != NULL);
    stReconciliationInfo *rootRecon = rootInfo->recon;
    assert(rootRecon != NULL);
    speciesTree = rootRecon->species;
    assert(speciesTree != NULL);

    // Go through the species tree and add each branch's probability
    // of receiving x duplications to the likelihood.
    double logLikelihood = 0.0;
    stList_append(bfQueue, speciesTree);
    while (stList_length(bfQueue) != 0) {
        stTree *species = stList_pop(bfQueue);
        for (int64_t i = 0; i < stTree_getChildNumber(species); i++) {
            stList_append(bfQueue, stTree_getChild(species, i));
        }
        if (species == speciesTree) {
            // We ignore the species-tree root since it's difficult to
            // calculate the probability of x duplications in the root
            // -- we aren't sure of the branch length.
            continue;
        }
	stList *dups = stHash_search(speciesToDups, species);
        bool isLost = true;
        int64_t numDups = 0;
        if (dups == NULL) {
            // If the dupe list isn't created, then we didn't see
            // anything reconciled to this node. We need to check
            // whether the gene has been lost in this node, if not, we
            // should assign it zero dups.
            stList *bfQueue2 = stList_construct();
            stList_append(bfQueue2, species);
            // Look in all the children in the subtree under species
            // and see if there are any that have been reconciled to
            // it (if not, then we can safely assume the gene is lost
            // in this lineage)
            while (stList_length(bfQueue2) != 0) {
                stTree *childSpecies = stList_pop(bfQueue2);
                for (int64_t i = 0; i < stTree_getChildNumber(childSpecies); i++) {
                    stList_append(bfQueue2, stTree_getChild(childSpecies, i));
                }
                if (stHash_search(speciesToDups, childSpecies) != NULL) {
                    isLost = false;
                    break;
                }
            }
            if (!isLost) {
                numDups = 0;
            }
            stList_destruct(bfQueue2);
        } else {
            // Obviously this is not lost.
            isLost = false;
            numDups = stList_length(dups);
        }
        if (!isLost) {
            logLikelihood += log(poisson(numDups, dupRate * stTree_getBranchLength(species)))/log(10);
        }
    }

    stList_destruct(bfQueue);
    stHash_destruct(speciesToDups);
    return logLikelihood;
}
