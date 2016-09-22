/*
 * pinchGraphs2.h
 *
 *  Created on: 9 Mar 2012
 *      Author: benedictpaten
 */

/*
 * A library for "pinch" graphs, sequence graphs representing a
 * multiple alignment between a set of sequences. Each sequence is
 * represented by a "thread," which is split into several gapless
 * "segments." The homologies are represented by "blocks," each of
 * which is a set of one or more segments, all of which are gaplessly
 * aligned to each other according to their orientation. These blocks
 * are constructed by "pinching" together threads where they are
 * aligned to each other. The pinch code automatically computes the
 * transitive alignment relationships when pinches overlap one
 * another.
 */

#ifndef ST_PINCH_GRAPHS_H_
#define ST_PINCH_GRAPHS_H_

#include "sonLib.h"

#ifdef __cplusplus
extern "C"{
#endif

//Datastructures

typedef struct _stPinchThreadSet stPinchThreadSet;

typedef struct _stPinchThreadIt {
    stPinchThreadSet *threadSet;
    int64_t index;
} stPinchThreadSetIt;

typedef struct _stPinchThread stPinchThread;

typedef struct _stPinchSegment stPinchSegment;

typedef struct _stPinchThreadSetSegmentIt {
    stPinchThreadSetIt threadIt;
    stPinchSegment *segment;
} stPinchThreadSetSegmentIt;

typedef struct _stPinchThreadSetBlockIt {
    stPinchThreadSetSegmentIt segmentIt;
} stPinchThreadSetBlockIt;

typedef struct _stPinchBlock stPinchBlock;

typedef struct _stPinchBlockIt {
    stPinchSegment *segment;
} stPinchBlockIt;

typedef struct _stPinchEnd {
    stPinchBlock *block;
    bool orientation;
} stPinchEnd;

typedef struct _stPinch {
    int64_t name1;
    int64_t name2;
    int64_t start1;
    int64_t start2;
    int64_t length;
    bool strand;
} stPinch;

typedef struct _stPinchInterval {
    int64_t name;
    int64_t start;
    int64_t length;
    void *label;
} stPinchInterval;

typedef struct _stPinchUndo stPinchUndo;

/*
 * Functions relating to "thread sets," which are collections of
 * threads. The main object, through which you can access the entirety
 * of the pinch graph.
 */

/*
 * Construct an empty pinch graph.
 */
stPinchThreadSet *stPinchThreadSet_construct(void);

/*
 * Destroy a pinch graph.
 */
void stPinchThreadSet_destruct(stPinchThreadSet *threadSet);

/*
 * Add a thread to a pinch graph. The "name" must be an int unique
 * among the threads in this graph.
 *
 * Valid positions within the new thread range from start (inclusive)
 * to start + length (exclusive).
 */
stPinchThread *stPinchThreadSet_addThread(stPinchThreadSet *threadSet, int64_t name, int64_t start, int64_t length);

/*
 * Gets a thread from a pinch graph.
 */
stPinchThread *stPinchThreadSet_getThread(stPinchThreadSet *threadSet, int64_t name);

/*
 * Gets the segment which includes the specified position in the
 * specified thread. If the position is out of range for the thread,
 * returns NULL.
 */
stPinchSegment *stPinchThreadSet_getSegment(stPinchThreadSet *threadSet, int64_t name, int64_t coordinate);

/*
 * Gets the number of threads in the graph.
 */
int64_t stPinchThreadSet_getSize(stPinchThreadSet *threadSet);

/*
 * Gets an iterator that traverses over the threads in the graph.
 */
stPinchThreadSetIt stPinchThreadSet_getIt(stPinchThreadSet *threadSet);

/*
 * Gets the next thread from the iterator. Returns NULL when done
 * iterating.
 */
stPinchThread *stPinchThreadSetIt_getNext(stPinchThreadSetIt *);

/*
 * Heals trivial splits: where there is an unnecessary breakpoint
 * between two neighboring blocks or segments, they are joined
 * together.
 *
 * NB: this function heals trivial block and segment splits, but
 * stPinchThread_joinTrivialBoundaries heals only trivial segment
 * splits.
 */
void stPinchThreadSet_joinTrivialBoundaries(stPinchThreadSet *threadSet);

/*
 * Get the total number of blocks in the graph.
 */
int64_t stPinchThreadSet_getTotalBlockNumber(stPinchThreadSet *threadSet);

/*
 * Get a list of adjacency-connected components for this pinch
 * graph. Each connected component is represented by a list of
 * stPinchEnds that are directly or indirectly connected by an
 * adjacency or series of adjacencies.
 */
stList *stPinchThreadSet_getAdjacencyComponents(stPinchThreadSet *threadSet);

/*
 * Same as stPinchThreadSet_getAdjacencyComponents, except you also
 * get a pointer to a hash that maps block ends to adjacency
 * components.
 */
stList *stPinchThreadSet_getAdjacencyComponents2(stPinchThreadSet *threadSet, stHash **edgeEndsToAdjacencyComponents);

/*
 * Get a list of thread components. Each thread component is a list of
 * stPinchThreads that transitively share at least one block (although
 * any two given threads in the component don't necessarily have to
 * share a block in common).
 */
stSortedSet *stPinchThreadSet_getThreadComponents(stPinchThreadSet *threadSet);

/*
 * Get a random set of randomly named threads that share no homology.
 */
stPinchThreadSet *stPinchThreadSet_getRandomEmptyGraph(void);

/*
 * Get a random pinch (a pairwise alignment between two threads).
 */
stPinch stPinchThreadSet_getRandomPinch(stPinchThreadSet *threadSet);

/*
 * Get a random pinch graph.
 */
stPinchThreadSet *stPinchThreadSet_getRandomGraph(void);

//convenience functions

/*
 * Get an iterator over every segment in the pinch graph.
 */
stPinchThreadSetSegmentIt stPinchThreadSet_getSegmentIt(stPinchThreadSet *threadSet);

stPinchSegment *stPinchThreadSetSegmentIt_getNext(stPinchThreadSetSegmentIt *segmentIt);

/*
 * Get an iterator over every block in the pinch graph.
 */
stPinchThreadSetBlockIt stPinchThreadSet_getBlockIt(stPinchThreadSet *threadSet);

stPinchBlock *stPinchThreadSetBlockIt_getNext(stPinchThreadSetBlockIt *blockIt);

//Thread

/*
 * Get the unique integer name for this thread.
 */
int64_t stPinchThread_getName(stPinchThread *stPinchThread);

/*
 * Get the start offset for this thread.
 */
int64_t stPinchThread_getStart(stPinchThread *stPinchThread);

/*
 * Get the length for this thread.
 */
int64_t stPinchThread_getLength(stPinchThread *stPinchThread);

/*
 * Get the segment that overlaps the given position. If the position
 * is out of range, returns NULL.
 */
stPinchSegment *stPinchThread_getSegment(stPinchThread *stPinchThread, int64_t coordinate);

/*
 * Get the 5'-most segment in the thread.
 */
stPinchSegment *stPinchThread_getFirst(stPinchThread *stPinchThread);

/*
 * Get the 3'-most segment in the thread.
 */
stPinchSegment *stPinchThread_getLast(stPinchThread *thread);

/*
 * Split the segment at the given thread and position in two, such
 * that there will be two segments, the left (or 5')-most of which
 * includes leftSideOfSplitPoint as its last base. If
 * leftSideOfSplitPoint is already the last base of a segment, this
 * does nothing.
 */
void stPinchThread_split(stPinchThread *thread, int64_t leftSideOfSplitPoint);

/*
 * Heals unnecessary breaks between unaligned segments in this thread.
 *
 * NB: Unlike stPinchThreadSet_joinTrivialBoundaries, this does not
 * heal trivial breaks between blocks.
 */
void stPinchThread_joinTrivialBoundaries(stPinchThread *thread);

/*
 * Pinch two threads together (a pairwise gapless alignment). Handles
 * transitive alignment automatically.
 *
 * strand2 is positive if aligning to the + strand of thread2 and 0 otherwise.
 *
 * NB: This always pinches [start1, start1 + length) and [start2, start2 + length)
 * irrespective of the strand2 parameter, i.e. all coordinates are +-strand relative.
 */
void stPinchThread_pinch(stPinchThread *thread1, stPinchThread *thread2, int64_t start1, int64_t start2, int64_t length, bool strand2);

/*
 * Same as stPinchThread_pinch, but only segments for which filterFn
 * returns 0 are pinched together. This function is run for all
 * segments within the range supplied, so only parts of the pinch may
 * be applied.
 */
void stPinchThread_filterPinch(stPinchThread *thread1, stPinchThread *thread2, int64_t start1, int64_t start2,
        int64_t length, bool strand2, bool(*filterFn)(stPinchSegment *, stPinchSegment *));

//Segments

/*
 * Start (inclusive) of this segment.
 */
int64_t stPinchSegment_getStart(stPinchSegment *segment);

/*
 * Total number of bases in this segment.
 */
int64_t stPinchSegment_getLength(stPinchSegment *segment);

/*
 * Gets the name of the thread that this segment belongs to.
 */
int64_t stPinchSegment_getName(stPinchSegment *segment);

/*
 * Get the segment to the 3' (aka right) of this segment, or NULL if
 * this is the first segment.
 */
stPinchSegment *stPinchSegment_get3Prime(stPinchSegment *segment);

/*
 * Get the segment to the 5' (aka left) of this segment, or NULL if
 * this is the first segment.
 */
stPinchSegment *stPinchSegment_get5Prime(stPinchSegment *segment);

/*
 * Get the thread that this segment belongs to.
 */
stPinchThread *stPinchSegment_getThread(stPinchSegment *segment);

/*
 * Get the block that this segment belongs to, or NULL if it's not in
 * a block.
 */
stPinchBlock *stPinchSegment_getBlock(stPinchSegment *segment);

/*
 * Get the orientation of this segment relative to its block. 0 if
 * opposite orientation, same orientation otherwise. Undefined if the segment
 * is not in a block.
 */
bool stPinchSegment_getBlockOrientation(stPinchSegment *segment);

/*
 * Set the orientation of this segment relative to its block.
 */
void stPinchSegment_setBlockOrientation(stPinchSegment *segment, bool orientation);

/*
 * As with stPinchThread_split, but for a given segment.
 */
void stPinchSegment_split(stPinchSegment *segment, int64_t leftSideOfSplitPoint);

/*
 * Makes this segment the first segment in its block, if it has one.
 */
void stPinchSegment_putSegmentFirstInBlock(stPinchSegment *segment);

/*
 * Compare two segments by name and position.
 */
int stPinchSegment_compare(const stPinchSegment *segment1, const stPinchSegment *segment2);

//Blocks

/*
 * Make a new block with just one segment in the given orientation (0
 * meaning opposite to the "block orientation").
 */
stPinchBlock *stPinchBlock_construct3(stPinchSegment *segment, bool orientation);

/*
 * Make a new block with just one segment and block orientation 1.
 */
stPinchBlock *stPinchBlock_construct2(stPinchSegment *segment1);

/*
 * Make a block from two segments of the same length.
 */
stPinchBlock *stPinchBlock_construct(stPinchSegment *segment1, bool orientation1, stPinchSegment *segment2, bool orientation2);

/*
 * Pinch together two blocks of the same length. If orientation is
 * true, the blocks are in the same orientation, otherwise they are in
 * opposite orientation.
 */
stPinchBlock *stPinchBlock_pinch(stPinchBlock *block1, stPinchBlock *block2, bool orientation);

/*
 * Pinch a segment into the block. If orientation is true, the segment
 * is in the same orientation as the block.
 */
stPinchBlock *stPinchBlock_pinch2(stPinchBlock *block1, stPinchSegment *segment, bool orientation);

/*
 * Get an iterator over the segments in this block.
 */
stPinchBlockIt stPinchBlock_getSegmentIterator(stPinchBlock *block);

stPinchSegment *stPinchBlockIt_getNext(stPinchBlockIt *stPinchBlockIt);

/*
 * Free a block and return its segments to being unaligned.
 */
void stPinchBlock_destruct(stPinchBlock *block);

/*
 * Get the length of a block. By definition, all its segments have the
 * same length.
 */
int64_t stPinchBlock_getLength(stPinchBlock *block);

/*
 * Get the first segment in a block.
 */
stPinchSegment *stPinchBlock_getFirst(stPinchBlock *block);

/*
 * Get the number of segments in a block.
 */
uint64_t stPinchBlock_getDegree(stPinchBlock *block);

/*
 * Get the number of times any two segments in this block have been
 * pinched together (including any times that they were pinched
 * together even when they were already in the same block).
 */
uint64_t stPinchBlock_getNumSupportingHomologies(stPinchBlock *block);

/*
 * Trim the given number of bases away from each end of the block. The
 * parts trimmed away become unaligned.
 */
void stPinchBlock_trim(stPinchBlock *block, int64_t blockEndTrim);

//Block ends

/*
 * Replace the info in the given stPinchEnd.
 */
void stPinchEnd_fillOut(stPinchEnd *end, stPinchBlock *block, bool orientation);

/*
 * Make a new pinch end.
 */
stPinchEnd *stPinchEnd_construct(stPinchBlock *block, bool orientation);

/*
 * Make a new pinch end without touching the heap.
 */
stPinchEnd stPinchEnd_constructStatic(stPinchBlock *block, bool orientation);

/*
 * Free a pinch end.
 */
void stPinchEnd_destruct(stPinchEnd *end);

/*
 * Get the block corresponding to a pinch end.
 */
stPinchBlock *stPinchEnd_getBlock(stPinchEnd *end);

/*
 * Get the orientation for this end: 1 indicates the 5' end relative
 * to the block orientation, and 0 indicates the 3' end relative to
 * block orientation.
 */
bool stPinchEnd_getOrientation(stPinchEnd *end);

/*
 * Returns true if the ends have the same block and orientation.
 */
int stPinchEnd_equalsFn(const void *, const void *);

/*
 * Hashes an end.
 */
uint64_t stPinchEnd_hashFn(const void *);

/*
 * Determines whether, to follow the adjacency corresponding to this
 * end for the given segment, you should travel 5' (in which case this
 * returns true) or 3' (in which case this returns false).
 */
bool stPinchEnd_traverse5Prime(bool endOrientation, stPinchSegment *segment);

/*
 * No clue.
 */
bool stPinchEnd_endOrientation(bool _5PrimeTraversal, stPinchSegment *segment);

/*
 * Determines whether the blocks adjacent at this end can be
 * joined without changing the information in the graph.
 */
bool stPinchEnd_boundaryIsTrivial(stPinchEnd end);

/*
 * Join a boundary. Ensure that the boundary is actually trivial with
 * stPinchEnd_boundaryIsTrivial before running.
 */
void stPinchEnd_joinTrivialBoundary(stPinchEnd end);

/*
 * Get the pinch ends directly adjacent to this end.
 */
stSet *stPinchEnd_getConnectedPinchEnds(stPinchEnd *end);

/*
 * Size of the set returned by stPinchEnd_getConnectedPinchEnds.
 */
int64_t stPinchEnd_getNumberOfConnectedPinchEnds(stPinchEnd *end);

/*
 * Find out if the end contains a self-loop that doesn't go through
 * otherBlock. If otherBlock is the block of end, just finds out if
 * the end contains any self-loops.
 */
bool stPinchEnd_hasSelfLoopWithRespectToOtherBlock(stPinchEnd *end, stPinchBlock *otherBlock);

/*
 * Get a list of stIntTuples representing the lengths of the
 * (indirect) adjacencies connecting the two ends.
 */
stList *stPinchEnd_getSubSequenceLengthsConnectingEnds(stPinchEnd *end, stPinchEnd *otherEnd);

/*
 * Pinch structure. A pinch represents a gapless alignment between two
 * threads.
 */

/*
 * Replace the data in pinch with the given data.
 */
void stPinch_fillOut(stPinch *pinch, int64_t name1, int64_t name2, int64_t start1, int64_t start2, int64_t length, bool strand);

/*
 * Create a pinch without touching the heap.
 */
stPinch stPinch_constructStatic(int64_t name1, int64_t name2, int64_t start1, int64_t start2, int64_t length, bool strand);

/*
 * Create a pinch.
 */
stPinch *stPinch_construct(int64_t name1, int64_t name2, int64_t start1, int64_t start2, int64_t length, bool strand);

/*
 * Free a pinch.
 */
void stPinch_destruct(stPinch *pinch);

/*
 * Pinch interval structure. A pinch interval is an interval on a
 * thread, labeled by an arbitrary pointer.
 */

/*
 * Replace the data in an existing pinch interval structure.
 */
void stPinchInterval_fillOut(stPinchInterval *pinchInterval, int64_t name, int64_t start, int64_t length, void *label);

/*
 * Create a pinch interval not on the heap.
 */
stPinchInterval stPinchInterval_constructStatic(int64_t name, int64_t start, int64_t length, void *label);

/*
 * Create a pinch interval.
 */
stPinchInterval *stPinchInterval_construct(int64_t name, int64_t start, int64_t length, void *label);

/*
 * Get the thread name for this interval.
 */
int64_t stPinchInterval_getName(stPinchInterval *pinchInterval);

/*
 * Get the start of this interval.
 */
int64_t stPinchInterval_getStart(stPinchInterval *pinchInterval);

/*
 * Get the length of this interval.
 */
int64_t stPinchInterval_getLength(stPinchInterval *pinchInterval);

/*
 * Get the label of this interval.
 */
void *stPinchInterval_getLabel(stPinchInterval *pinchInterval);

/*
 * For using in a sorted set of pinch intervals.
 */
int stPinchInterval_compareFunction(const stPinchInterval *interval1, const stPinchInterval *interval2);

/*
 * Free a pinch interval.
 */
void stPinchInterval_destruct(stPinchInterval *pinchInterval);

/*
 * Create a set of pinch intervals such that each interval's label
 * corresponds to the closest pinch end.
 */
stSortedSet *stPinchThreadSet_getLabelIntervals(stPinchThreadSet *threadSet, stHash *pinchEndsToLabels);

/*
 * Get the interval corresponding to the given thread and position
 * from a set of pinch intervals.
 */
stPinchInterval *stPinchIntervals_getInterval(stSortedSet *pinchIntervals, int64_t name, int64_t position);

/*
 * Functions for undoing pinches. Not for undoing homologies in
 * general (this is impossible in the current code), but for allowing
 * the possiblity of taking back a pinch.
 *
 * Undos can only be applied once (per total pinch region), and must
 * be applied in the reverse of the order that the corresponding
 * pinches were applied in. Any modification of the graph (not
 * including undone pinches, but including kept pinches) between a
 * pinch and its undo invalidates the undo, resulting in undefined
 * behavior.
 */

/*
 * Prepare an undo for a pinch.
 */
stPinchUndo *stPinchThread_prepareUndo(stPinchThread *thread1, stPinchThread *thread2, int64_t start1, int64_t start2, int64_t length, bool strand2);

/*
 * Undo a pinch, restoring all alignment relationships to be the same
 * as before the pinch. Extra trivial boundaries may still exist, and
 * any degree-1 blocks present in the pinched regions may have been
 * undone.
 */
void stPinchThreadSet_undoPinch(stPinchThreadSet *threadSet, stPinchUndo *undo);

/*
 * Partially undo a pinch.
 *
 * NB: Defining a partial undo is a bit hard. Consider an alignment
 * between four bases, A:1, A:2, B:1, and B:2. Suppose there is
 * already an alignment between A:1 and A:2 as well as B:1 and B:2,
 * and we pinch together B:1-2 with A:1-2. All bases become
 * transitively aligned together.
 *
 * The trick comes when we decide to only undo part of this pinch. One
 * possible definition of a "partial undo" is that it is equivalent to
 * having applied a pinch with the undo region "masked" out. But under
 * this definition, undoing either part of the example pinch--or even
 * both halves one after the other--has absolutely no effect on the
 * graph. Presumably the pinched region is being undone for a good
 * reason--the homologies involved are "bad" in some way--so that
 * won't work.
 *
 * Instead of using that definition of a partial undo, this produces a
 * graph where the new pinch involved in the given region is removed,
 * no matter if that happens to affect other parts of the graph
 * outside of the undo region as well. E.g. in the example above,
 * undoing either half of the pinch is equivalent to removing the
 * entire pinch. This definition has the useful property that, for any
 * partitioning of a pinch, undoing all parts is equivalent to undoing
 * the entire pinch.
 */
void stPinchThreadSet_partiallyUndoPinch(stPinchThreadSet *threadSet, stPinchUndo *undo, int64_t offset, int64_t length);

/*
 * Find a potential offset that can be used to undo part of a
 * block. If the block isn't undoable, or has already been completely
 * undone, then returns false. Otherwise, returns true and sets
 * undoOffset and undoLength to the proper values.
 */
bool stPinchUndo_findOffsetForBlock(stPinchUndo *undo, stPinchThreadSet *threadSet,
                                    stPinchBlock *block, int64_t *undoOffset,
                                    int64_t *undoLength);
/*
 * Free an undo.
 */
void stPinchUndo_destruct(stPinchUndo *undo);

#ifdef __cplusplus
}
#endif

#endif /* ST_PINCH_GRAPHS_H_ */
