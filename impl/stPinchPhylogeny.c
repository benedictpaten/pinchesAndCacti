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
#include "stSpimapLayer.h"

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

static stFeatureBlock *stFeatureBlock_construct(stFeatureSegment *firstSegment, stPinchBlock *block) {
    stFeatureBlock *featureBlock = st_malloc(sizeof(stFeatureBlock));
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

stList *stFeatureBlock_getContextualFeatureBlocks(stPinchBlock *block, int64_t maxBaseDistance,
        int64_t maxBlockDistance,
        bool ignoreUnalignedBases, bool onlyIncludeCompleteFeatureBlocks, stHash *strings) {
    /*
     * First build the set of feature blocks, not caring if this produces trivial blocks.
     */
    stHash *blocksToFeatureBlocks = stHash_construct(); //Hash to map blocks to featureBlocks.
    stHash *segmentsToFeatureSegments = stHash_construct(); //Hash to map segments to featureSegments, to remove overlaps.
    int64_t i = 0; //Index of segments in block.
    stPinchBlockIt it = stPinchBlock_getSegmentIterator(block);
    stPinchSegment *segment;
    while ((segment = stPinchBlockIt_getNext(&it)) != NULL) { //For each segment build the feature segment.
        int64_t blockDistance, baseDistance; //Distances from segment2 to segment
        //Base distance is the difference from the start coordinate of segment2 to the mid point of the segment.
        stPinchSegment *segment2 = get5PrimeMostSegment(segment, &baseDistance, &blockDistance, maxBaseDistance,
                maxBlockDistance, ignoreUnalignedBases);

        do {
            stPinchBlock *block2;
            if ((block2 = stPinchSegment_getBlock(segment2)) != NULL) {
                //The following checks that if baseDistance + stPinchSegment_getLength(segment2) / 2 == 0 then the segment has to be the mid segment
                if (baseDistance + stPinchSegment_getLength(segment2) / 2 == 0) {
                    assert(segment2 == segment);
                } else {
                    assert(segment2 != segment);
                }

                stFeatureSegment *fSegment = stHash_search(segmentsToFeatureSegments, segment2);
                if (fSegment == NULL) {
                    //Make a new feature segment
                    fSegment = stFeatureSegment_construct(segment2, strings, i, baseDistance);
                    //Attach it to the related block.
                    stFeatureBlock *featureBlock = stHash_search(blocksToFeatureBlocks, block2);
                    if (featureBlock == NULL) { //Create a new feature block
                        featureBlock = stFeatureBlock_construct(fSegment, block2);
                        stHash_insert(blocksToFeatureBlocks, block2, featureBlock);
                    } else { //Link it to the existing feature block
                        featureBlock->tail->nFeatureSegment = fSegment;
                        featureBlock->tail = fSegment;
                    }
                    stHash_insert(segmentsToFeatureSegments, segment2, fSegment);
                } else {
                    assert(segment2 == fSegment->segment);
                    assert(stPinchSegment_getLength(segment2) == fSegment->length);
                    assert(fSegment->distance != baseDistance); //It is not possible for these two distances to be equal.
                    //If the midpoint of segment2 is closer to the mid point of segment than the midpoint of featureSegment->segment, then switch
                    if (llabs(fSegment->distance + fSegment->length / 2)
                            > llabs(baseDistance + fSegment->length / 2)) {
                        fSegment->segmentIndex = i;
                        fSegment->distance = baseDistance;
                    }
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
    stHash_destruct(segmentsToFeatureSegments);
    /*
     * Now filter the set of blocks so that only desired blocks are present.
     */
    stList *unfilteredFeatureBlocks = stHash_getValues(blocksToFeatureBlocks);
    stList *featureBlocks = stList_construct3(0, (void (*)(void *)) stFeatureBlock_destruct);
    stHash_destruct(blocksToFeatureBlocks); //Cleanup now, as no longer needed, and values freed.
    for (int64_t i = 0; i < stList_length(unfilteredFeatureBlocks); i++) {
        stFeatureBlock *featureBlock = stList_get(unfilteredFeatureBlocks, i);
        assert(featureBlock != NULL);
        if (!onlyIncludeCompleteFeatureBlocks || countDistinctIndices(featureBlock) == stPinchBlock_getDegree(block)) {
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
 * Adds to a feature matrix by iterating through the feature columns and adding in features of the column,
 * according to the given function.
 */
static void addFeaturesToMatrix(stMatrix *matrix, stList *featureColumns, double distanceWeightFn(int64_t, int64_t),
bool sampleColumns, bool (*equalFn)(stFeatureSegment *, stFeatureSegment *, int64_t),
bool (*nullFeature)(stFeatureSegment *, int64_t)) {
    for (int64_t i = 0; i < stList_length(featureColumns); i++) {
        stFeatureColumn *featureColumn = stList_get(featureColumns,
                sampleColumns ? st_randomInt(0, stList_length(featureColumns)) : i);
        stFeatureSegment *fSegment = featureColumn->featureBlock->head;
        int64_t distance1 = stFeatureSegment_getColumnDistance(fSegment, featureColumn->columnIndex);
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
                            if (equalFn(fSegment, nSegment, featureColumn->columnIndex)) {
                                *stMatrix_getCell(matrix, j, k) += distanceWeightFn(distance1, distance2);
                            } else {
                                *stMatrix_getCell(matrix, k, j) += distanceWeightFn(distance1, distance2);
                            }
                        }
                    }
                    nSegment = nSegment->nFeatureSegment;
                }
            }
            fSegment = fSegment->nFeatureSegment;
        }
    }
}

double stPinchPhylogeny_constantDistanceWeightFn(int64_t i, int64_t j) {
    return 1;
}

stMatrix *stPinchPhylogeny_getMatrixFromSubstitutions(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    stMatrix *matrix = stMatrix_construct(stPinchBlock_getDegree(block), stPinchBlock_getDegree(block));
    addFeaturesToMatrix(matrix, featureColumns, distanceWeightFn == NULL ? stPinchPhylogeny_constantDistanceWeightFn : distanceWeightFn, sampleColumns, stFeatureSegment_basesEqual,
            stFeatureSegment_baseIsWildCard);
    return matrix;
}

stMatrix *stPinchPhylogeny_getMatrixFromBreakpoints(stList *featureColumns, stPinchBlock *block,
        double distanceWeightFn(int64_t, int64_t), bool sampleColumns) {
    stMatrix *matrix = stMatrix_construct(stPinchBlock_getDegree(block), stPinchBlock_getDegree(block));
    addFeaturesToMatrix(matrix, featureColumns, distanceWeightFn == NULL ? stPinchPhylogeny_constantDistanceWeightFn : distanceWeightFn, sampleColumns, stFeatureSegment_leftAdjacenciesEqual,
            stFeatureSegment_leftAdjacencyIsWildCard);
    addFeaturesToMatrix(matrix, featureColumns, distanceWeightFn == NULL ? stPinchPhylogeny_constantDistanceWeightFn : distanceWeightFn, sampleColumns, stFeatureSegment_rightAdjacenciesEqual,
            stFeatureSegment_rightAdjacencyIsWildCard);
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
        if (childInfo->bootstrapSupport < threshold) {
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
            assert(outgroup < childInfo->totalNumLeaves);
            if(childInfo->leavesBelow[outgroup]) {
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

    if(stList_length(outgroups) == info->totalNumLeaves) {
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
        for(j = 0; j < cladeInfo->totalNumLeaves; j++) {
            if(cladeInfo->leavesBelow[j]) {
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

// Builds a tree from a set of feature columns.
// TODO: add distance weight function and matrix-merge function as
// parameters if necessary
stTree *stPinchPhylogeny_buildTreeFromFeatureColumns(stList *featureColumns,
                                                     stPinchBlock *block,
                                                     bool bootstrap) {
    stMatrix *snpMatrix = stPinchPhylogeny_getMatrixFromSubstitutions(featureColumns, block, NULL, bootstrap);
    stMatrix *breakpointMatrix = stPinchPhylogeny_getMatrixFromBreakpoints(featureColumns, block, NULL, bootstrap);

    // Build distance matrix from the two matrices
    stMatrix *mergedMatrix = stMatrix_add(snpMatrix, breakpointMatrix);
    stMatrix *matrix = stPinchPhylogeny_getSymmetricDistanceMatrix(mergedMatrix);
    stTree *tree = stPhylogeny_neighborJoin(matrix, NULL);

    // Clean up
    stMatrix_destruct(snpMatrix);
    stMatrix_destruct(breakpointMatrix);
    stMatrix_destruct(mergedMatrix);
    stMatrix_destruct(matrix);
    return tree;
}

// Gets a list of disjoint leaf sets (ingroup clades separated by
// outgroups) from a set of feature columns. The leaves are ints
// corresponding to the index of their respective feature column
// TODO: add distance weight function and matrix-merge function as
// parameters if necessary
stList *stPinchPhylogeny_getLeafSetsFromFeatureColumns(stList *featureColumns,
                                                       stPinchBlock *block,
                                                       int64_t numBootstraps,
                                                       double confidenceThreshold,
                                                       stList *outgroups) {
    int64_t numLeaves = stPinchBlock_getDegree(block);
    if(numLeaves < 3) {
        // No point in building a tree for < 3 nodes, return a set
        // containing everything
        stList *ret = stList_construct();
        stList *inner = stList_construct();
        for(int64_t i = 0; i < numLeaves; i++) {
            stIntTuple *iTuple = stIntTuple_construct1(i);
            stList_append(inner, iTuple);
        }
        stList_append(ret, inner);
        return ret;
    }

    // Get the canonical tree, which does not use resampling of the columns.
    stTree *canonicalTree = stPinchPhylogeny_buildTreeFromFeatureColumns(featureColumns, block, false);
    stList *bootstraps = stList_construct();
    if(numBootstraps > 0) {
        // Make the bootstrap trees and re-score the canonical tree's
        // partitions according to the bootstraps'.
        for(int64_t i = 0; i < numBootstraps; i++) {
            stTree *bootstrap = stPinchPhylogeny_buildTreeFromFeatureColumns(featureColumns, block, true);
            stList_append(bootstraps, bootstrap);
        }
        stPhylogeny_scoreFromBootstraps(canonicalTree, bootstraps);

        // Remove partitions that don't meet the criteria
        stTree *tmp = stPinchPhylogeny_removePoorlySupportedPartitions(canonicalTree, confidenceThreshold);
        stPhylogenyInfo_destructOnTree(canonicalTree);
        stTree_destruct(canonicalTree);
        canonicalTree = tmp;
    }
    // Get the leaf sets.
    stList *ret = stPinchPhylogeny_splitTreeOnOutgroups(canonicalTree, outgroups);

    // Clean up.
    for(int64_t i = 0; i < stList_length(bootstraps); i++) {
        stTree *bootstrap = stList_get(bootstraps, i);
        stPhylogenyInfo_destructOnTree(bootstrap);
        stTree_destruct(bootstrap);
    }
    stList_destruct(bootstraps);
    return ret;
}

// (Re)root and a gene tree to a tree with minimal dups and losses.
stTree *stPinchPhylogeny_reconcileBinary(stTree *geneTree, stTree *speciesTree, stHash *leafToSpecies) {
    return spimap_rootAndReconcile(geneTree, speciesTree, leafToSpecies);
}

// Compute the reconciliation cost of a rooted gene tree and a species tree.
double reconciliationCost(stTree *geneTree, stTree *speciesTree) {
    return 0.0;
}
