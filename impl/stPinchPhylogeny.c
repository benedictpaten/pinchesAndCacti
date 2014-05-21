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
#include "quicktree_1.1/include/cluster.h"
#include "quicktree_1.1/include/tree.h"
#include "quicktree_1.1/include/buildtree.h"

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

/*
 * Get the base at a given location in a segment.
 */
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

// Free a stPhylogenyInfo struct
void stPhylogenyInfo_destruct(stPhylogenyInfo *info) {
    assert(info != NULL);
    free(info->leavesBelow);
    free(info);
}

// Free the stPhylogenyInfo struct for this node and all nodes below it.
void stPhylogenyInfo_destructOnTree(stTree *tree) {
    int64_t i;
    stPhylogenyInfo_destruct(stTree_getClientData(tree));
    stTree_setClientData(tree, NULL);
    for (i = 0; i < stTree_getChildNumber(tree); i++) {
        stPhylogenyInfo_destructOnTree(stTree_getChild(tree, i));
    }
}

// Set (and allocate) the leavesBelow and totalNumLeaves attribute in
// the phylogenyInfo for the given tree and all subtrees. The
// phylogenyInfo structure (in the clientData field) must already be
// allocated!
static void setLeavesBelow(stTree *tree, int64_t totalNumLeaves) {
    int64_t i, j;
    assert(stTree_getClientData(tree) != NULL);
    stPhylogenyInfo *info = stTree_getClientData(tree);
    for (i = 0; i < stTree_getChildNumber(tree); i++) {
        setLeavesBelow(stTree_getChild(tree, i), totalNumLeaves);
    }

    info->totalNumLeaves = totalNumLeaves;
    if (info->leavesBelow != NULL) {
        // leavesBelow has already been allocated somewhere else, free it.
        free(info->leavesBelow);
    }
    info->leavesBelow = st_calloc(totalNumLeaves, sizeof(int64_t));
    if (stTree_getChildNumber(tree) == 0) {
        assert(info->matrixIndex < totalNumLeaves);
        assert(info->matrixIndex >= 0);
        info->leavesBelow[info->matrixIndex] = 1;
    } else {
        for (i = 0; i < totalNumLeaves; i++) {
            for (j = 0; j < stTree_getChildNumber(tree); j++) {
                stPhylogenyInfo *childInfo = stTree_getClientData(stTree_getChild(tree, j));
                info->leavesBelow[i] |= childInfo->leavesBelow[i];
            }
        }
    }
}

static stTree *quickTreeToStTreeR(struct Tnode *tNode) {
    stTree *ret = stTree_construct();
    bool hasChild = false;
    if (tNode->left != NULL) {
        stTree *left = quickTreeToStTreeR(tNode->left);
        stTree_setParent(left, ret);
        hasChild = true;
    }
    if (tNode->right != NULL) {
        stTree *right = quickTreeToStTreeR(tNode->right);
        stTree_setParent(right, ret);
        hasChild = true;
    }
    if (stTree_getClientData(ret) == NULL) {
        // Allocate the phylogenyInfo for this node.
        stPhylogenyInfo *info = st_calloc(1, sizeof(stPhylogenyInfo));
        if (!hasChild) {
            info->matrixIndex = tNode->nodenumber;
        } else {
            info->matrixIndex = -1;
        }
        stTree_setClientData(ret, info);
    }
    stTree_setBranchLength(ret, tNode->distance);

    // Can remove if needed, probably not useful except for testing.
    stTree_setLabel(ret, stString_print("%u", tNode->nodenumber));
    return ret;
}

// Helper function for converting an unrooted QuickTree Tree into an
// stTree, rooted halfway along the longest branch.
static stTree *quickTreeToStTree(struct Tree *tree) {
    struct Tree *rootedTree = get_root_Tnode(tree);
    stTree *ret = quickTreeToStTreeR(rootedTree->child[0]);
    setLeavesBelow(ret, (stTree_getNumNodes(ret) + 1) / 2);
    return ret;
}

