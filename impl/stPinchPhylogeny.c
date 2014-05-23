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
#include "stPinchPhylogeny.h"
#include "stPhylogeny.h"

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
            stString_reverseComplement(featureSegment->string[featureSegment->length - 1 - columnIndex]) :
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
    return stFeatureSegment_getBase(fSegment1, columnIndex) == stFeatureSegment_getBase(fSegment2, columnIndex);
}

/*
 * Returns non-zero if the left adjacency of the segments is equal.
 */
static bool stFeatureSegment_leftAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2,
        int64_t columnIndex) {
    return columnIndex > 0 ?
            1 :
            (fSegment1->pPinchEnd.block == fSegment2->pPinchEnd.block
                    && fSegment1->pPinchEnd.orientation == fSegment2->pPinchEnd.orientation);
}

/*
 * Returns non-zero if the right adjacency of the segments is equal.
 */
static bool stFeatureSegment_rightAdjacenciesEqual(stFeatureSegment *fSegment1, stFeatureSegment *fSegment2,
        int64_t columnIndex) {
    return columnIndex < fSegment1->length - 1 ?
            1 :
            (fSegment1->pPinchEnd.block == fSegment2->pPinchEnd.block
                    && fSegment1->pPinchEnd.orientation == fSegment2->pPinchEnd.orientation);
}

static stFeatureBlock *stFeatureBlock_construct(stFeatureSegment *firstSegment, stPinchBlock *block) {
    stFeatureBlock *featureBlock = st_malloc(sizeof(featureBlock));
    featureBlock->head = firstSegment;
    featureBlock->tail = firstSegment;
    firstSegment->nFeatureSegment = NULL; //Defensive assignment.
    featureBlock->length = stPinchBlock_getLength(block);
    return featureBlock;
}

static void stFeatureBlock_destruct(stFeatureBlock *featureBlock) {
    stFeatureSegment_destruct(featureBlock->head);
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
    int64_t i = 1, j = segment->segmentIndex;
    while ((segment = segment->nFeatureSegment) != NULL) {
        if (segment->segmentIndex != j) {
            i++;
            segment->segmentIndex = j;
        }
    }
    return i;
}

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
        if (featureBlock->head != featureBlock->tail
                && (!onlyIncludeCompleteFeatureBlocks
                        || countDistinctIndices(featureBlock) == stPinchBlock_getDegree(block))) { //Is non-trivial
            stList_append(featureBlocks, featureBlock);
        } else {
            stFeatureBlock_destruct(featureBlock); //This is a trivial/unneeded feature block, so remove.
        }
    }
    stList_destruct(unfilteredFeatureBlocks);

    return featureBlocks;
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
 * Adds to a feature matrix by iterating through the feature columns and adding in features of the column, according to the given function.
 */
static void addFeaturesToMatrix(stMatrix *matrix, stList *featureColumns, double distanceWeightFn(int64_t, int64_t),
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
                    *stMatrix_getCell(matrix, fSegment->segmentIndex, nSegment->segmentIndex) = distanceWeightFn(
                            distance1, distance2);
                } else {
                    *stMatrix_getCell(matrix, fSegment->segmentIndex, nSegment->segmentIndex) = distanceWeightFn(
                            distance1, distance2);
                }
                nSegment = nSegment->nFeatureSegment;
            }
            fSegment = fSegment->nFeatureSegment;
        }
    }
}

stMatrix *stPinchPhylogeny_getMatrixFromSubstitutions(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    stMatrix *matrix = stMatrix_construct(stPinchBlock_getDegree(block), stPinchBlock_getDegree(block));
    addFeaturesToMatrix(matrix, featureColumns, distanceWeightFn, sampleColumns, stFeatureSegment_basesEqual);
    return matrix;
}

stMatrix *stPinchPhylogeny_getMatrixFromBreakPoints(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    stMatrix *matrix = stMatrix_construct(stPinchBlock_getDegree(block), stPinchBlock_getDegree(block));
    addFeaturesToMatrix(matrix, featureColumns, distanceWeightFn, sampleColumns, stFeatureSegment_leftAdjacenciesEqual);
    addFeaturesToMatrix(matrix, featureColumns, distanceWeightFn, sampleColumns,
            stFeatureSegment_rightAdjacenciesEqual);
    return matrix;
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
            *stMatrix_getCell(distanceMatrix, i, j) = ((double) differences) / (similarities + differences);
            *stMatrix_getCell(distanceMatrix, j, i) = *stMatrix_getCell(distanceMatrix, i, j);
        }
    }
    return distanceMatrix;
}

// Returns a new tree with poorly-supported partitions removed. The
// tree must have bootstrap-support information for every node.
stTree *removePoorlySupportedPartitions(stTree *tree,
                                        double threshold)
{
    int64_t i, j;
    stTree *ret = stTree_cloneNode(tree);
    stPhylogenyInfo *info = stPhylogenyInfo_clone(stTree_getClientData(tree));
    stTree_setClientData(ret, info);
    for(i = 0; i < stTree_getChildNumber(tree); i++) {
        stTree *child = stTree_getChild(tree, i);
        stTree_setParent(removePoorlySupportedPartitions(child, threshold), ret);
    }
    if(info->bootstrapSupport < threshold) {
        // Make all grandchildren (if any) into children instead.
        for(i = 0; i < stTree_getChildNumber(ret); i++) {
            stTree *child = stTree_getChild(ret, i);
            if(stTree_getChildNumber(child) != 0) {
                for(j = 0; j < stTree_getChildNumber(child); j++) {
                    stTree *grandChild = stTree_getChild(child, j);
                    // hacky, but the grandchild number has decreased by 1
                    j--;
                    stTree_setParent(grandChild, ret);
                }
                // Cleanup the unneeded child
                stPhylogenyInfo_destruct(stTree_getClientData(child));
                stTree_setParent(child, NULL);
                // hacky, but the child number has decreased by 1
                i--;
                // FIXME: need to destruct just one node here, not all
                // the children!
                // stTree_destruct(child);
            }
        }
    }
    return ret;
}

stList *splitTreeOnOutgroups(stTree *tree, stSet *outgroups) {
    return NULL;
}

// (Re)root and reconcile a gene tree to conform with the species tree.
stTree *reconcile(stTree *geneTree, stTree *speciesTree) {
    return NULL;
}

// Compute the reconciliation cost of a rooted gene tree and a species tree.
double reconciliationCost(stTree *geneTree, stTree *speciesTree) {
    return 0.0;
}
