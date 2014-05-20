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
    int64_t distance; //The distance of the first base in the segment from the id-point of the chosen segment.
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

// Set (and allocate) the leavesBelow and totalNumLeaves attribute in
// the phylogenyInfo for the given tree and all subtrees. The
// phylogenyInfo structure (in the clientData field) must already be
// allocated!
static void setLeavesBelow(stTree *tree, int64_t totalNumLeaves)
{
    int64_t i, j;
    assert(stTree_getClientData(tree) != NULL);
    stPhylogenyInfo *info = stTree_getClientData(tree);
    for(i = 0; i < stTree_getChildNumber(tree); i++) {
        setLeavesBelow(stTree_getChild(tree, i), totalNumLeaves);
    }

    info->totalNumLeaves = totalNumLeaves;
    if(info->leavesBelow != NULL) {
        // leavesBelow has already been allocated somewhere else, free it.
        free(info->leavesBelow);
    }
    info->leavesBelow = st_calloc(totalNumLeaves, sizeof(int64_t));
    if(stTree_getChildNumber(tree) == 0) {
        assert(info->matrixIndex < totalNumLeaves);
        assert(info->matrixIndex >= 0);
        info->leavesBelow[info->matrixIndex] = 1;
    } else {
        for(i = 0; i < totalNumLeaves; i++) {
            for(j = 0; j < stTree_getChildNumber(tree); j++) {
                stPhylogenyInfo *childInfo = stTree_getClientData(stTree_getChild(tree, j));
                info->leavesBelow[i] |= childInfo->leavesBelow[i];
            }
        }
    }
}

static stTree *quickTreeToStTreeR(struct Tnode *tNode)
{
    stTree *ret = stTree_construct();
    bool hasChild = false;
    if(tNode->left != NULL) {
        stTree *left = quickTreeToStTreeR(tNode->left);
        stTree_setParent(left, ret);
        hasChild = true;
    }
    if(tNode->right != NULL) {
        stTree *right = quickTreeToStTreeR(tNode->right);
        stTree_setParent(right, ret);
        hasChild = true;
    }
    if(stTree_getClientData(ret) == NULL) {
        // Allocate the phylogenyInfo for this node.
        stPhylogenyInfo *info = st_calloc(1, sizeof(stPhylogenyInfo));
        if(!hasChild) {
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
static stTree *quickTreeToStTree(struct Tree *tree)
{
    struct Tree *rootedTree = get_root_Tnode(tree);
    stTree *ret = quickTreeToStTreeR(rootedTree->child[0]);
    setLeavesBelow(ret, (stTree_getNumNodes(ret)+1)/2);
    return ret;
}

// distanceMatrix should have, probably, distFrom, distTo, labelFrom, labelTo
// (or could just be double and a labeling array passed as a parameter, which is probably better)
// Only one half of the distanceMatrix is used, distances[i][j] for which i > j
// Tree returned is labeled by the indices of the distance matrix and is rooted halfway along the longest branch.
stTree *neighborJoin(double **distances, int64_t numSequences)
{
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
    for(i = 0; i < numSequences; i++) {
        struct Sequence *seq = empty_Sequence();
        seq->name = stString_print("%" PRIi64, i);
        clusters[i] = single_Sequence_Cluster(seq);
    }
    clusterGroup->clusters = clusters;
    clusterGroup->numclusters = numSequences;
    // Fill in the QuickTree distance matrix
    distanceMatrix = empty_DistanceMatrix(numSequences);
    for(i = 0; i < numSequences; i++) {
        for(j = 0; j < i; j++) {
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
void comparePartitions(stTree *partition, stTree *bootstrap)
{
    int64_t i, j;
    stPhylogenyInfo *partitionInfo, *bootstrapInfo;
    if(stTree_getChildNumber(partition) != stTree_getChildNumber(bootstrap)) {
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
    if(memcmp(partitionInfo->leavesBelow, bootstrapInfo->leavesBelow,
              partitionInfo->totalNumLeaves * sizeof(int64_t))) {
        return;
    }

    // The sets of leaves under the nodes are equal; now we need to
    // check that the sets they are partitioned into are equal.
    for(i = 0; i < stTree_getChildNumber(partition); i++) {
        stPhylogenyInfo *childInfo = stTree_getClientData(stTree_getChild(partition, i));
        bool foundPartition = false;
        for(j = 0; j < stTree_getChildNumber(bootstrap); j++) {
            stPhylogenyInfo *bootstrapChildInfo = stTree_getClientData(stTree_getChild(bootstrap, j));
            if(memcmp(childInfo->leavesBelow, bootstrapChildInfo->leavesBelow,
                      partitionInfo->totalNumLeaves * sizeof(int64_t)) == 0) {
                foundPartition = true;
                break;
            }
        }
        if(!foundPartition) {
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
void scoreByBootstraps(stTree *destTree, stList *bootstraps)
{
    int64_t i;
    for(i = 0; i < stList_length(bootstraps); i++) {
        
    }
}

// Remove partitions that are poorly-supported from the tree. The tree
// must have bootstrap-support information in all the nodes.
stTree *removePoorlySupportedPartitions(stTree *bootstrappedTree,
                                        double threshold)
{
    return NULL;
}

stList *splitTreeOnOutgroups(stTree *tree, stSet *outgroups)
{
    return NULL;
}

// (Re)root and reconcile a gene tree to conform with the species tree.
stTree *reconcile(stTree *geneTree, stTree *speciesTree)
{
    return NULL;
}

// Compute the reconciliation cost of a rooted gene tree and a species tree.
double reconciliationCost(stTree *geneTree, stTree *speciesTree)
{
    return 0.0;
}