// distanceMatrix should have, probably, distFrom, distTo, labelFrom, labelTo
// (or could just be double and a labeling array passed as a parameter, which is probably better)
// Only one half of the distanceMatrix is used, distances[i][j] for which i > j
// Tree returned is labeled by the indices of the distance matrix and is rooted halfway along the longest branch.
stTree *neighborJoin(double **distances, int64_t numSequences) {
    struct DistanceMatrix *distanceMatrix;
    struct Tree *tree;
    int64_t i, j;
    assert(numSequences > 0);
    assert(distances != NULL);
    // Set up the basic QuickTree data structures to represent the sequences.
    // The data structures are only filled in as much as absolutely
    // necessary, so they will probably be invalid for anything but
    // running neighbor-joining.
    struct ClusterGroup *clusterGroup = empty_ClusterGroup();
    struct Cluster **clusters = st_malloc(numSequences * sizeof(struct Cluster *));
    for (i = 0; i < numSequences; i++) {
        struct Sequence *seq = empty_Sequence();
        seq->name = stString_print("%" PRIi64, i);
        clusters[i] = single_Sequence_Cluster(seq);
    }
    clusterGroup->clusters = clusters;
    clusterGroup->numclusters = numSequences;
    // Fill in the QuickTree distance matrix
    distanceMatrix = empty_DistanceMatrix(numSequences);
    for (i = 0; i < numSequences; i++) {
        for (j = 0; j < i; j++) {
            distanceMatrix->data[i][j] = distances[i][j];
        }
    }
    clusterGroup->matrix = distanceMatrix;
    // Finally, run the neighbor-joining algorithm.
    tree = neighbour_joining_buildtree(clusterGroup, 0);
    free_ClusterGroup(clusterGroup);
    return quickTreeToStTree(tree);
}

// Compare a single partition to a single bootstrap partition.
void comparePartitions(stTree *partition, stTree *bootstrap) {
    int64_t i, j;
    stPhylogenyInfo *partitionInfo, *bootstrapInfo;
    if (stTree_getChildNumber(partition) != stTree_getChildNumber(bootstrap)) {
        // Can't compare different numbers of partitions
        return;
    }

    partitionInfo = stTree_getClientData(partition);
    bootstrapInfo = stTree_getClientData(bootstrap);
    assert(partitionInfo != NULL);
    assert(bootstrapInfo != NULL);
    assert(partitionInfo->totalNumLeaves == bootstrapInfo->totalNumLeaves);
    // Check if the set of leaves is equal in both partitions. If not,
    // the partitions can't be equal.
    if (memcmp(partitionInfo->leavesBelow, bootstrapInfo->leavesBelow,
            partitionInfo->totalNumLeaves * sizeof(int64_t))) {
        return;
    }

    // The sets of leaves under the nodes are equal; now we need to
    // check that the sets they are partitioned into are equal.
    for (i = 0; i < stTree_getChildNumber(partition); i++) {
        stPhylogenyInfo *childInfo = stTree_getClientData(stTree_getChild(partition, i));
        bool foundPartition = false;
        for (j = 0; j < stTree_getChildNumber(bootstrap); j++) {
            stPhylogenyInfo *bootstrapChildInfo = stTree_getClientData(stTree_getChild(bootstrap, j));
            if (memcmp(childInfo->leavesBelow, bootstrapChildInfo->leavesBelow,
                    partitionInfo->totalNumLeaves * sizeof(int64_t)) == 0) {
                foundPartition = true;
                break;
            }
        }
        if (!foundPartition) {
            return;
        }
    }

    // The partitions are equal, increase the support 
    partitionInfo->bootstraps++;
}

// Score each partition in destTree by how many times the same
// partition appears in the bootstrapped trees. The information is
// stored in a double in the clientData field of the tree nodes,
// indicating the fraction of bootstraps that support the partition.
// FIXME: right now it is just an int rather than a double.
void scoreByBootstraps(stTree *destTree, stList *bootstraps) {
    int64_t i;
    for (i = 0; i < stList_length(bootstraps); i++) {

    }
}

// Remove partitions that are poorly-supported from the tree. The tree
// must have bootstrap-support information in all the nodes.
stTree *removePoorlySupportedPartitions(stTree *bootstrappedTree, double threshold) {
    return NULL;
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
