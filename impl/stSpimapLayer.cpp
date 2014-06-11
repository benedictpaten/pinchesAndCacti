#include "Tree.h"
#include "phylogeny.h"
#include "sonLib.h"

using namespace spidir;

static void getptree(stTree *tree, int *leafIndex, int *internalIndex,
                     int *ptree, stHash *nodeToIndex) {
    int myIdx = *internalIndex;
    stHash_insert(nodeToIndex, tree, stIntTuple_construct1(myIdx));
    if(stTree_getParent(tree) == NULL) {
        ptree[myIdx] = -1;
    }

    assert(stTree_getChildNumber(tree) == 2);

    for(int64_t i = 0; i < stTree_getChildNumber(tree); i++) {
        stTree *child = stTree_getChild(tree, i);
        if(stTree_getChildNumber(child) == 0) {
            ptree[*leafIndex] = myIdx;
            stHash_insert(nodeToIndex, child, stIntTuple_construct1(*leafIndex));
            (*leafIndex)--;
        } else {
            ptree[*internalIndex - 1] = myIdx;
            (*internalIndex)--;
            getptree(child, leafIndex, internalIndex, ptree, nodeToIndex);
        }
    }
}

// Convert an stTree into a spimap Tree. The tree must be binary.
static Tree *spimapTreeFromStTree(stTree *tree, stHash *nodeToIndex) {
    // Create the parent tree array representation of the tree
    int numNodes = stTree_getNumNodes(tree);
    int *ptree = (int *) calloc(numNodes, sizeof(int));
    int internalIndex = numNodes - 1, leafIndex = (numNodes + 1)/2 - 1;
    getptree(tree, &leafIndex, &internalIndex, ptree, nodeToIndex);
    Tree *ret = makeTree(numNodes, ptree);
    free(ptree);
    return ret;
}

// Convert an stTree into a spimap SpeciesTree. The tree must be binary.
static SpeciesTree *spimapSpeciesTreeFromStTree(stTree *tree, stHash *nodeToIndex) {
    // Create the parent tree array representation of the tree
    int numNodes = stTree_getNumNodes(tree);
    int *ptree = (int *) calloc(numNodes, sizeof(int));
    int internalIndex = numNodes - 1, leafIndex = (numNodes + 1)/2 - 1;
    getptree(tree, &leafIndex, &internalIndex, ptree, nodeToIndex);
    SpeciesTree *ret = new SpeciesTree(numNodes);
    ptree2tree(numNodes, ptree, ret);
    ret->setDepths();
    free(ptree);
    return ret;
}

extern "C" {
// Reroot a binary gene tree against a binary species tree to minimize
// the number of dups and losses using spimap. Uses the algorithm
// described in Zmasek & Eddy, Bioinformatics, 2001.
// FIXME: oh my god so many extra traversals
stTree *spimap_rootAndReconcile(stTree *geneTree, stTree *speciesTree,
                                stHash *leafToSpecies) {
    stHash *geneToIndex = stHash_construct2(NULL, (void (*)(void *))stIntTuple_destruct);
    stHash *speciesToIndex = stHash_construct2(NULL, (void (*)(void *))stIntTuple_destruct);
    Tree *gTree = spimapTreeFromStTree(geneTree, geneToIndex);
    SpeciesTree *sTree = spimapSpeciesTreeFromStTree(speciesTree, speciesToIndex);
    int *gene2species = (int *)calloc(stTree_getNumNodes(geneTree), sizeof(int));
    stHashIterator *genesIt = stHash_getIterator(leafToSpecies);
    stTree *curGene = NULL;
    while((curGene = (stTree *)stHash_getNext(genesIt)) != NULL) {
        stTree *curSpecies = (stTree *)stHash_search(leafToSpecies, curGene);
        assert(curSpecies != NULL);
        stIntTuple *geneIndex = (stIntTuple *)stHash_search(geneToIndex, curGene);
        stIntTuple *speciesIndex = (stIntTuple *)stHash_search(speciesToIndex, curSpecies);
        assert(geneIndex != NULL);
        assert(speciesIndex != NULL);
        gene2species[stIntTuple_get(geneIndex, 0)] = stIntTuple_get(speciesIndex, 0);
    }
    reconRoot(gTree, sTree, gene2species);
    // Root our gene tree in the same place.
    // This is a little sketchy since it's somewhat unclear whether
    // the nodes are renumbered when spimap reroots them.
    Node *root = gTree->root;
    stIntTuple *child1Idx = stIntTuple_construct1(root->children[0]->name);
    stIntTuple *child2Idx = stIntTuple_construct1(root->children[1]->name);
    stHash *indexToGene = stHash_invert(geneToIndex, (uint64_t (*)(const void *))stIntTuple_hashKey,
                                        (int (*)(const void *, const void *))stIntTuple_equalsFn,
                                        NULL, NULL);
    stTree *child1 = (stTree *)stHash_search(indexToGene, child1Idx);
    stTree *child2 = (stTree *)stHash_search(indexToGene, child2Idx);
    assert(child1 != NULL);
    assert(child2 != NULL);
    stTree *ret = NULL;
    if(child1 == stTree_getParent(child2)) {
        ret = stTree_reRoot(child2, stTree_getBranchLength(child2)/2);
    } else if(child2 == stTree_getParent(child1)) {
        ret =  stTree_reRoot(child1, stTree_getBranchLength(child1)/2);
    } else {
        // Can happen if the root hasn't changed.
        assert(stTree_getParent(child1) == stTree_getParent(child2));
        assert(stTree_getParent(stTree_getParent(child1)) == NULL);
//        ret = stTree_reRoot(child1, stTree_getBranchLength(child1));
        ret = stTree_clone(stTree_getParent(child1));
    }
    stHash_destruct(indexToGene);
    stHash_destruct(geneToIndex);
    stHash_destruct(speciesToIndex);
    delete sTree;
    delete gTree;
    return ret;
}
}
