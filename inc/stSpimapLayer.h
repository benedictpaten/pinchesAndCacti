#ifndef STSPIMAPLAYER_H_
#define STSPIMAPLAYER_H_
// Reroot a binary gene tree against a binary species tree to minimize
// the number of dups and losses using spimap. Uses the algorithm
// described in Zmasek & Eddy, Bioinformatics, 2001.
stTree *spimap_rootAndReconcile(stTree *geneTree, stTree *speciesTree,
                                stHash *leafToSpecies);

void spimap_reconciliationCost(stTree *geneTree, stTree *speciesTree,
                               stHash *leafToSpecies, int64_t *dups,
                               int64_t *losses);

#endif
